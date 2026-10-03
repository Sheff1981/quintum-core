#pragma once

#include "node/mempool.hpp"

#include <cstddef>
#include <optional>

namespace quintum::wallet {

inline constexpr Amount kDefaultFeeRatePerKb{1'000U};

[[nodiscard]] std::optional<Amount> fee_for_size(
    std::size_t serialized_size,
    Amount rate_per_kb
) noexcept;

[[nodiscard]] Amount recommended_fee_rate(
    const Mempool& mempool
) noexcept;

} // namespace quintum::wallet
