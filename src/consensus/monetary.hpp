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

// Next incompatible RandomX Testnet monetary schedule.
// These constants are deliberately separate from the currently active
// legacy SHA-256 Testnet schedule until the new network is activated.
inline constexpr Amount kRandomXInitialSubsidy =
    50ULL * kAtomicUnitsPerCoin;
inline constexpr std::uint32_t kRandomXEraBlocks =
    1'000'000U;
inline constexpr std::uint32_t kRandomXPrimaryEras =
    6U;
inline constexpr std::uint32_t kRandomXPrimaryEndHeight =
    kRandomXEraBlocks * kRandomXPrimaryEras;
inline constexpr Amount kRandomXTailSubsidy =
    1ULL * kAtomicUnitsPerCoin;
inline constexpr std::uint32_t kRandomXFounderBasisPoints =
    500U;
inline constexpr std::uint32_t kBasisPointsDenominator =
    10'000U;
inline constexpr std::uint32_t kRandomXCoinbaseMaturity =
    500U;

inline constexpr Amount kRandomXPrimaryIssuance =
    98'437'500ULL * kAtomicUnitsPerCoin;
inline constexpr Amount kRandomXFounderPrimaryIssuance =
    4'921'875ULL * kAtomicUnitsPerCoin;
inline constexpr Amount kRandomXMinerPrimaryIssuance =
    93'515'625ULL * kAtomicUnitsPerCoin;

[[nodiscard]] bool money_range(Amount value) noexcept;

[[nodiscard]] Amount block_subsidy(std::uint32_t height) noexcept;

[[nodiscard]] Amount randomx_total_subsidy(
    std::uint32_t height
) noexcept;

[[nodiscard]] Amount randomx_founder_subsidy(
    std::uint32_t height
) noexcept;

[[nodiscard]] Amount randomx_miner_subsidy(
    std::uint32_t height
) noexcept;

[[nodiscard]] std::optional<Amount> transaction_output_total(
    const Transaction& tx
) noexcept;

[[nodiscard]] bool coinbase_reward_is_valid(
    const Transaction& coinbase,
    std::uint32_t height,
    Amount fees
) noexcept;

} // namespace quintum::consensus
