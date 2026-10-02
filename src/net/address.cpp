#include "net/address.hpp"

#include "crypto/sha256.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace quintum::net {
namespace {

constexpr std::array<Byte, 8> kPeerMagic{
    'Q', 'P', 'E', 'E', 'R', 'S', '1', 0
};
constexpr std::uint32_t kPeerStoreVersion = 1U;
constexpr std::size_t kChecksumSize = 32U;
constexpr std::size_t kPeerRecordSize =
    4U + 2U + 8U + 8U + 8U + 8U + 4U;

bool same_endpoint(
    const PeerAddress& a,
    const PeerAddress& b) noexcept
{
    return a.ipv4 == b.ipv4 &&
           a.port == b.port;
}

bool excluded_endpoint(
    const PeerAddress& value,
    std::span<const PeerAddress> excluded) noexcept
{
    return std::any_of(
        excluded.begin(),
        excluded.end(),
        [&](const PeerAddress& item) {
            return same_endpoint(value, item);
        }
    );
}

std::uint64_t failure_delay(
    std::uint32_t failures) noexcept
{
    constexpr std::uint64_t base{60U};
    constexpr std::uint64_t maximum{86'400U};

    if (failures == 0U) {
        return base;
    }

    const std::uint32_t shift =
        std::min<std::uint32_t>(failures - 1U, 10U);

    return std::min<std::uint64_t>(
        base << shift,
        maximum
    );
}

bool flush_file(std::FILE* file) noexcept
{
    if (std::fflush(file) != 0) {
        return false;
    }
#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#else
    return ::fsync(fileno(file)) == 0;
#endif
}

bool replace_file(
    const std::filesystem::path& source,
    const std::filesystem::path& destination)
{
#ifdef _WIN32
    return MoveFileExW(
               source.c_str(),
               destination.c_str(),
               MOVEFILE_REPLACE_EXISTING |
                   MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (::rename(
            source.c_str(),
            destination.c_str()) != 0) {
        return false;
    }

    const auto directory = destination.parent_path();
    const int fd = ::open(
        directory.c_str(),
        O_RDONLY | O_DIRECTORY
    );
    if (fd >= 0) {
        (void)::fsync(fd);
        (void)::close(fd);
    }
    return true;
#endif
}

void append_raw(
    Bytes& out,
    std::span<const Byte> value)
{
    out.insert(out.end(), value.begin(), value.end());
}

std::optional<std::uint32_t> read_ipv4_octet(
    std::string_view text,
    std::size_t& offset) noexcept
{
    if (offset >= text.size()) {
        return std::nullopt;
    }

    std::uint32_t value{0U};
    std::size_t digits{0U};

    while (offset < text.size() &&
           text[offset] >= '0' &&
           text[offset] <= '9') {
        if (digits == 3U) {
            return std::nullopt;
        }
        value = value * 10U +
            static_cast<std::uint32_t>(
                text[offset] - '0');
        if (value > 255U) {
            return std::nullopt;
        }
        ++offset;
        ++digits;
    }

    if (digits == 0U) {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::optional<std::uint32_t> parse_ipv4(
    std::string_view text) noexcept
{
    std::array<std::uint32_t, 4> octets{};
    std::size_t offset{0U};

    for (std::size_t i = 0U; i < octets.size(); ++i) {
        const auto octet =
            read_ipv4_octet(text, offset);
        if (!octet) {
            return std::nullopt;
        }
        octets[i] = *octet;

        if (i + 1U < octets.size()) {
            if (offset >= text.size() ||
                text[offset] != '.') {
                return std::nullopt;
            }
            ++offset;
        }
    }

    if (offset != text.size()) {
        return std::nullopt;
    }

    return (octets[0] << 24U) |
           (octets[1] << 16U) |
           (octets[2] << 8U) |
           octets[3];
}

std::string format_ipv4(
    std::uint32_t address)
{
    return std::to_string(
               (address >> 24U) & 0xffU) + "." +
           std::to_string(
               (address >> 16U) & 0xffU) + "." +
           std::to_string(
               (address >> 8U) & 0xffU) + "." +
           std::to_string(address & 0xffU);
}

bool valid_peer_address(
    const PeerAddress& address,
    bool allow_local) noexcept
{
    if (address.port == 0U) {
        return false;
    }

    const std::uint32_t first =
        (address.ipv4 >> 24U) & 0xffU;
    const std::uint32_t second =
        (address.ipv4 >> 16U) & 0xffU;

    if (first == 0U ||
        first >= 224U ||
        address.ipv4 == 0xffffffffU) {
        return false;
    }

    if (allow_local) {
        return true;
    }

    if (first == 10U ||
        first == 127U ||
        (first == 169U && second == 254U) ||
        (first == 172U &&
         second >= 16U && second <= 31U) ||
        (first == 192U && second == 168U)) {
        return false;
    }

    return true;
}

Bytes serialize_addresses(
    std::span<const PeerAddress> addresses)
{
    const std::size_t count =
        std::min<std::size_t>(
            addresses.size(),
            kMaxAddrMessageEntries
        );

    Bytes out;
    out.reserve(
        compact_size_serialized_size(count) +
        count * (4U + 2U + 8U + 8U)
    );

    append_compact_size(
        out,
        static_cast<std::uint64_t>(count)
    );

    for (std::size_t i = 0U; i < count; ++i) {
        append_little_endian(
            out,
            addresses[i].ipv4
        );
        append_little_endian(
            out,
            addresses[i].port
        );
        append_little_endian(
            out,
            addresses[i].services
        );
        append_little_endian(
            out,
            addresses[i].last_seen
        );
    }

    return out;
}

std::optional<std::vector<PeerAddress>>
parse_addresses(
    std::span<const Byte> payload,
    bool allow_local)
{
    std::size_t offset{0U};
    const auto count =
        read_compact_size(payload, offset);

    if (!count ||
        *count > kMaxAddrMessageEntries) {
        return std::nullopt;
    }

    constexpr std::size_t record_size =
        4U + 2U + 8U + 8U;

    if (*count >
        static_cast<std::uint64_t>(
            (payload.size() - offset) /
            record_size)) {
        return std::nullopt;
    }

    std::vector<PeerAddress> out;
    out.reserve(static_cast<std::size_t>(*count));

    for (std::uint64_t i = 0U; i < *count; ++i) {
        const auto ipv4 =
            read_little_endian<std::uint32_t>(
                payload,
                offset
            );
        const auto port =
            read_little_endian<std::uint16_t>(
                payload,
                offset
            );
        const auto services =
            read_little_endian<std::uint64_t>(
                payload,
                offset
            );
        const auto last_seen =
            read_little_endian<std::uint64_t>(
                payload,
                offset
            );

        if (!ipv4 || !port ||
            !services || !last_seen) {
            return std::nullopt;
        }

        PeerAddress address{
            .ipv4 = *ipv4,
            .port = *port,
            .services = *services,
            .last_seen = *last_seen,
        };

        if (!valid_peer_address(
                address,
                allow_local)) {
            continue;
        }

        const bool duplicate =
            std::any_of(
                out.begin(),
                out.end(),
                [&](const PeerAddress& item) {
                    return same_endpoint(
                        item,
                        address
                    );
                }
            );

        if (!duplicate) {
            out.push_back(address);
        }
    }

    if (offset != payload.size()) {
        return std::nullopt;
    }

    return out;
}

AddrManager::AddrManager(
    const consensus::ChainParams& params,
    std::filesystem::path directory,
    bool allow_local)
    : params_(params),
      directory_(std::move(directory)),
      path_(directory_ / "peers.dat"),
      allow_local_(allow_local)
{
}

AddrStoreError AddrManager::load()
{
    entries_.clear();

    std::error_code ec;
    if (!std::filesystem::exists(path_, ec)) {
        return ec
            ? AddrStoreError::io_error
            : AddrStoreError::not_found;
    }

    std::ifstream input(
        path_,
        std::ios::binary
    );
    if (!input) {
        return AddrStoreError::io_error;
    }

    input.seekg(0, std::ios::end);
    const auto end = input.tellg();
    if (end < 0) {
        return AddrStoreError::io_error;
    }

    const auto size =
        static_cast<std::uintmax_t>(end);

    if (size >
        static_cast<std::uintmax_t>(
            std::numeric_limits<std::size_t>::max()) ||
        size < kPeerMagic.size() +
               sizeof(std::uint32_t) +
               sizeof(Byte) +
               4U +
               1U +
               kChecksumSize) {
        return AddrStoreError::corrupt;
    }

    Bytes bytes(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    input.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size())
    );

    if (!input) {
        return AddrStoreError::io_error;
    }

    const auto body =
        std::span<const Byte>(
            bytes.data(),
            bytes.size() - kChecksumSize
        );
    const auto checksum =
        crypto::double_sha256(body);

    if (!std::equal(
            checksum.begin(),
            checksum.end(),
            bytes.end() -
                static_cast<std::ptrdiff_t>(kChecksumSize))) {
        return AddrStoreError::corrupt;
    }

    std::size_t offset{0U};
    if (!std::equal(
            kPeerMagic.begin(),
            kPeerMagic.end(),
            bytes.begin())) {
        return AddrStoreError::corrupt;
    }
    offset += kPeerMagic.size();

    const auto version =
        read_little_endian<std::uint32_t>(
            body,
            offset
        );
    if (!version ||
        *version != kPeerStoreVersion) {
        return AddrStoreError::corrupt;
    }

    if (offset >= body.size()) {
        return AddrStoreError::corrupt;
    }
    const Byte network = body[offset++];

    const Byte expected_network =
        static_cast<Byte>(params_.network);

    if (network != expected_network) {
        return AddrStoreError::wrong_network;
    }

    if (offset + params_.message_start.size() >
        body.size() ||
        !std::equal(
            params_.message_start.begin(),
            params_.message_start.end(),
            body.begin() +
                static_cast<std::ptrdiff_t>(offset))) {
        return AddrStoreError::wrong_network;
    }
    offset += params_.message_start.size();

    const auto count =
        read_compact_size(body, offset);
    if (!count ||
        *count > 1'000'000ULL ||
        *count >
            static_cast<std::uint64_t>(
                (body.size() - offset) /
                kPeerRecordSize)) {
        return AddrStoreError::corrupt;
    }

    std::vector<AddrInfo> loaded;
    loaded.reserve(static_cast<std::size_t>(*count));

    for (std::uint64_t i = 0U; i < *count; ++i) {
        const auto ipv4 =
            read_little_endian<std::uint32_t>(
                body,
                offset
            );
        const auto port =
            read_little_endian<std::uint16_t>(
                body,
                offset
            );
        const auto services =
            read_little_endian<std::uint64_t>(
                body,
                offset
            );
        const auto last_seen =
            read_little_endian<std::uint64_t>(
                body,
                offset
            );
        const auto last_attempt =
            read_little_endian<std::uint64_t>(
                body,
                offset
            );
        const auto last_success =
            read_little_endian<std::uint64_t>(
                body,
                offset
            );
        const auto next_attempt =
            read_little_endian<std::uint64_t>(
                body,
                offset
            );
        const auto failures =
            read_little_endian<std::uint32_t>(
                body,
                offset
            );

        if (!ipv4 || !port || !services ||
            !last_seen || !last_attempt ||
            !last_success || !next_attempt ||
            !failures) {
            return AddrStoreError::corrupt;
        }

        AddrInfo info{
            .address = PeerAddress{
                .ipv4 = *ipv4,
                .port = *port,
                .services = *services,
                .last_seen = *last_seen,
            },
            .last_attempt = *last_attempt,
            .last_success = *last_success,
            .next_attempt = *next_attempt,
            .failures = *failures,
        };

        if (!valid_peer_address(
                info.address,
                allow_local_)) {
            continue;
        }

        const bool duplicate =
            std::any_of(
                loaded.begin(),
                loaded.end(),
                [&](const AddrInfo& item) {
                    return same_endpoint(
                        item.address,
                        info.address
                    );
                }
            );
        if (!duplicate) {
            loaded.push_back(info);
        }
    }

    if (offset != body.size()) {
        return AddrStoreError::corrupt;
    }

    entries_ = std::move(loaded);
    return AddrStoreError::none;
}

AddrStoreError AddrManager::save() const
{
    std::error_code ec;
    std::filesystem::create_directories(
        directory_,
        ec
    );
    if (ec) {
        return AddrStoreError::io_error;
    }

    Bytes body;
    append_raw(body, kPeerMagic);
    append_little_endian(
        body,
        kPeerStoreVersion
    );
    body.push_back(
        static_cast<Byte>(params_.network)
    );
    append_raw(
        body,
        params_.message_start
    );
    append_compact_size(
        body,
        static_cast<std::uint64_t>(
            entries_.size())
    );

    for (const auto& info : entries_) {
        append_little_endian(
            body,
            info.address.ipv4
        );
        append_little_endian(
            body,
            info.address.port
        );
        append_little_endian(
            body,
            info.address.services
        );
        append_little_endian(
            body,
            info.address.last_seen
        );
        append_little_endian(
            body,
            info.last_attempt
        );
        append_little_endian(
            body,
            info.last_success
        );
        append_little_endian(
            body,
            info.next_attempt
        );
        append_little_endian(
            body,
            info.failures
        );
    }

    const auto checksum =
        crypto::double_sha256(body);

    body.insert(
        body.end(),
        checksum.begin(),
        checksum.end()
    );

    const auto temporary =
        path_.string() + ".tmp";

#ifdef _WIN32
    std::FILE* file{nullptr};
    if (fopen_s(
            &file,
            temporary.c_str(),
            "wb") != 0) {
        file = nullptr;
    }
#else
    std::FILE* file =
        std::fopen(
            temporary.c_str(),
            "wb"
        );
#endif

    if (file == nullptr) {
        return AddrStoreError::io_error;
    }

    const std::size_t written =
        std::fwrite(
            body.data(),
            1U,
            body.size(),
            file
        );

    const bool durable =
        written == body.size() &&
        flush_file(file);
    const bool closed =
        std::fclose(file) == 0;

    if (!durable || !closed ||
        !replace_file(
            temporary,
            path_)) {
        std::error_code ignored;
        std::filesystem::remove(
            temporary,
            ignored
        );
        return AddrStoreError::io_error;
    }

    return AddrStoreError::none;
}

bool AddrManager::add(
    const PeerAddress& address)
{
    if (!valid_peer_address(
            address,
            allow_local_)) {
        return false;
    }

    if (auto* existing = find(address)) {
        existing->address.services |=
            address.services;
        existing->address.last_seen =
            std::max(
                existing->address.last_seen,
                address.last_seen
            );
        return false;
    }

    entries_.push_back(
        AddrInfo{.address = address}
    );
    return true;
}

std::size_t AddrManager::add(
    std::span<const PeerAddress> addresses)
{
    std::size_t added{0U};
    for (const auto& address : addresses) {
        if (add(address)) {
            ++added;
        }
    }
    return added;
}

void AddrManager::mark_attempt(
    const PeerAddress& address,
    std::uint64_t now)
{
    if (auto* entry = find(address)) {
        entry->last_attempt = now;
    }
}

void AddrManager::mark_success(
    const PeerAddress& address,
    std::uint64_t now)
{
    if (auto* entry = find(address)) {
        entry->last_attempt = now;
        entry->last_success = now;
        entry->failures = 0U;
        entry->next_attempt =
            now > std::numeric_limits<std::uint64_t>::max() - 60U
                ? std::numeric_limits<std::uint64_t>::max()
                : now + 60U;
    }
}

void AddrManager::mark_failure(
    const PeerAddress& address,
    std::uint64_t now)
{
    if (auto* entry = find(address)) {
        entry->last_attempt = now;
        if (entry->failures <
            std::numeric_limits<std::uint32_t>::max()) {
            ++entry->failures;
        }

        const auto delay =
            failure_delay(entry->failures);
        entry->next_attempt =
            now >
                std::numeric_limits<std::uint64_t>::max() -
                    delay
                ? std::numeric_limits<std::uint64_t>::max()
                : now + delay;
    }
}

std::optional<PeerAddress> AddrManager::select(
    std::uint64_t now,
    std::span<const PeerAddress> excluded) const
{
    const AddrInfo* best{nullptr};

    for (const auto& entry : entries_) {
        if (entry.next_attempt > now ||
            excluded_endpoint(
                entry.address,
                excluded)) {
            continue;
        }

        if (best == nullptr ||
            entry.failures < best->failures ||
            (entry.failures == best->failures &&
             entry.last_success > best->last_success) ||
            (entry.failures == best->failures &&
             entry.last_success == best->last_success &&
             entry.last_attempt < best->last_attempt)) {
            best = &entry;
        }
    }

    if (best == nullptr) {
        return std::nullopt;
    }

    return best->address;
}

std::vector<PeerAddress> AddrManager::addresses(
    std::size_t limit) const
{
    const std::size_t count =
        std::min<std::size_t>(
            {limit,
             entries_.size(),
             kMaxAddrMessageEntries}
        );

    std::vector<PeerAddress> out;
    out.reserve(count);

    for (std::size_t i = 0U; i < count; ++i) {
        out.push_back(entries_[i].address);
    }

    return out;
}

std::size_t AddrManager::size() const noexcept
{
    return entries_.size();
}

const std::vector<AddrInfo>&
AddrManager::entries() const noexcept
{
    return entries_;
}

const std::filesystem::path&
AddrManager::path() const noexcept
{
    return path_;
}

AddrInfo* AddrManager::find(
    const PeerAddress& address) noexcept
{
    const auto it = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&](const AddrInfo& info) {
            return same_endpoint(
                info.address,
                address
            );
        }
    );
    return it == entries_.end()
        ? nullptr
        : &*it;
}

const AddrInfo* AddrManager::find(
    const PeerAddress& address) const noexcept
{
    const auto it = std::find_if(
        entries_.begin(),
        entries_.end(),
        [&](const AddrInfo& info) {
            return same_endpoint(
                info.address,
                address
            );
        }
    );
    return it == entries_.end()
        ? nullptr
        : &*it;
}

std::span<const SeedEndpoint>
hardcoded_seeds(
    consensus::Network network) noexcept
{
    static constexpr std::array<SeedEndpoint, 0>
        no_seeds{};

    // Public QUINTUM seed infrastructure does not exist yet.
    // Do not invent or silently depend on a developer-controlled host.
    // This hook is deliberately ready for pinned independent seed nodes.
    switch (network) {
    case consensus::Network::mainnet:
    case consensus::Network::testnet:
    case consensus::Network::regtest:
    default:
        return no_seeds;
    }
}

} // namespace quintum::net
