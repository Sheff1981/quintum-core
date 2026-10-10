#include "consensus/chainparams.hpp"
#include "consensus/tx_auth.hpp"
#include "net/runtime.hpp"
#include "net/diagnostics.hpp"
#include "wallet/address.hpp"

#include <jni.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace {

std::mutex g_mutex;
std::unique_ptr<quintum::net::NetworkRuntime> g_runtime;

// The diagnostic journal has a separate lifetime and lock. Reading it must
// never wait for native startup, shutdown, or the chain-state mutex.
std::mutex g_diagnostics_mutex;
std::shared_ptr<quintum::net::Diagnostics> g_diagnostics;
std::atomic<bool> g_core_running{false};
std::atomic<std::size_t> g_active_peers{0U};
std::atomic<std::size_t> g_known_peers{0U};

std::shared_ptr<quintum::net::Diagnostics> diagnostics()
{
    std::scoped_lock lock(g_diagnostics_mutex);
    return g_diagnostics;
}

std::shared_ptr<quintum::net::Diagnostics> initialize_diagnostics(
    const std::filesystem::path& root)
{
    std::scoped_lock lock(g_diagnostics_mutex);
    if (!g_diagnostics) {
        g_diagnostics = std::make_shared<quintum::net::Diagnostics>(root / "diagnostics");
    }
    return g_diagnostics;
}

std::atomic<bool> g_mining_cancel{false};
std::atomic<bool> g_mining_running{false};
std::atomic<std::uint64_t> g_mining_attempts{0U};
std::atomic<std::uint64_t> g_mining_found_blocks{0U};
std::atomic<std::uint64_t> g_mining_started_ms{0U};
std::atomic<std::uint32_t> g_mining_threads{0U};
std::thread g_mining_thread;

std::uint64_t steady_millis()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

void stop_mining()
{
    g_mining_cancel.store(true, std::memory_order_relaxed);
    if (g_mining_thread.joinable()) {
        g_mining_thread.join();
    }
    g_mining_running.store(false, std::memory_order_relaxed);
}

} // namespace

