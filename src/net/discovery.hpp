#pragma once

#include "net/address.hpp"
#include "net/peer.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace quintum::net {

struct ProxyRoutes {
    std::optional<Socks5Proxy> tor{};
    std::optional<Socks5Proxy> i2p{};
};

[[nodiscard]] PeerHandshakeResult connect_peer_address(
    const consensus::ChainParams& params,
    const PeerAddress& address,
    const VersionMessage& local_version,
    std::uint32_t timeout_ms,
    const ProxyRoutes& routes
);

enum class DiscoveryError {
    none,
    no_candidate,
    peer_failed,
    store_failed,
};

struct DiscoveryLearnResult {
    DiscoveryError error{DiscoveryError::none};
    PeerError peer_error{PeerError::none};
    AddrStoreError store_error{AddrStoreError::none};
    std::size_t received{0U};
    std::size_t added{0U};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == DiscoveryError::none;
    }
};

struct DiscoveryConnectResult {
    DiscoveryError error{DiscoveryError::none};
    PeerError peer_error{PeerError::none};
    AddrStoreError store_error{AddrStoreError::none};
    std::optional<PeerAddress> address{};
    std::optional<PeerSession> session{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == DiscoveryError::none &&
               session.has_value();
    }
};

class PeerDiscovery {
public:
    explicit PeerDiscovery(AddrManager& addrman) noexcept;

    [[nodiscard]] AddrStoreError initialize(
        consensus::Network network,
        std::uint64_t now
    );

    [[nodiscard]] std::size_t bootstrap_seeds(
        std::span<const SeedEndpoint> seeds,
        std::uint64_t now
    );

    [[nodiscard]] std::size_t bootstrap_hardcoded(
        consensus::Network network,
        std::uint64_t now
    );

    [[nodiscard]] std::size_t bootstrap_dns_seeds(
        std::span<const SeedEndpoint> seeds,
        std::uint64_t now
    );

    [[nodiscard]] DiscoveryLearnResult learn_from_peer(
        PeerSession& peer,
        bool allow_local
    );

    [[nodiscard]] DiscoveryConnectResult connect_any(
        const consensus::ChainParams& params,
        const VersionMessage& local_version,
        std::uint64_t now,
        std::uint32_t timeout_ms,
        std::size_t max_candidates,
        std::span<const PeerAddress> excluded = {},
        ProxyRoutes routes = {}
    );

    [[nodiscard]] DiscoveryConnectResult connect_one(
        const consensus::ChainParams& params,
        const VersionMessage& local_version,
        std::uint64_t now,
        std::uint32_t timeout_ms,
        std::span<const PeerAddress> excluded = {},
        ProxyRoutes routes = {}
    );

private:
    AddrManager& addrman_;
};

} // namespace quintum::net
