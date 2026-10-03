#include "wallet/fee_policy.hpp"

#include "consensus/monetary.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace quintum::wallet {
namespace {

std::optional<Amount> fee_rate_for_entry(
    const MempoolEntry& entry) noexcept
{
    if (entry.serialized_size == 0U ||
        entry.fee == 0U ||
        !consensus::money_range(entry.fee)) {
        return std::nullopt;
    }

    const Amount size =
        static_cast<Amount>(
            entry.serialized_size
        );

    if (static_cast<std::size_t>(size) !=
        entry.serialized_size) {
        return std::nullopt;
    }

    const Amount whole =
        entry.fee / size;
    const Amount remainder =
        entry.fee % size;

    if (whole >
        std::numeric_limits<Amount>::max() /
            1'000U) {
        return std::nullopt;
    }

    Amount rate = whole * 1'000U;

    const Amount fractional =
        (remainder * 1'000U) / size +
        (((remainder * 1'000U) % size) != 0U
             ? 1U
             : 0U);

    if (fractional >
        std::numeric_limits<Amount>::max() -
            rate) {
        return std::nullopt;
    }

    rate += fractional;
    return rate;
}

} // namespace

std::optional<Amount> fee_for_size(
    std::size_t serialized_size,
    Amount rate_per_kb) noexcept
{
    if (!consensus::money_range(rate_per_kb)) {
        return std::nullopt;
    }

    if (serialized_size == 0U ||
        rate_per_kb == 0U) {
        return Amount{0U};
    }

    const std::size_t whole_kb =
        serialized_size / 1'000U;
    const std::size_t remainder_bytes =
        serialized_size % 1'000U;

    if (whole_kb >
        static_cast<std::size_t>(
            consensus::kMaxMoney /
            rate_per_kb)) {
        return std::nullopt;
    }

    Amount fee =
        static_cast<Amount>(whole_kb) *
        rate_per_kb;

    const Amount rate_whole =
        rate_per_kb / 1'000U;
    const Amount rate_remainder =
        rate_per_kb % 1'000U;

    const Amount partial_base =
        static_cast<Amount>(
            remainder_bytes
        ) * rate_whole;

    const Amount partial_remainder_product =
        static_cast<Amount>(
            remainder_bytes
        ) * rate_remainder;

    const Amount partial =
        partial_base +
        partial_remainder_product / 1'000U +
        ((partial_remainder_product % 1'000U) != 0U
             ? 1U
             : 0U);

    if (partial >
        consensus::kMaxMoney - fee) {
        return std::nullopt;
    }

    fee += partial;

    if (!consensus::money_range(fee)) {
        return std::nullopt;
    }

    return fee;
}

Amount recommended_fee_rate(
    const Mempool& mempool) noexcept
{
    std::vector<Amount> rates;
    rates.reserve(mempool.entries().size());

    for (const auto& entry :
         mempool.entries()) {
        const auto rate =
            fee_rate_for_entry(entry);

        if (rate) {
            rates.push_back(*rate);
        }
    }

    if (rates.empty()) {
        return kDefaultFeeRatePerKb;
    }

    std::sort(
        rates.begin(),
        rates.end()
    );

    const Amount median =
        rates[rates.size() / 2U];

    return std::max(
        kDefaultFeeRatePerKb,
        median
    );
}

} // namespace quintum::wallet
