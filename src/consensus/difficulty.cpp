#include "consensus/difficulty.hpp"

#include "consensus/pow.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace quintum::consensus {
namespace {

bool less_or_equal(
    const Hash256& lhs,
    const Hash256& rhs) noexcept
{
    return !std::lexicographical_compare(
        rhs.begin(), rhs.end(),
        lhs.begin(), lhs.end()
    );
}

struct DivideResult {
    Hash256 quotient{};
    std::uint64_t remainder{0U};
};

DivideResult divide_by_u64(
    const Hash256& value,
    std::uint64_t divisor)
{
    DivideResult result;

    if (divisor == 0U) {
        return result;
    }

    std::uint64_t remainder{0U};

    for (std::size_t i = 0U; i < value.size(); ++i) {
        const std::uint64_t current =
            (remainder << 8U) |
            static_cast<std::uint64_t>(value[i]);

        result.quotient[i] =
            static_cast<Byte>(current / divisor);

        remainder = current % divisor;
    }

    result.remainder = remainder;
    return result;
}

bool multiply_by_u64(
    const Hash256& value,
    std::uint64_t multiplier,
    Hash256& result) noexcept
{
    std::uint64_t carry{0U};

    for (std::size_t i = value.size(); i-- > 0U;) {
        const std::uint64_t product =
            static_cast<std::uint64_t>(value[i]) *
            multiplier +
            carry;

        result[i] = static_cast<Byte>(product & 0xffU);
        carry = product >> 8U;
    }

    return carry == 0U;
}

bool add_u64(
    Hash256& value,
    std::uint64_t addend) noexcept
{
    std::uint64_t carry = addend;

    for (std::size_t i = value.size(); i-- > 0U;) {
        const std::uint64_t sum =
            static_cast<std::uint64_t>(value[i]) +
            (carry & 0xffU);

        value[i] = static_cast<Byte>(sum & 0xffU);

        carry =
            (carry >> 8U) +
            (sum >> 8U);

        if (carry == 0U) {
            return true;
        }
    }

    return carry == 0U;
}

} // namespace

Hash256 scale_target_clamped(
    const Hash256& target,
    std::uint64_t numerator,
    std::uint64_t denominator,
    const Hash256& limit)
{
    if (denominator == 0U) {
        return limit;
    }

    const auto divided =
        divide_by_u64(target, denominator);

    Hash256 scaled{};
    if (!multiply_by_u64(
            divided.quotient,
            numerator,
            scaled)) {
        return limit;
    }

    if (numerator != 0U &&
        divided.remainder >
            std::numeric_limits<std::uint64_t>::max() /
            numerator) {
        return limit;
    }

    const std::uint64_t correction =
        (divided.remainder * numerator) /
        denominator;

    if (!add_u64(scaled, correction)) {
        return limit;
    }

    if (!less_or_equal(scaled, limit)) {
        return limit;
    }

    return scaled;
}

bool target_within_pow_limit(
    std::uint32_t bits,
    const PowParams& params)
{
    const auto target = decode_compact_target(bits);
    const auto limit =
        decode_compact_target(params.pow_limit_bits);

    if (!target.valid() ||
        !limit.valid() ||
        encode_compact_target(target.target) != bits ||
        encode_compact_target(limit.target) !=
            params.pow_limit_bits) {
        return false;
    }

    return less_or_equal(target.target, limit.target);
}

DifficultyResult calculate_retarget_bits(
    std::uint32_t previous_bits,
    std::uint64_t first_block_timestamp,
    std::uint64_t last_block_timestamp,
    const PowParams& params)
{
    DifficultyResult result;

    if (params.no_retargeting) {
        result.bits = previous_bits;
        return result;
    }

    if (params.retarget_interval == 0U ||
        params.target_spacing_seconds == 0U) {
        result.error =
            DifficultyError::arithmetic_failure;
        return result;
    }

    const auto previous =
        decode_compact_target(previous_bits);
    const auto limit =
        decode_compact_target(params.pow_limit_bits);

    if (!previous.valid() ||
        encode_compact_target(previous.target) !=
            previous_bits) {
        result.error =
            DifficultyError::invalid_previous_target;
        return result;
    }

    if (!limit.valid() ||
        encode_compact_target(limit.target) !=
            params.pow_limit_bits) {
        result.error =
            DifficultyError::invalid_pow_limit;
        return result;
    }

    if (!less_or_equal(previous.target, limit.target)) {
        result.error =
            DifficultyError::invalid_previous_target;
        return result;
    }

    const auto interval =
        static_cast<std::uint64_t>(
            params.retarget_interval
        );

    if (params.target_spacing_seconds >
        std::numeric_limits<std::uint64_t>::max() /
            interval) {
        result.error =
            DifficultyError::arithmetic_failure;
        return result;
    }

    const std::uint64_t target_timespan =
        params.target_spacing_seconds * interval;

    if (target_timespan == 0U) {
        result.error =
            DifficultyError::arithmetic_failure;
        return result;
    }

    std::uint64_t actual_timespan{0U};

    if (last_block_timestamp >
        first_block_timestamp) {
        actual_timespan =
            last_block_timestamp -
            first_block_timestamp;
    }

    const std::uint64_t minimum_timespan =
        target_timespan / 4U;
    const std::uint64_t maximum_timespan =
        target_timespan >
                std::numeric_limits<std::uint64_t>::max() /
                4U
            ? std::numeric_limits<std::uint64_t>::max()
            : target_timespan * 4U;

    actual_timespan = std::clamp(
        actual_timespan,
        minimum_timespan,
        maximum_timespan
    );

    const auto scaled = scale_target_clamped(
        previous.target,
        actual_timespan,
        target_timespan,
        limit.target
    );

    result.bits = encode_compact_target(scaled);
    return result;
}

} // namespace quintum::consensus
