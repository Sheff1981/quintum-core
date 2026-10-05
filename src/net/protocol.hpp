#pragma once

#include "consensus/chainparams.hpp"
#include "core/serialize.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace quintum::net {

inline constexpr std::uint32_t kProtocolVersion = 1U;
inline constexpr std::uint32_t kPeerAddressProtocolVersion = 2U;
inline constexpr std::size_t kMessageHeaderSize = 24U;
inline constexpr std::uint32_t kMaxMessagePayload = 2'000'000U;

enum class WireError {
    none,
    invalid_command,
    payload_too_large,
    truncated,
    bad_magic,
    malformed_header,
    checksum_mismatch,
    malformed_payload,
};

struct WireMessage {
    std::string command{};
    Bytes payload{};
};

struct WireEncodeResult {
    WireError error{WireError::none};
    Bytes bytes{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == WireError::none;
    }
};

struct WireDecodeResult {
    WireError error{WireError::none};
    WireMessage message{};
    std::size_t consumed{0U};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == WireError::none;
    }
};

struct VersionMessage {
    std::uint32_t protocol_version{kProtocolVersion};
    std::uint64_t services{0U};
    std::uint64_t timestamp{0U};
    std::uint64_t nonce{0U};
    std::uint32_t start_height{0U};
    std::uint16_t listen_port{0U};
};

[[nodiscard]] WireEncodeResult encode_message(
    const consensus::ChainParams& params,
    std::string_view command,
    std::span<const Byte> payload
);

[[nodiscard]] WireDecodeResult decode_message(
    const consensus::ChainParams& params,
    std::span<const Byte> bytes
);

[[nodiscard]] Bytes serialize_version(
    const VersionMessage& version
);

[[nodiscard]] std::optional<VersionMessage> parse_version(
    std::span<const Byte> payload
);

[[nodiscard]] Bytes serialize_nonce(std::uint64_t nonce);

[[nodiscard]] std::optional<std::uint64_t> parse_nonce(
    std::span<const Byte> payload
);

} // namespace quintum::net