extern "C" JNIEXPORT jint JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeStart(
    JNIEnv* env,
    jobject,
    jstring data_directory)
{
    if (data_directory == nullptr) return 1;
    const char* raw = env->GetStringUTFChars(data_directory, nullptr);
    if (raw == nullptr) return 1;
    const std::filesystem::path root{raw};
    env->ReleaseStringUTFChars(data_directory, raw);

    std::shared_ptr<quintum::net::Diagnostics> journal;
    try {
        journal = initialize_diagnostics(root);
        journal->event("core", "info", "starting");
    } catch (...) {
        // Diagnostics must not prevent a node from starting.
    }
    std::scoped_lock lock(g_mutex);
    if (g_runtime) return 2;

    const auto& params = quintum::consensus::chain_params(
        quintum::consensus::Network::randomx_testnet
    );
    auto runtime = std::make_unique<quintum::net::NetworkRuntime>(
        params, root / std::string(params.name)
    );

    quintum::net::NetworkRuntimeConfig config;
    config.enable_nat_mapping = false;
    // Android clients normally sit behind carrier NAT or a VPN. They still
    // make outbound P2P connections, and must not fail startup when the
    // default inbound port is already occupied by another local process.
    config.allow_ephemeral_listener_fallback = true;
    config.wallet_enabled = false;
    config.diagnostics = journal;

    const auto result = runtime->start(std::move(config));
    if (!result.ok()) {
        if (journal) journal->failure("core", static_cast<int>(result.error), "native startup failed");
        runtime->stop();
        return 3;
    }
    g_runtime = std::move(runtime);
    g_core_running.store(true);
    if (journal) journal->event("core", "info", "started");
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeStartMining(
    JNIEnv* env,
    jobject,
    jstring payout_address,
    jint requested_threads)
{
    if (payout_address == nullptr) return 1;

    const char* raw = env->GetStringUTFChars(payout_address, nullptr);
    if (raw == nullptr) return 1;
    const std::string address{raw};
    env->ReleaseStringUTFChars(payout_address, raw);

    std::scoped_lock lock(g_mutex);
    if (!g_runtime || !g_runtime->running()) return 2;
    if (g_mining_running.load(std::memory_order_relaxed)) return 3;
    // A previous mining worker may have exited on its own while its
    // std::thread remains joinable. Reassigning it would call std::terminate.
    if (g_mining_thread.joinable()) g_mining_thread.join();

    const auto& params = quintum::consensus::chain_params(
        quintum::consensus::Network::randomx_testnet
    );
    const auto decoded = quintum::wallet::decode_address(params.network, address);
    if (!decoded.ok()) return 4;

    const quintum::Bytes payout_script =
        quintum::consensus::make_p2pk_locking_script(decoded.public_key);

    const auto hardware = std::max(1U, std::thread::hardware_concurrency());
    const auto threads = static_cast<std::uint32_t>(
        std::clamp<int>(requested_threads, 1, static_cast<int>(hardware))
    );

    g_mining_cancel.store(false, std::memory_order_relaxed);
    g_mining_attempts.store(0U, std::memory_order_relaxed);
    g_mining_found_blocks.store(0U, std::memory_order_relaxed);
    g_mining_threads.store(threads, std::memory_order_relaxed);
    g_mining_started_ms.store(steady_millis(), std::memory_order_relaxed);
    g_mining_running.store(true, std::memory_order_relaxed);

    auto* runtime = g_runtime.get();
    g_mining_thread = std::thread([runtime, payout_script, threads] {
        constexpr std::uint64_t kBatchAttempts = 4096U;
        while (!g_mining_cancel.load(std::memory_order_relaxed)) {
            const auto result = runtime->mine_mempool_block_parallel(
                payout_script,
                kBatchAttempts,
                threads,
                false,
                &g_mining_cancel
            );
            g_mining_attempts.fetch_add(
                result.mining.attempts,
                std::memory_order_relaxed
            );
            if (result.ok()) {
                g_mining_found_blocks.fetch_add(1U, std::memory_order_relaxed);
                continue;
            }
            if (g_mining_cancel.load(std::memory_order_relaxed)) break;
            if (result.error != quintum::NodeMineError::proof_of_work_exhausted) break;
        }
        g_mining_running.store(false, std::memory_order_relaxed);
    });

    return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeStopMining(JNIEnv*, jobject)
{
    stop_mining();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeMiningRunning(JNIEnv*, jobject)
{
    return g_mining_running.load(std::memory_order_relaxed) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeMiningAttempts(JNIEnv*, jobject)
{
    return static_cast<jlong>(g_mining_attempts.load(std::memory_order_relaxed));
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeMiningFoundBlocks(JNIEnv*, jobject)
{
    return static_cast<jlong>(g_mining_found_blocks.load(std::memory_order_relaxed));
}

extern "C" JNIEXPORT jint JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeMiningThreads(JNIEnv*, jobject)
{
    return static_cast<jint>(g_mining_threads.load(std::memory_order_relaxed));
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeMiningRuntimeMillis(JNIEnv*, jobject)
{
    const auto started = g_mining_started_ms.load(std::memory_order_relaxed);
    if (started == 0U) return 0;
    return static_cast<jlong>(steady_millis() - started);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeBlockHeight(JNIEnv*, jobject)
{
    std::scoped_lock lock(g_mutex);
    if (!g_runtime) return -1;
    const auto value = g_runtime->status_nonblocking().height;
    return value ? static_cast<jlong>(*value) : -1;
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeKnownAddressCount(JNIEnv*, jobject)
{
    std::scoped_lock lock(g_mutex);
    if (!g_runtime) return 0;
    return static_cast<jlong>(g_runtime->status_nonblocking().known_addresses);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_quintum_wallet_core_NativeCore_nativePeerCount(JNIEnv*, jobject)
{
    std::scoped_lock lock(g_mutex);
    if (!g_runtime) return 0;
    return static_cast<jlong>(g_runtime->status_nonblocking().peers);
}

extern "C" JNIEXPORT jdouble JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeDifficulty(JNIEnv*, jobject)
{
    std::scoped_lock lock(g_mutex);
    if (!g_runtime) return -1.0;
    const auto value = g_runtime->status_nonblocking().difficulty;
    return value ? static_cast<jdouble>(*value) : -1.0;
}

extern "C" JNIEXPORT jdouble JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeMiningHashRate(JNIEnv*, jobject)
{
    // Do not report a stale lifetime-average hash rate after the worker stops.
    // The mining UI uses this value to indicate active CPU work.
    if (!g_mining_running.load(std::memory_order_relaxed)) return 0.0;
    const auto started = g_mining_started_ms.load(std::memory_order_relaxed);
    if (started == 0U) return 0.0;
    const auto elapsed = steady_millis() - started;
    if (elapsed == 0U) return 0.0;
    const auto attempts = g_mining_attempts.load(std::memory_order_relaxed);
    return static_cast<jdouble>(attempts) * 1000.0 / static_cast<jdouble>(elapsed);
}

extern "C" JNIEXPORT void JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeStop(JNIEnv*, jobject)
{
    if (const auto journal = diagnostics()) journal->stage("core_stopping", "core");
    stop_mining();
    std::scoped_lock lock(g_mutex);
    if (g_runtime) {
        g_runtime->stop();
        g_runtime.reset();
    }
    g_core_running.store(false);
    g_active_peers.store(0U);
    g_known_peers.store(0U);
    if (const auto journal = diagnostics()) journal->event("core", "info", "stopped");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeRunning(JNIEnv*, jobject)
{
    std::scoped_lock lock(g_mutex);
    return g_runtime && g_runtime->running() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeP2pDiagnostic(JNIEnv*, jobject)
{
    std::scoped_lock lock(g_mutex);
    if (!g_runtime) return -1;
    return static_cast<jint>(g_runtime->android_p2p_diagnostic());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_quintum_wallet_core_NativeCore_nativePeerDetails(JNIEnv* env, jobject)
{
    std::scoped_lock lock(g_mutex);
    if (!g_runtime)
        return env->NewStringUTF("Native core unavailable");
    const auto details = g_runtime->android_peer_details();
    return env->NewStringUTF(details.empty() ? "Peer details unavailable" : details.c_str());
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeConnectElapsedMs(JNIEnv*, jobject)
{
    std::scoped_lock lock(g_mutex);
    if (!g_runtime) return 0;
    return static_cast<jlong>(g_runtime->android_connect_elapsed_ms());
}

extern "C" JNIEXPORT jint JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeLastConnectError(JNIEnv*, jobject)
{
    std::scoped_lock lock(g_mutex);
    return g_runtime ? static_cast<jint>(g_runtime->android_last_connect_error()) : 0;
}
extern "C" JNIEXPORT jlong JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeConnectAttempts(JNIEnv*, jobject)
{
    std::scoped_lock lock(g_mutex);
    return g_runtime ? static_cast<jlong>(g_runtime->android_connect_attempts()) : 0;
}


extern "C" JNIEXPORT void JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeInitializeDiagnostics(
    JNIEnv* env, jobject, jstring directory)
{
    if (directory == nullptr) return;
    const char* raw = env->GetStringUTFChars(directory, nullptr);
    if (raw == nullptr) return;
    const std::filesystem::path root{raw};
    env->ReleaseStringUTFChars(directory, raw);
    try { (void)initialize_diagnostics(root); } catch (...) { }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeDiagnostics(JNIEnv* env, jobject)
{
    try {
        const auto journal = diagnostics();

        // Try, never wait: startup/shutdown owns g_mutex for longer than a UI
        // polling interval. The journal and cached counters remain readable.
        std::unique_lock lock(g_mutex, std::try_to_lock);
        if (lock.owns_lock() && g_runtime) {
            const auto status = g_runtime->status_nonblocking();
            g_core_running.store(status.running);
            g_active_peers.store(status.peers);
            g_known_peers.store(status.known_addresses);
        }
        if (journal) {
            journal->gauge("core_running", g_core_running.load() ? 1U : 0U);
            journal->gauge("active_peers", g_active_peers.load());
            journal->gauge("known_peers", g_known_peers.load());
        }
        const auto snapshot = journal ? journal->snapshot_json() : std::string{"{}"};
        return env->NewStringUTF(snapshot.c_str());
    } catch (...) {
        return env->NewStringUTF("{\"log_error\":\"native diagnostics unavailable\"}");
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeDiagnosticLog(JNIEnv* env, jobject)
{
    try {
        const auto journal = diagnostics();
        const auto log = journal ? journal->export_log() : std::string{};
        return env->NewStringUTF(log.c_str());
    } catch (...) { return env->NewStringUTF(""); }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeClearDiagnosticLog(JNIEnv*, jobject)
{
    const auto journal = diagnostics();
    return journal && journal->clear_log() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeServiceEvent(
    JNIEnv* env, jobject, jstring event)
{
    if (event == nullptr) return;
    const char* raw = env->GetStringUTFChars(event, nullptr);
    if (raw == nullptr) return;
    const std::string name{raw};
    env->ReleaseStringUTFChars(event, raw);
    // No arbitrary lifecycle exception text, extras, addresses or credentials.
    constexpr std::string_view allowed[] = {
        "created", "start_request", "core_started", "core_already_running",
        "start_failed", "destroyed", "timeout", "task_removed",
        "activity_foreground", "activity_background"
    };
    if (std::find(std::begin(allowed), std::end(allowed), name) == std::end(allowed)) return;
    if (const auto journal = diagnostics()) {
        const bool failed = name == "start_failed" || name == "timeout";
        journal->event("android", failed ? "warning" : "info", name);
    }
}
