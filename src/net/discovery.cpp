#include "net/discovery.hpp"

#include <utility>

namespace quintum::net {

PeerDiscovery::PeerDiscovery(
    AddrManager& addrman) noexcept
    : addrman_(addrman)
{
}

DiscoveryConnectResult
PeerDiscovery::connect_one(
    const consensus::ChainParams& params,
    const VersionMessage& local_version,
    std::uint64_t now,
    std::uint32_t timeout_ms,
    std::span<const PeerAddress> excluded)
{
    DiscoveryConnectResult out;

    const auto selected =
        addrman_.select(now, excluded);

    if (!selected) {
        out.error = DiscoveryError::no_candidate;
        return out;
    }

    out.address = *selected;
    addrman_.mark_attempt(*selected, now);

    auto connected =
        connect_and_handshake(
            params,
            format_ipv4(selected->ipv4),
            selected->port,
            local_version,
            timeout_ms
        );

    if (!connected.ok()) {
        addrman_.mark_failure(*selected, now);
        out.peer_error = connected.error;
        out.store_error = addrman_.save();
        out.error = out.store_error ==
                        AddrStoreError::none
                    ? DiscoveryError::peer_failed
                    : DiscoveryError::store_failed;
        return out;
    }

    addrman_.mark_success(*selected, now);
    out.store_error = addrman_.save();

    if (out.store_error != AddrStoreError::none) {
        connected.session->close();
        out.error = DiscoveryError::store_failed;
        return out;
    }

    out.session.emplace(
        std::move(*connected.session)
    );
    return out;
}

} // namespace quintum::net
