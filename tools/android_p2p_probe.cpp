#include "consensus/chainparams.hpp"
#include "net/peer.hpp"
#include "net/sync.hpp"

#include <chrono>
#include <iostream>
#include <random>

// Read-only live-seed diagnostic. No NodeRuntime, wallet, mining or data files.
int main(int argc, char** argv)
{
    using namespace quintum;
    using namespace quintum::net;
    if (argc != 2) {
        std::cerr << "Usage: quintum_android_p2p_probe HOST\n";
        return 2;
    }
    const auto& params = consensus::randomx_testnet_params();
    std::random_device random;
    const auto nonce = (static_cast<std::uint64_t>(random()) << 32U) |
        static_cast<std::uint64_t>(random());
    const auto now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    const VersionMessage local{
        .protocol_version = params.p2p_protocol_version,
        .services = kServiceNetwork | kServiceCompactBlocks |
            kServiceEncryptedTransport | kServiceAddrV2 | kServiceChainWork |
            kServiceDandelionRelay,
        .timestamp = now,
        .nonce = nonce == 0U ? 1U : nonce,
        .start_height = 0U,
        .listen_port = 0U,
    };
    const auto started = std::chrono::steady_clock::now();
    auto result = connect_and_handshake(params, argv[1], params.p2p_port, local, 5'000U);
    std::cout << "endpoint=" << argv[1] << ':' << params.p2p_port
              << " handshake_ok=" << result.ok()
              << " phase=" << result.phase
              << " peer_error=" << static_cast<int>(result.error)
              << " wire_error=" << static_cast<int>(result.wire_error)
              << " socket_error=" << result.socket_error
              << " remote_closed=" << result.remote_closed
              << " partial_io_bytes=" << result.partial_io_bytes
              << " elapsed_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - started).count() << std::endl;
    if (!result.ok()) return 1;
    auto& peer = *result.session;
    std::cout << "protocol=" << peer.remote_version().protocol_version
              << " remote_height=" << peer.remote_version().start_height
              << " services=" << peer.remote_version().services
              << " encrypted=" << peer.encrypted() << std::endl;
    if ((peer.remote_version().services & kServiceChainWork) != 0U) {
        auto error = peer.send_command("chainwork", serialize_chain_work(Hash256{}));
        WireMessage response;
        if (error == PeerError::none) error = peer.receive_command(response);
        const bool valid = error == PeerError::none && response.command == "chainwork" &&
            parse_chain_work(response.payload).has_value();
        std::cout << "phase=chainwork success=" << valid
                  << " peer_error=" << static_cast<int>(error) << std::endl;
        if (!valid) return 1;
    }
    const GetHeadersRequest request{.locator = {params.genesis.hash}, .stop = {}};
    auto error = peer.send_command("getheaders", serialize_getheaders(request));
    WireMessage response;
    if (error == PeerError::none) error = peer.receive_command(response);
    const auto headers = response.command == "headers" ? parse_headers(response.payload) :
        std::optional<std::vector<BlockHeader>>{};
    std::cout << "phase=headers success=" << (error == PeerError::none && headers.has_value())
              << " command=" << response.command
              << " peer_error=" << static_cast<int>(error)
              << " count=" << (headers ? headers->size() : 0U) << std::endl;
    // Header parsing is a connectivity check, not consensus validation or IBD.
    return error == PeerError::none && headers.has_value() ? 0 : 1;
}
