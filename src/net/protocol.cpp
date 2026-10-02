#include "net/protocol.hpp"

#include "crypto/sha256.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace quintum::net {
namespace {

constexpr std::size_t kCommandSize = 12U;
constexpr std::size_t kChecksumSize = 4U;

bool valid_command_char(char ch) noexcept
{
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9');
}

bool valid_command(std::string_view command) noexcept
{
    if (command.empty() ||
        command.size() > kCommandSize) {
        return false;
    }

    return std::all_of(
        command.begin(),
        command.end(),
        valid_command_char
    );
}

} // namespace

WireEncodeResult encode_message(
    const consensus::ChainParams& params,
    std::string_view command,
    std::span<const Byte> payload)
{
    WireEncodeResult out;

    if (!valid_command(command)) {
        out.error = WireError::invalid_command;
        return out;
    }

    if (payload.size() >
        static_cast<std::size_t>(kMaxMessagePayload) ||
        payload.size() >
        static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max())) {
        out.error = WireError::payload_too_large;
        return out;
    }

    out.bytes.reserve(
        kMessageHeaderSize + payload.size()
    );

    out.bytes.insert(
        out.bytes.end(),
        params.message_start.begin(),
        params.message_start.end()
    );

    for (std::size_t i = 0U; i < kCommandSize; ++i) {
        out.bytes.push_back(
            i < command.size()
                ? static_cast<Byte>(command[i])
                : 0U
        );
    }

    append_little_endian(
        out.bytes,
        static_cast<std::uint32_t>(payload.size())
    );

    const auto digest =
        crypto::double_sha256(payload);

    out.bytes.insert(
        out.bytes.end(),
        digest.begin(),
        digest.begin() +
            static_cast<std::ptrdiff_t>(kChecksumSize)
    );

    out.bytes.insert(
        out.bytes.end(),
        payload.begin(),
        payload.end()
    );

    return out;
}

WireDecodeResult decode_message(
    const consensus::ChainParams& params,
    std::span<const Byte> bytes)
{
    WireDecodeResult out;

    if (bytes.size() < kMessageHeaderSize) {
        out.error = WireError::truncated;
        return out;
    }

    if (!std::equal(
            params.message_start.begin(),
            params.message_start.end(),
            bytes.begin())) {
        out.error = WireError::bad_magic;
        return out;
    }

    std::size_t command_end = 4U;

    while (command_end < 4U + kCommandSize &&
           bytes[command_end] != 0U) {
        const char ch =
            static_cast<char>(bytes[command_end]);

        if (!valid_command_char(ch)) {
            out.error = WireError::malformed_header;
            return out;
        }

        ++command_end;
    }

    if (command_end == 4U) {
        out.error = WireError::malformed_header;
        return out;
    }

    for (std::size_t i = command_end;
         i < 4U + kCommandSize;
         ++i) {
        if (bytes[i] != 0U) {
            out.error = WireError::malformed_header;
            return out;
        }
    }

    out.message.command.assign(
        reinterpret_cast<const char*>(bytes.data() + 4U),
        command_end - 4U
    );

    std::size_t offset = 4U + kCommandSize;
    const auto payload_size =
        read_little_endian<std::uint32_t>(
            bytes,
            offset
        );

    if (!payload_size) {
        out.error = WireError::truncated;
        return out;
    }

    if (*payload_size > kMaxMessagePayload) {
        out.error = WireError::payload_too_large;
        return out;
    }

    const std::size_t total_size =
        kMessageHeaderSize +
        static_cast<std::size_t>(*payload_size);

    if (bytes.size() < total_size) {
        out.error = WireError::truncated;
        return out;
    }

    std::array<Byte, kChecksumSize> stored_checksum{};
    std::copy_n(
        bytes.begin() +
            static_cast<std::ptrdiff_t>(offset),
        kChecksumSize,
        stored_checksum.begin()
    );

    const auto payload =
        bytes.subspan(
            kMessageHeaderSize,
            static_cast<std::size_t>(*payload_size)
        );

    const auto digest =
        crypto::double_sha256(payload);

    if (!std::equal(
            stored_checksum.begin(),
            stored_checksum.end(),
            digest.begin())) {
        out.error = WireError::checksum_mismatch;
        return out;
    }

    out.message.payload.assign(
        payload.begin(),
        payload.end()
    );
    out.consumed = total_size;
    return out;
}

Bytes serialize_version(
    const VersionMessage& version)
{
    Bytes out;
    out.reserve(32U);

    append_little_endian(
        out,
        version.protocol_version
    );
    append_little_endian(
        out,
        version.services
    );
    append_little_endian(
        out,
        version.timestamp
    );
    append_little_endian(
        out,
        version.nonce
    );
    append_little_endian(
        out,
        version.start_height
    );

    return out;
}

std::optional<VersionMessage> parse_version(
    std::span<const Byte> payload)
{
    if (payload.size() != 32U) {
        return std::nullopt;
    }

    std::size_t offset{0U};

    const auto protocol_version =
        read_little_endian<std::uint32_t>(
            payload,
            offset
        );
    const auto services =
        read_little_endian<std::uint64_t>(
            payload,
            offset
        );
    const auto timestamp =
        read_little_endian<std::uint64_t>(
            payload,
            offset
        );
    const auto nonce =
        read_little_endian<std::uint64_t>(
            payload,
            offset
        );
    const auto start_height =
        read_little_endian<std::uint32_t>(
            payload,
            offset
        );

    if (!protocol_version ||
        !services ||
        !timestamp ||
        !nonce ||
        !start_height ||
        offset != payload.size()) {
        return std::nullopt;
    }

    return VersionMessage{
        .protocol_version = *protocol_version,
        .services = *services,
        .timestamp = *timestamp,
        .nonce = *nonce,
        .start_height = *start_height,
    };
}

Bytes serialize_nonce(std::uint64_t nonce)
{
    Bytes out;
    out.reserve(sizeof(nonce));
    append_little_endian(out, nonce);
    return out;
}

std::optional<std::uint64_t> parse_nonce(
    std::span<const Byte> payload)
{
    if (payload.size() != sizeof(std::uint64_t)) {
        return std::nullopt;
    }

    std::size_t offset{0U};
    const auto value =
        read_little_endian<std::uint64_t>(
            payload,
            offset
        );

    if (!value || offset != payload.size()) {
        return std::nullopt;
    }

    return value;
}

} // namespace quintum::net
