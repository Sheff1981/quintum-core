#include "consensus/chainparams.hpp"
#include "net/peer.hpp"
#include "net/sync.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <set>
#include <string>

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
    // P2P v2 requires a nonzero advertised listening port. Use a real
    // ephemeral listener as the runtime does, rather than sending an invalid
    // version that the remote parser rejects before replying.
    PeerListener listener(params);
    const auto listen_error = listener.listen("127.0.0.1", 0U);
    if (listen_error != PeerError::none) {
        std::cerr << "phase=local-listener peer_error="
                  << static_cast<int>(listen_error) << std::endl;
        return 2;
    }
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
        .listen_port = listener.local_port(),
    };
    if (!parse_version(serialize_version(local))) {
        std::cerr << "Invalid local version; refusing a misleading live probe" << std::endl;
        return 2;
    }
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
    // Probe peer discovery before header sync, matching Android initial setup.
    // This also distinguishes an unresponsive address exchange from a
    // slow or broken getheaders response on the live seed.
    {
        const auto started_discovery = std::chrono::steady_clock::now();
        std::vector<PeerAddress> learned;
        const auto discovery_error = peer.request_addresses(false, learned);
        std::cout << "phase=discovery success=" << (discovery_error == PeerError::none)
                  << " peer_error=" << static_cast<int>(discovery_error)
                  << " count=" << learned.size()
                  << " elapsed_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - started_discovery).count()
                  << std::endl;
        if (discovery_error != PeerError::none) return 1;
        // Aggregate only: avoid publishing third-party peer IP addresses in CI logs.
        std::set<std::string> unique_hosts;
        std::set<std::string> unique_endpoints;
        std::set<std::uint16_t> unique_ports;
        std::size_t default_port = 0U;
        std::size_t stale = 0U;
        std::size_t future = 0U;
        std::size_t ipv4 = 0U;
        std::size_t ipv6 = 0U;
        std::size_t other = 0U;
        constexpr std::uint64_t kThirtyDays = 30U * 24U * 60U * 60U;
        for (const auto& address : learned) {
            const std::string host = std::to_string(static_cast<int>(address.network)) +
                ":" + std::string(reinterpret_cast<const char*>(address.bytes.data()),
                                  address.bytes.size());
            unique_hosts.insert(host);
            unique_endpoints.insert(host + ":" + std::to_string(address.port));
            unique_ports.insert(address.port);
            default_port += address.port == params.p2p_port ? 1U : 0U;
            stale += address.last_seen < now && now - address.last_seen > kThirtyDays ? 1U : 0U;
            future += address.last_seen > now + 24U * 60U * 60U ? 1U : 0U;
            if (address.network == AddressNetwork::ipv4) ++ipv4;
            else if (address.network == AddressNetwork::ipv6) ++ipv6;
            else ++other;
        }
        std::cout << "phase=address-audit entries=" << learned.size()
                  << " unique_hosts=" << unique_hosts.size()
                  << " unique_endpoints=" << unique_endpoints.size()
                  << " unique_ports=" << unique_ports.size()
                  << " default_port=" << default_port
                  << " stale_over_30d=" << stale
                  << " future_over_1d=" << future
                  << " ipv4=" << ipv4 << " ipv6=" << ipv6
                  << " other=" << other << std::endl;
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
