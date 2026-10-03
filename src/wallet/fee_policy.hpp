#pragma once

#include "node/mempool.hpp"
#include "policy/fees.hpp"

namespace quintum::wallet {

inline constexpr Amount kDefaultFeeRatePerKb{1'000U};

using quintum::policy::fee_for_size;

[[nodiscard]] Amount recommended_fee_rate(
    const Mempool& mempool
) noexcept;

} // namespace quintum::wallet
