#pragma once

#include "node/mempool.hpp"
#include "policy/fees.hpp"

namespace quintum::wallet {

inline constexpr Amount kDefaultFeeRatePerKb{1'000U};

using quintum::policy::fee_for_size;

[[nodiscard]] Amount recommended_fee_rate(
    const Mempool& mempool
) noexcept;

[[nodiscard]] std::optional<std::size_t>
estimate_p2pk_transaction_size(
    std::size_t input_count,
    std::size_t output_count
) noexcept;

} // namespace quintum::wallet
