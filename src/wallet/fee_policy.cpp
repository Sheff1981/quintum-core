#include "wallet/fee_policy.hpp"

#include <algorithm>
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

} // namespace quintum::wallet
