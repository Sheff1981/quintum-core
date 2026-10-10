#include "consensus/chainparams.hpp"
#include "net/diagnostics.hpp"
#include "net/runtime.hpp"
#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

// CI-only live-network validation. Uses a disposable node-only data directory,
// no RPC server, no wallet, no mining, and the unchanged production validator.
int main(int argc, char** argv)
{
    using namespace quintum;
    using namespace quintum::net;
    if (argc != 3) {
        std::cerr << "Usage: quintum_android_sync_probe VPS_IPV4 EMPTY_DATA_DIRECTORY\n";
        return 2;
    }
    const auto address = parse_ipv4(argv[1]);
    const std::filesystem::path root{argv[2]};
    if (!address || std::filesystem::exists(root)) {
        std::cerr << "A valid IPv4 and a new disposable directory are required\n";
        return 2;
    }
    const auto& params = consensus::randomx_testnet_params();
    auto diagnostics = std::make_shared<Diagnostics>(root / "diagnostics");
    NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.wallet_enabled = false;
    config.target_outbound = 1U;
    config.enable_nat_mapping = false;
    config.diagnostics = diagnostics;
    config.bootstrap_peers = {PeerAddress{
        .ipv4 = *address, .port = params.p2p_port,
        .services = kServiceNetwork,
    }};
    NetworkRuntime runtime{params, root / "chain"};
    if (!runtime.start(config).ok()) return 1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(18);
    bool complete = false;
    std::uint32_t height = 0U;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto text = diagnostics->snapshot_json();
        std::cout << text << std::endl;
        const auto snapshot = nlohmann::json::parse(text);
        if (snapshot.value("stage", "") == "fully_synchronized") {
            const auto local = snapshot.value("local_height", 0U);
            const auto remote = snapshot.value("remote_height", 0U);
            const auto accepted = snapshot.value("blocks_accepted", 0U);
            if (remote > 0U && local >= remote && accepted > 0U) {
                complete = true;
                height = local;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    // External workflow timeout also bounds a validator still finishing a
    // batch at shutdown. The journal is flushed after every event.
    runtime.stop();
    std::ofstream(root / "diagnostic-export.jsonl") << diagnostics->export_log();
    if (!complete) {
        std::cerr << "Live sync did not complete within the probe budget\n";
        return 1;
    }
    NetworkRuntime restarted{params, root / "chain"};
    config.target_outbound = 0U;
    const auto start = restarted.start(config);
    const auto restored = start.ok() ? restarted.status().height : std::nullopt;
    restarted.stop();
    const bool persistent = restored && *restored == height;
    const bool wallet_absent = !std::filesystem::exists(root / "chain" / "wallet.dat")
        && !std::filesystem::exists(root / "chain" / "wallet_state.dat");
    std::cout << "validated_height=" << height << " restored_height="
        << (restored ? std::to_string(*restored) : "unavailable")
        << " restart_persistent=" << persistent
        << " wallet_absent=" << wallet_absent << std::endl;
    return persistent && wallet_absent ? 0 : 1;
}
