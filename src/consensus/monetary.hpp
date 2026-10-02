#pragma once

#include "primitives/transaction.hpp"

#include <cstdint>
#include <optional>

namespace quintum::consensus {

inline constexpr Amount kAtomicUnitsPerCoin = 100'000'000ULL;
inline constexpr Amount kMaxMoney = 21'000'000ULL * kAtomicUnitsPerCoin;
inline constexpr Amount kInitialSubsidy = 50ULL * kAtomicUnitsPerCoin;
inline constexpr std::uint32_t kSubsidyHalvingInterval = 210'000U;
inline constexpr std::uint32_t kCoinbaseMaturity = 100U;

[[nodiscard]] bool money_range(Amount value) noexcept;

[[nodiscard]] Amount block_subsidy(std::uint32_t height) noexcept;

[[nodiscard]] std::optional<Amount> transaction_output_total(
    const Transaction& tx
) noexcept;

[[nodiscard]] bool coinbase_reward_is_valid(
    const Transaction& coinbase,
    std::uint32_t height,
    Amount fees
) noexcept;

} // namespace quintum::consensus
