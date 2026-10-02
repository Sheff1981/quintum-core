#pragma once

#include "core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

namespace quintum {

using Bytes = std::vector<Byte>;

template <typename T>
requires(std::is_integral_v<T> && std::is_unsigned_v<T>)
void append_little_endian(Bytes& out, T value)
{
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<Byte>(value & static_cast<T>(0xffU)));
        value >>= 8U;
    }
}

template <typename T>
requires(std::is_integral_v<T> && std::is_unsigned_v<T>)
std::optional<T> read_little_endian(std::span<const Byte> data, std::size_t& offset)
{
    if (offset > data.size() || data.size() - offset < sizeof(T)) {
        return std::nullopt;
    }

    T value{0};
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value |= static_cast<T>(data[offset + i]) << (8U * i);
    }

    offset += sizeof(T);
    return value;
}

// Canonical CompactSize encoding, compatible with the proven Bitcoin wire pattern.
inline void append_compact_size(Bytes& out, std::uint64_t value)
{
    if (value < 253U) {
        out.push_back(static_cast<Byte>(value));
    } else if (value <= std::numeric_limits<std::uint16_t>::max()) {
        out.push_back(253U);
        append_little_endian(out, static_cast<std::uint16_t>(value));
    } else if (value <= std::numeric_limits<std::uint32_t>::max()) {
        out.push_back(254U);
        append_little_endian(out, static_cast<std::uint32_t>(value));
    } else {
        out.push_back(255U);
        append_little_endian(out, value);
    }
}

inline std::optional<std::uint64_t> read_compact_size(std::span<const Byte> data, std::size_t& offset)
{
    if (offset >= data.size()) {
        return std::nullopt;
    }

    const Byte prefix = data[offset++];
    if (prefix < 253U) {
        return prefix;
    }

    if (prefix == 253U) {
        const auto value = read_little_endian<std::uint16_t>(data, offset);
        if (!value || *value < 253U) {
            return std::nullopt;
        }
        return *value;
    }

    if (prefix == 254U) {
        const auto value = read_little_endian<std::uint32_t>(data, offset);
        if (!value || *value <= std::numeric_limits<std::uint16_t>::max()) {
            return std::nullopt;
        }
        return *value;
    }

    const auto value = read_little_endian<std::uint64_t>(data, offset);
    if (!value || *value <= std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return *value;
}

} // namespace quintum
