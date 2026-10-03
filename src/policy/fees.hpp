#pragma once

#include "primitives/transaction.hpp"

#include <cstddef>
#include <optional>

namespace quintum::policy {

inline constexpr Amount kDefaultMinRelayFeeRatePerKb{1'000U};

[[nodiscard]] std::optional<Amount> fee_for_size(
    std::size_t serialized_size,
    Amount rate_per_kb
) noexcept;

[[nodiscard]] std::optional<Amount> fee_rate_for_size(
    Amount fee,
    std::size_t serialized_size
) noexcept;

} // namespace quintum::policy
