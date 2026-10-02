#pragma once

#include "net/address.hpp"
#include "net/peer.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace quintum::net {

enum class DiscoveryError {
    none,
    no_candidate,
    peer_failed,
    store_failed,
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

    [[nodiscard]] DiscoveryConnectResult connect_one(
        const consensus::ChainParams& params,
        const VersionMessage& local_version,
        std::uint64_t now,
        std::uint32_t timeout_ms,
        std::span<const PeerAddress> excluded = {}
    );

private:
    AddrManager& addrman_;
};

} // namespace quintum::net
