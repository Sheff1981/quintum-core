#include "consensus/monetary.hpp"

#include <limits>

namespace quintum::consensus {

bool money_range(Amount value) noexcept
{
    return value <= kMaxMoney;
}

Amount block_subsidy(std::uint32_t height) noexcept
{
    const std::uint32_t halvings =
        height / kSubsidyHalvingInterval;

    if (halvings >= 64U) {
        return 0U;
    }

    return kInitialSubsidy >> halvings;
}

Amount randomx_total_subsidy(
    std::uint32_t height) noexcept
{
    if (height == 0U) {
        return 0U;
    }

    if (height > kRandomXPrimaryEndHeight) {
        return kRandomXTailSubsidy;
    }

    const std::uint32_t era =
        (height - 1U) / kRandomXEraBlocks;

    return kRandomXInitialSubsidy >> era;
}

Amount randomx_founder_subsidy(
    std::uint32_t height) noexcept
{
    if (height == 0U ||
        height > kRandomXPrimaryEndHeight) {
        return 0U;
    }

    const Amount subsidy =
        randomx_total_subsidy(height);

    return (
        subsidy *
        static_cast<Amount>(
            kRandomXFounderBasisPoints)
    ) /
        static_cast<Amount>(
            kBasisPointsDenominator);
}

Amount randomx_miner_subsidy(
    std::uint32_t height) noexcept
{
    const Amount total =
        randomx_total_subsidy(height);

    return total -
           randomx_founder_subsidy(height);
}

std::optional<Amount> transaction_output_total(
    const Transaction& tx) noexcept
{
    Amount total{0U};

    for (const auto& output : tx.outputs) {
        if (!money_range(output.value)) {
            return std::nullopt;
        }

        if (output.value > kMaxMoney - total) {
            return std::nullopt;
        }

        total += output.value;
    }

    return total;
}

bool coinbase_reward_is_valid(
    const Transaction& coinbase,
    std::uint32_t height,
    Amount fees) noexcept
{
    if (!coinbase.is_coinbase() || !money_range(fees)) {
        return false;
    }

    const auto output_total =
        transaction_output_total(coinbase);

    if (!output_total) {
        return false;
    }

    const Amount subsidy = block_subsidy(height);

    if (fees > kMaxMoney - subsidy) {
        return false;
    }

    const Amount allowed = subsidy + fees;
    return *output_total <= allowed;
}

} // namespace quintum::consensus
