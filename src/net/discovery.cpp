#include "net/discovery.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

namespace quintum::net {
namespace {

bool dns_runtime_ready() noexcept
{
#ifdef _WIN32
    struct Runtime {
        bool ok{false};

        Runtime() noexcept
        {
            WSADATA data{};
            ok = WSAStartup(
                MAKEWORD(2, 2),
                &data
            ) == 0;
        }

        ~Runtime()
        {
            if (ok) {
                WSACleanup();
            }
        }
    };

    static Runtime runtime;
    return runtime.ok;
#else
    return true;
#endif
}

std::vector<std::uint32_t> resolve_ipv4_addresses(
    std::string_view host)
{
    std::vector<std::uint32_t> out;

    if (host.empty() || !dns_runtime_ready()) {
        return out;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* resolved{nullptr};
    const std::string name{host};

    if (getaddrinfo(
            name.c_str(),
            nullptr,
            &hints,
            &resolved) != 0) {
        return out;
    }

    for (addrinfo* item = resolved;
         item != nullptr;
         item = item->ai_next) {
        if (item->ai_family != AF_INET ||
            item->ai_addr == nullptr ||
            item->ai_addrlen <
                sizeof(sockaddr_in)) {
            continue;
        }

        const auto* ipv4 =
            reinterpret_cast<const sockaddr_in*>(
                item->ai_addr
            );

        const std::uint32_t value =
            ntohl(ipv4->sin_addr.s_addr);

        if (std::find(
                out.begin(),
                out.end(),
                value) == out.end()) {
            out.push_back(value);
        }
    }

    freeaddrinfo(resolved);
    return out;
}

} // namespace

PeerHandshakeResult connect_peer_address(
    const consensus::ChainParams& params,
    const PeerAddress& address,
    const VersionMessage& local_version,
    std::uint32_t timeout_ms,
    const ProxyRoutes& routes)
{
    const std::string host =
        format_peer_host(address);

    if (address.network ==
        AddressNetwork::tor_v3) {
        if (!routes.tor) {
            PeerHandshakeResult out;
            out.error =
                PeerError::proxy_negotiation_failed;
            return out;
        }

        return connect_and_handshake(
            params,
            host,
            address.port,
            local_version,
            timeout_ms,
            *routes.tor
        );
    }

    if (address.network ==
        AddressNetwork::i2p) {
        if (!routes.i2p) {
            PeerHandshakeResult out;
            out.error =
                PeerError::proxy_negotiation_failed;
            return out;
        }

        return connect_and_handshake(
            params,
            host,
            address.port,
            local_version,
            timeout_ms,
            *routes.i2p
        );
    }

    return connect_and_handshake(
        params,
        host,
        address.port,
        local_version,
        timeout_ms
    );
}

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

    if (loaded != AddrStoreError::none &&
        loaded != AddrStoreError::not_found) {
        return loaded;
    }

    const auto dns = dns_seeds(network);
    const auto hardcoded =
        hardcoded_seeds(network);

    std::size_t added{0U};

    added += bootstrap_dns_seeds(
        dns,
        now
    );
    added += bootstrap_hardcoded(
        network,
        now
    );

    // peers.dat intentionally persists retry/backoff state, but a bootstrap
    // endpoint must not stay suppressed for hours across an application
    // restart. bootstrap_seeds() makes existing fixed seeds eligible for one
    // immediate startup attempt without erasing their failure counters.
    if (added == 0U &&
        dns.empty() &&
        hardcoded.empty()) {
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

        const bool inserted =
            addrman_.add(address);

        // A persisted seed may have a long exponential-backoff deadline.
        // Give it one startup retry while preserving its failure count.
        addrman_.make_retry_eligible(
            address,
            now
        );

        if (inserted) {
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

std::size_t PeerDiscovery::bootstrap_dns_seeds(
    std::span<const SeedEndpoint> seeds,
    std::uint64_t now)
{
    std::size_t added{0U};

    for (const auto& seed : seeds) {
        if (seed.host.empty() ||
            seed.port == 0U) {
            continue;
        }

        for (const auto ipv4 :
             resolve_ipv4_addresses(seed.host)) {
            const PeerAddress address{
                .ipv4 = ipv4,
                .port = seed.port,
                .services = 1U,
                .last_seen = now,
            };

            const bool inserted = addrman_.add(address);
            // DNS seeds must receive the same startup retry opportunity
            // as fixed seeds, even when their addresses already exist
            // in peers.dat with an expired or long backoff schedule.
            addrman_.make_retry_eligible(address, now);
            if (inserted) {
                ++added;
            }
        }
    }

    return added;
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
    std::span<const PeerAddress> excluded,
    ProxyRoutes routes)
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
            excluded,
            routes
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
    std::span<const PeerAddress> excluded,
    ProxyRoutes routes)
{
    DiscoveryConnectResult out;

    const auto selected =
        addrman_.select(
            now,
            excluded,
            routes.tor.has_value(),
            routes.i2p.has_value()
        );

    if (!selected) {
        out.error = DiscoveryError::no_candidate;
        return out;
    }

    out.address = *selected;
    addrman_.mark_attempt(*selected, now);

    auto connected =
        connect_peer_address(
            params,
            *selected,
            local_version,
            timeout_ms,
            routes
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
