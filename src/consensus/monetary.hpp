#pragma once

#include "crypto/secp256k1.hpp"
#include "primitives/transaction.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace quintum::consensus {

inline constexpr Amount kAtomicUnitsPerCoin = 100'000'000ULL;

// Legacy SHA-256 networks retain their original 21M transaction-money
// range. This is a per-transaction arithmetic/consensus bound, not a claim
// about future RandomX lifetime supply.
inline constexpr Amount kLegacyMaxMoney =
    21'000'000ULL * kAtomicUnitsPerCoin;
inline constexpr Amount kMaxMoney =
    kLegacyMaxMoney;

inline constexpr Amount kInitialSubsidy =
    50ULL * kAtomicUnitsPerCoin;
inline constexpr std::uint32_t kSubsidyHalvingInterval =
    210'000U;
inline constexpr std::uint32_t kCoinbaseMaturity =
    100U;

// RandomX Testnet monetary schedule.
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

// Safety/range bound for a single RandomX transaction/value sum.
// It is deliberately much larger than scheduled supply and is NOT a
// total-supply cap. Tail emission remains governed only by subsidy rules.
inline constexpr Amount kRandomXMoneyRange =
    100'000'000'000ULL * kAtomicUnitsPerCoin;

inline constexpr Amount kRandomXPrimaryIssuance =
    98'437'500ULL * kAtomicUnitsPerCoin;
inline constexpr Amount kRandomXFounderPrimaryIssuance =
    4'921'875ULL * kAtomicUnitsPerCoin;
inline constexpr Amount kRandomXMinerPrimaryIssuance =
    93'515'625ULL * kAtomicUnitsPerCoin;

enum class MonetarySchedule {
    legacy_halving,
    randomx_v1,
};

struct MonetaryParams {
    MonetarySchedule schedule{
        MonetarySchedule::legacy_halving
    };
    Amount max_money{kLegacyMaxMoney};
    std::uint32_t coinbase_maturity{
        kCoinbaseMaturity
    };
    bool founder_payout_enabled{false};
    crypto::PublicKey founder_public_key{};

    // Mainnet custody can use a consensus-pinned m-of-n payout without
    // changing the legacy/testnet single-key format. Disabled by default so
    // all existing chains retain byte-for-byte founder output scripts.
    bool founder_multisig_enabled{false};
    std::uint8_t founder_multisig_threshold{0U};
    std::uint8_t founder_multisig_key_count{0U};
    std::array<crypto::PublicKey, 5>
        founder_multisig_public_keys{};
};

[[nodiscard]] bool money_range(
    Amount value
) noexcept;

[[nodiscard]] bool money_range(
    Amount value,
    const MonetaryParams& params
) noexcept;

[[nodiscard]] Amount block_subsidy(
    std::uint32_t height
) noexcept;

[[nodiscard]] Amount block_subsidy(
    std::uint32_t height,
    const MonetaryParams& params
) noexcept;

[[nodiscard]] Amount randomx_total_subsidy(
    std::uint32_t height
) noexcept;

[[nodiscard]] Amount randomx_founder_subsidy(
    std::uint32_t height
) noexcept;

[[nodiscard]] Amount randomx_miner_subsidy(
    std::uint32_t height
) noexcept;

[[nodiscard]] Amount founder_subsidy(
    std::uint32_t height,
    const MonetaryParams& params
) noexcept;

[[nodiscard]] Amount miner_subsidy(
    std::uint32_t height,
    const MonetaryParams& params
) noexcept;

[[nodiscard]] std::optional<Bytes>
founder_payout_script(
    const MonetaryParams& params
);

[[nodiscard]] std::optional<Amount>
transaction_output_total(
    const Transaction& tx
) noexcept;

[[nodiscard]] std::optional<Amount>
transaction_output_total(
    const Transaction& tx,
    const MonetaryParams& params
) noexcept;

[[nodiscard]] bool coinbase_reward_is_valid(
    const Transaction& coinbase,
    std::uint32_t height,
    Amount fees
) noexcept;

[[nodiscard]] bool coinbase_reward_is_valid(
    const Transaction& coinbase,
    std::uint32_t height,
    Amount fees,
    const MonetaryParams& params
) noexcept;

} // namespace quintum::consensus
