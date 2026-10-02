#include "consensus/pow.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace quintum::consensus {
namespace {

bool is_zero_hash(const Hash256& value) noexcept
{
    return std::all_of(
        value.begin(),
        value.end(),
        [](Byte byte) { return byte == 0U; }
    );
}

} // namespace

bool CompactTarget::is_zero() const noexcept
{
    return is_zero_hash(target);
}

bool CompactTarget::valid() const noexcept
{
    return !negative && !overflow && !is_zero();
}

CompactTarget decode_compact_target(std::uint32_t bits)
{
    CompactTarget result;

    const std::uint32_t exponent = bits >> 24U;
    const std::uint32_t mantissa = bits & 0x007fffffU;

    result.negative = (mantissa != 0U) && ((bits & 0x00800000U) != 0U);
    result.overflow =
        (mantissa != 0U) &&
        (
            exponent > 34U ||
            (mantissa > 0xffU && exponent > 33U) ||
            (mantissa > 0xffffU && exponent > 32U)
        );

    if (mantissa == 0U) {
        return result;
    }

    if (exponent <= 3U) {
        const auto shifted =
            mantissa >> (8U * (3U - exponent));

        result.target[29] = static_cast<Byte>(shifted >> 16U);
        result.target[30] = static_cast<Byte>(shifted >> 8U);
        result.target[31] = static_cast<Byte>(shifted);
        return result;
    }

    const std::array<Byte, 3> bytes{
        static_cast<Byte>(mantissa >> 16U),
        static_cast<Byte>(mantissa >> 8U),
        static_cast<Byte>(mantissa),
    };

    const auto start = static_cast<std::int64_t>(32) -
                       static_cast<std::int64_t>(exponent);

    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto position = start + static_cast<std::int64_t>(i);
        if (position >= 0 && position < 32) {
            result.target[static_cast<std::size_t>(position)] = bytes[i];
        }
    }

    return result;
}

std::uint32_t encode_compact_target(const Hash256& target)
{
    std::size_t first = 0U;
    while (first < target.size() && target[first] == 0U) {
        ++first;
    }

    if (first == target.size()) {
        return 0U;
    }

    std::uint32_t exponent =
        static_cast<std::uint32_t>(target.size() - first);

    std::uint32_t mantissa{0U};

    if (exponent <= 3U) {
        for (std::size_t i = first; i < target.size(); ++i) {
            mantissa = (mantissa << 8U) | target[i];
        }
        mantissa <<= 8U * (3U - exponent);
    } else {
        mantissa = static_cast<std::uint32_t>(target[first]) << 16U;

        if (first + 1U < target.size()) {
            mantissa |= static_cast<std::uint32_t>(target[first + 1U]) << 8U;
        }
        if (first + 2U < target.size()) {
            mantissa |= static_cast<std::uint32_t>(target[first + 2U]);
        }
    }

    if ((mantissa & 0x00800000U) != 0U) {
        mantissa >>= 8U;
        ++exponent;
    }

    return (exponent << 24U) | (mantissa & 0x007fffffU);
}

bool hash_meets_target(
    const Hash256& hash,
    const Hash256& target) noexcept
{
    return !std::lexicographical_compare(
        target.begin(),
        target.end(),
        hash.begin(),
        hash.end()
    );
}

PowCheckError check_proof_of_work(const BlockHeader& header)
{
    const auto compact = decode_compact_target(header.bits);
    if (!compact.valid()) {
        return PowCheckError::invalid_target;
    }

    const auto hash = block_hash(header);
    if (!hash_meets_target(hash, compact.target)) {
        return PowCheckError::hash_above_target;
    }

    return PowCheckError::none;
}

MiningResult mine_header(
    BlockHeader& header,
    std::uint64_t max_attempts)
{
    const auto compact = decode_compact_target(header.bits);

    MiningResult result;
    result.nonce = header.nonce;

    if (!compact.valid()) {
        result.status = MineStatus::invalid_target;
        return result;
    }

    for (std::uint64_t attempt = 0U; attempt < max_attempts; ++attempt) {
        result.hash = block_hash(header);
        result.nonce = header.nonce;
        result.attempts = attempt + 1U;

        if (hash_meets_target(result.hash, compact.target)) {
            result.status = MineStatus::found;
            return result;
        }

        if (header.nonce == std::numeric_limits<std::uint64_t>::max()) {
            result.status = MineStatus::exhausted;
            return result;
        }

        ++header.nonce;
    }

    result.status = MineStatus::exhausted;
    return result;
}

} // namespace quintum::consensus
