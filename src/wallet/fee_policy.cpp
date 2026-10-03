#include "wallet/fee_policy.hpp"

#include "core/serialize.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace quintum::wallet {

Amount recommended_fee_rate(
    const Mempool& mempool) noexcept
{
    std::vector<Amount> rates;
    rates.reserve(mempool.entries().size());

    for (const auto& entry :
         mempool.entries()) {
        const auto rate =
            policy::fee_rate_for_size(
                entry.fee,
                entry.serialized_size
            );

        if (rate) {
            rates.push_back(*rate);
        }
    }

    const Amount floor =
        std::max(
            kDefaultFeeRatePerKb,
            mempool.min_relay_fee_rate_per_kb()
        );

    if (rates.empty()) {
        return floor;
    }

    std::sort(
        rates.begin(),
        rates.end()
    );

    const Amount median =
        rates[rates.size() / 2U];

    return std::max(
        floor,
        median
    );
}


std::optional<std::size_t>
estimate_p2pk_transaction_size(
    std::size_t input_count,
    std::size_t output_count) noexcept
{
    constexpr std::size_t kFixedInputBytes{
        32U + 4U + 1U + 65U + 4U
    };
    constexpr std::size_t kFixedOutputBytes{
        8U + 1U + 34U
    };
    constexpr std::size_t kVersionAndLockTime{
        4U + 4U
    };

    if (input_count >
            std::numeric_limits<std::uint64_t>::max() ||
        output_count >
            std::numeric_limits<std::uint64_t>::max()) {
        return std::nullopt;
    }

    std::size_t total =
        kVersionAndLockTime;

    const auto add_checked =
        [&](std::size_t value) -> bool {
            if (value >
                std::numeric_limits<std::size_t>::max() -
                    total) {
                return false;
            }

            total += value;
            return true;
        };

    if (!add_checked(
            compact_size_serialized_size(
                static_cast<std::uint64_t>(
                    input_count))) ||
        !add_checked(
            compact_size_serialized_size(
                static_cast<std::uint64_t>(
                    output_count)))) {
        return std::nullopt;
    }

    if (input_count >
            std::numeric_limits<std::size_t>::max() /
                kFixedInputBytes ||
        output_count >
            std::numeric_limits<std::size_t>::max() /
                kFixedOutputBytes) {
        return std::nullopt;
    }

    if (!add_checked(
            input_count *
            kFixedInputBytes) ||
        !add_checked(
            output_count *
            kFixedOutputBytes)) {
        return std::nullopt;
    }

    return total;
}

} // namespace quintum::wallet
