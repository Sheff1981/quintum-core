#include "consensus/chainparams.hpp"
#include "net/runtime.hpp"

#include <jni.h>

#include <filesystem>
#include <memory>
#include <mutex>

namespace {

std::mutex g_mutex;
std::unique_ptr<quintum::net::NetworkRuntime> g_runtime;

} // namespace

extern "C" JNIEXPORT jint JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeStart(
    JNIEnv* env,
    jobject,
    jstring data_directory)
{
    if (data_directory == nullptr) {
        return 1;
    }

    const char* raw =
        env->GetStringUTFChars(data_directory, nullptr);

    if (raw == nullptr) {
        return 1;
    }

    const std::filesystem::path root{raw};
    env->ReleaseStringUTFChars(data_directory, raw);

    std::scoped_lock lock(g_mutex);

    if (g_runtime) {
        return 2;
    }

    const auto& params = quintum::consensus::chain_params(
        quintum::consensus::Network::randomx_testnet
    );

    auto runtime =
        std::make_unique<quintum::net::NetworkRuntime>(
            params,
            root / std::string(params.name)
        );

    quintum::net::NetworkRuntimeConfig config;
    config.enable_nat_mapping = false;
    // Wallet creation/recovery belongs to explicit Android onboarding.
    // Never create an unprotected wallet merely because the node service starts.
    config.wallet_enabled = false;

    const auto result = runtime->start(std::move(config));

    if (!result.ok()) {
        runtime->stop();
        return 3;
    }

    g_runtime = std::move(runtime);
    return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeStop(
    JNIEnv*,
    jobject)
{
    std::scoped_lock lock(g_mutex);

    if (g_runtime) {
        g_runtime->stop();
        g_runtime.reset();
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_quintum_wallet_core_NativeCore_nativeRunning(
    JNIEnv*,
    jobject)
{
    std::scoped_lock lock(g_mutex);
    return g_runtime && g_runtime->running()
        ? JNI_TRUE
        : JNI_FALSE;
}
