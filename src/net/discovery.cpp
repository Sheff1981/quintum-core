#include "net/discovery.hpp"

#include <utility>

namespace quintum::net {

PeerDiscovery::PeerDiscovery(
    AddrManager& addrman) noexcept
    : addrman_(addrman)
{
}

AddrStoreError PeerDiscovery::initialize(
    consensus::Network network,
    std::uint64_t now)
{
    const auto loaded = addrman_.load();

    if (loaded == AddrStoreError::none) {
        return AddrStoreError::none;
    }

    if (loaded != AddrStoreError::not_found) {
        return loaded;
    }

    const auto added =
        bootstrap_hardcoded(network, now);

    if (added == 0U) {
        return AddrStoreError::none;
    }

    return addrman_.save();
}

std::size_t PeerDiscovery::bootstrap_seeds(
    std::span<const SeedEndpoint> seeds,
    std::uint64_t now)
{
    std::size_t added{0U};

    for (const auto& seed : seeds) {
        const auto ipv4 = parse_ipv4(seed.host);
        if (!ipv4 || seed.port == 0U) {
            continue;
        }

        PeerAddress address{
            .ipv4 = *ipv4,
            .port = seed.port,
            .services = 1U,
            .last_seen = now,
        };

        if (addrman_.add(address)) {
            ++added;
        }
    }

    return added;
}

std::size_t PeerDiscovery::bootstrap_hardcoded(
    consensus::Network network,
    std::uint64_t now)
{
    return bootstrap_seeds(
        hardcoded_seeds(network),
        now
    );
}

DiscoveryLearnResult
PeerDiscovery::learn_from_peer(
    PeerSession& peer,
    bool allow_local)
{
    DiscoveryLearnResult out;
    std::vector<PeerAddress> learned;

    out.peer_error =
        peer.request_addresses(
            allow_local,
            learned
        );

    if (out.peer_error != PeerError::none) {
        out.error = DiscoveryError::peer_failed;
        return out;
    }

    out.received = learned.size();
    out.added = addrman_.add(learned);
    out.store_error = addrman_.save();

    if (out.store_error != AddrStoreError::none) {
        out.error = DiscoveryError::store_failed;
    }

    return out;
}

DiscoveryConnectResult
PeerDiscovery::connect_any(
    const consensus::ChainParams& params,
    const VersionMessage& local_version,
    std::uint64_t now,
    std::uint32_t timeout_ms,
    std::size_t max_candidates,
    std::span<const PeerAddress> excluded)
{
    DiscoveryConnectResult last;

    if (max_candidates == 0U) {
        last.error = DiscoveryError::no_candidate;
        return last;
    }

    for (std::size_t i = 0U;
         i < max_candidates;
         ++i) {
        auto current = connect_one(
            params,
            local_version,
            now,
            timeout_ms,
            excluded
        );

        if (current.ok()) {
            return current;
        }

        last = std::move(current);

        if (last.error ==
                DiscoveryError::no_candidate ||
            last.error ==
                DiscoveryError::store_failed) {
            return last;
        }
    }

    return last;
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
