#pragma once

#include "consensus/chainparams.hpp"
#include "core/serialize.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace quintum::net {

inline constexpr std::size_t kMaxAddrMessageEntries = 1'000U;
inline constexpr std::size_t kMaxAddrManagerEntries = 50'000U;
inline constexpr std::size_t kMaxAddrEntriesPerIpv4Group = 64U;

enum class AddressNetwork : std::uint8_t {
    ipv4 = 0x01U,
    tor_v3 = 0x04U,
    i2p = 0x05U,
};

struct PeerAddress {
    std::uint32_t ipv4{0U};
    AddressNetwork network{AddressNetwork::ipv4};
    std::string host{};
    std::uint16_t port{0U};
    std::uint64_t services{0U};
    std::uint64_t last_seen{0U};

    [[nodiscard]] bool operator==(
        const PeerAddress&) const noexcept = default;
};

[[nodiscard]] std::optional<std::uint32_t> parse_ipv4(
    std::string_view text
) noexcept;

[[nodiscard]] std::string format_ipv4(
    std::uint32_t address
);

[[nodiscard]] std::string format_peer_host(
    const PeerAddress& address
);

[[nodiscard]] bool is_overlay_address(
    const PeerAddress& address
) noexcept;

[[nodiscard]] bool valid_peer_address(
    const PeerAddress& address,
    bool allow_local
) noexcept;

[[nodiscard]] Bytes serialize_addresses(
    std::span<const PeerAddress> addresses
);

[[nodiscard]] std::optional<std::vector<PeerAddress>>
parse_addresses(
    std::span<const Byte> payload,
    bool allow_local
);

[[nodiscard]] Bytes serialize_addresses_v2(
    std::span<const PeerAddress> addresses
);

[[nodiscard]] std::optional<std::vector<PeerAddress>>
parse_addresses_v2(
    std::span<const Byte> payload,
    bool allow_local
);

enum class AddrStoreError {
    none,
    not_found,
    io_error,
    corrupt,
    wrong_network,
};

struct AddrInfo {
    PeerAddress address{};
    std::uint64_t last_attempt{0U};
    std::uint64_t last_success{0U};
    std::uint64_t next_attempt{0U};
    std::uint32_t failures{0U};
};

class AddrManager {
public:
    AddrManager(
        const consensus::ChainParams& params,
        std::filesystem::path directory,
        bool allow_local = false
    );

    void set_allow_local(bool allow_local) noexcept;
    [[nodiscard]] AddrStoreError load();
    [[nodiscard]] AddrStoreError save() const;

    [[nodiscard]] bool add(const PeerAddress& address);
    [[nodiscard]] std::size_t add(
        std::span<const PeerAddress> addresses
    );

    void mark_attempt(
        const PeerAddress& address,
        std::uint64_t now
    );
    void mark_success(
        const PeerAddress& address,
        std::uint64_t now
    );
    void mark_failure(
        const PeerAddress& address,
        std::uint64_t now
    );

    // A persisted retry deadline may be hours in the future after repeated
    // failures. Bootstrap endpoints must get one fresh startup opportunity
    // without discarding their failure history.
    void make_retry_eligible(
        const PeerAddress& address,
        std::uint64_t now
    );

    [[nodiscard]] std::optional<PeerAddress> select(
        std::uint64_t now,
        std::span<const PeerAddress> excluded = {},
        bool allow_tor = true,
        bool allow_i2p = true
    ) const;

    [[nodiscard]] std::vector<PeerAddress> addresses(
        std::size_t limit = kMaxAddrMessageEntries
    ) const;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const std::vector<AddrInfo>& entries() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;

private:
    [[nodiscard]] AddrInfo* find(
        const PeerAddress& address
    ) noexcept;
    [[nodiscard]] const AddrInfo* find(
        const PeerAddress& address
    ) const noexcept;

    consensus::ChainParams params_{};
    std::filesystem::path directory_{};
    std::filesystem::path path_{};
    bool allow_local_{false};
    std::vector<AddrInfo> entries_{};
};

struct SeedEndpoint {
    std::string_view host{};
    std::uint16_t port{0U};
};

[[nodiscard]] std::span<const SeedEndpoint>
hardcoded_seeds(
    consensus::Network network
) noexcept;

[[nodiscard]] std::span<const SeedEndpoint>
dns_seeds(
    consensus::Network network
) noexcept;

} // namespace quintum::net
