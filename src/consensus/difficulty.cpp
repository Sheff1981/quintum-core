#include "consensus/difficulty.hpp"

#include "consensus/pow.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace quintum::consensus {
namespace {

using WideTarget = std::array<std::uint32_t, 9>;

WideTarget to_wide_target(const Hash256& value) noexcept
{
    WideTarget out{};
    for (std::size_t limb = 0U; limb < 8U; ++limb) {
        const std::size_t offset =
            value.size() - ((limb + 1U) * 4U);
        out[limb] =
            (static_cast<std::uint32_t>(value[offset]) << 24U) |
            (static_cast<std::uint32_t>(value[offset + 1U]) << 16U) |
            (static_cast<std::uint32_t>(value[offset + 2U]) << 8U) |
            static_cast<std::uint32_t>(value[offset + 3U]);
    }
    return out;
}

Hash256 from_wide_target(const WideTarget& value) noexcept
{
    Hash256 out{};
    for (std::size_t limb = 0U; limb < 8U; ++limb) {
        const std::size_t offset =
            out.size() - ((limb + 1U) * 4U);
        const std::uint32_t word = value[limb];
        out[offset] = static_cast<Byte>(word >> 24U);
        out[offset + 1U] = static_cast<Byte>(word >> 16U);
        out[offset + 2U] = static_cast<Byte>(word >> 8U);
        out[offset + 3U] = static_cast<Byte>(word);
    }
    return out;
}

bool wide_is_zero(const WideTarget& value) noexcept
{
    return std::all_of(
        value.begin(), value.end(),
        [](std::uint32_t word) { return word == 0U; }
    );
}

bool multiply_wide(
    WideTarget& value,
    std::uint32_t multiplier) noexcept
{
    std::uint64_t carry{0U};
    for (auto& word : value) {
        const std::uint64_t product =
            static_cast<std::uint64_t>(word) * multiplier + carry;
        word = static_cast<std::uint32_t>(
            product & 0xffffffffULL
        );
        carry = product >> 32U;
    }
    return carry == 0U;
}

bool shift_left_one_wide(WideTarget& value) noexcept
{
    std::uint32_t carry{0U};
    for (auto& word : value) {
        const std::uint32_t next = word >> 31U;
        word = static_cast<std::uint32_t>(
            (word << 1U) | carry
        );
        carry = next;
    }
    return carry == 0U;
}

void shift_right_one_wide(WideTarget& value) noexcept
{
    std::uint32_t carry{0U};
    for (std::size_t i = value.size(); i-- > 0U;) {
        const std::uint32_t next = value[i] & 1U;
        value[i] =
            (value[i] >> 1U) | (carry << 31U);
        carry = next;
    }
}

bool checked_mul_i64(
    std::int64_t lhs,
    std::int64_t rhs,
    std::int64_t& out) noexcept
{
    if (lhs == 0 || rhs == 0) {
        out = 0;
        return true;
    }
    if ((lhs == -1 &&
         rhs == std::numeric_limits<std::int64_t>::min()) ||
        (rhs == -1 &&
         lhs == std::numeric_limits<std::int64_t>::min())) {
        return false;
    }
    if (lhs > 0) {
        if (rhs > 0) {
            if (lhs >
                std::numeric_limits<std::int64_t>::max() / rhs) {
                return false;
            }
        } else if (
            rhs <
            std::numeric_limits<std::int64_t>::min() / lhs) {
            return false;
        }
    } else if (rhs > 0) {
        if (lhs <
            std::numeric_limits<std::int64_t>::min() / rhs) {
            return false;
        }
    } else if (
        lhs <
        std::numeric_limits<std::int64_t>::max() / rhs) {
        return false;
    }
    out = lhs * rhs;
    return true;
}

bool checked_add_i64(
    std::int64_t lhs,
    std::int64_t rhs,
    std::int64_t& out) noexcept
{
    if ((rhs > 0 &&
         lhs > std::numeric_limits<std::int64_t>::max() - rhs) ||
        (rhs < 0 &&
         lhs < std::numeric_limits<std::int64_t>::min() - rhs)) {
        return false;
    }
    out = lhs + rhs;
    return true;
}

bool checked_sub_i64(
    std::int64_t lhs,
    std::int64_t rhs,
    std::int64_t& out) noexcept
{
    if (rhs ==
        std::numeric_limits<std::int64_t>::min()) {
        return false;
    }
    return checked_add_i64(lhs, -rhs, out);
}

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

DifficultyResult calculate_asert_bits(
    std::uint32_t reference_bits,
    std::int64_t target_spacing_seconds,
    std::int64_t time_diff_seconds,
    std::int64_t height_diff,
    std::uint32_t pow_limit_bits,
    std::int64_t half_life_seconds)
{
    DifficultyResult result;

    if (target_spacing_seconds <= 0 ||
        half_life_seconds <= 0 ||
        height_diff < 0) {
        result.error = DifficultyError::arithmetic_failure;
        return result;
    }

    const auto reference =
        decode_compact_target(reference_bits);
    const auto limit =
        decode_compact_target(pow_limit_bits);

    if (!reference.valid() ||
        encode_compact_target(reference.target) != reference_bits) {
        result.error = DifficultyError::invalid_previous_target;
        return result;
    }

    if (!limit.valid() ||
        encode_compact_target(limit.target) != pow_limit_bits) {
        result.error = DifficultyError::invalid_pow_limit;
        return result;
    }

    if (!less_or_equal(reference.target, limit.target)) {
        result.error = DifficultyError::invalid_previous_target;
        return result;
    }

    if (height_diff ==
        std::numeric_limits<std::int64_t>::max()) {
        result.error = DifficultyError::arithmetic_failure;
        return result;
    }

    std::int64_t ideal_time{0};
    if (!checked_mul_i64(
            target_spacing_seconds,
            height_diff + 1,
            ideal_time)) {
        result.error = DifficultyError::arithmetic_failure;
        return result;
    }

    std::int64_t schedule_delta{0};
    if (!checked_sub_i64(
            time_diff_seconds,
            ideal_time,
            schedule_delta)) {
        result.error = DifficultyError::arithmetic_failure;
        return result;
    }

    const std::int64_t whole =
        schedule_delta / half_life_seconds;
    const std::int64_t remainder =
        schedule_delta % half_life_seconds;

    std::int64_t whole_scaled{0};
    std::int64_t remainder_scaled{0};
    if (!checked_mul_i64(
            whole, 65'536, whole_scaled) ||
        !checked_mul_i64(
            remainder, 65'536, remainder_scaled)) {
        result.error = DifficultyError::arithmetic_failure;
        return result;
    }

    const std::int64_t fractional_scaled =
        remainder_scaled / half_life_seconds;

    std::int64_t exponent{0};
    if (!checked_add_i64(
            whole_scaled,
            fractional_scaled,
            exponent)) {
        result.error = DifficultyError::arithmetic_failure;
        return result;
    }

    static_assert(
        std::int64_t{-1} >> 1 == std::int64_t{-1},
        "ASERT requires arithmetic signed right shift"
    );

    std::int64_t shifts = exponent >> 16U;
    const std::uint16_t frac =
        static_cast<std::uint16_t>(exponent);
    const std::uint64_t f =
        static_cast<std::uint64_t>(frac);

    const std::uint64_t polynomial =
        195'766'423'245'049ULL * f +
        971'821'376ULL * f * f +
        5'127ULL * f * f * f +
        (1ULL << 47U);

    const std::uint32_t factor =
        65'536U +
        static_cast<std::uint32_t>(
            polynomial >> 48U
        );

    WideTarget wide =
        to_wide_target(reference.target);

    if (!multiply_wide(wide, factor)) {
        result.bits = pow_limit_bits;
        return result;
    }

    shifts -= 16;

    if (shifts <= -288) {
        wide.fill(0U);
    } else if (shifts < 0) {
        const auto count =
            static_cast<std::uint32_t>(-shifts);
        for (std::uint32_t i = 0U; i < count; ++i) {
            shift_right_one_wide(wide);
        }
    } else if (shifts >= 288) {
        result.bits = pow_limit_bits;
        return result;
    } else {
        const auto count =
            static_cast<std::uint32_t>(shifts);
        for (std::uint32_t i = 0U; i < count; ++i) {
            if (!shift_left_one_wide(wide)) {
                result.bits = pow_limit_bits;
                return result;
            }
        }
    }

    if (wide_is_zero(wide)) {
        Hash256 minimum{};
        minimum.back() = 1U;
        result.bits = encode_compact_target(minimum);
        return result;
    }

    if (wide[8] != 0U) {
        result.bits = pow_limit_bits;
        return result;
    }

    const Hash256 target =
        from_wide_target(wide);

    if (!less_or_equal(target, limit.target)) {
        result.bits = pow_limit_bits;
        return result;
    }

    result.bits = encode_compact_target(target);
    if (result.bits == 0U) {
        Hash256 minimum{};
        minimum.back() = 1U;
        result.bits = encode_compact_target(minimum);
    }

    return result;
}

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
