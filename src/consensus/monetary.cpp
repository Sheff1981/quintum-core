#include "consensus/monetary.hpp"

#include "consensus/tx_auth.hpp"

namespace quintum::consensus {

namespace {

constexpr MonetaryParams legacy_params() noexcept
{
    return MonetaryParams{};
}

} // namespace

bool money_range(Amount value) noexcept
{
    return money_range(
        value,
        legacy_params()
    );
}

bool money_range(
    Amount value,
    const MonetaryParams& params) noexcept
{
    return value <= params.max_money;
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

Amount block_subsidy(
    std::uint32_t height,
    const MonetaryParams& params) noexcept
{
    return params.schedule ==
                   MonetarySchedule::randomx_v1
        ? randomx_total_subsidy(height)
        : block_subsidy(height);
}

Amount founder_subsidy(
    std::uint32_t height,
    const MonetaryParams& params) noexcept
{
    return params.schedule ==
                   MonetarySchedule::randomx_v1
        ? randomx_founder_subsidy(height)
        : 0U;
}

Amount miner_subsidy(
    std::uint32_t height,
    const MonetaryParams& params) noexcept
{
    return params.schedule ==
                   MonetarySchedule::randomx_v1
        ? randomx_miner_subsidy(height)
        : block_subsidy(height);
}


std::optional<Bytes>
founder_payout_script(
    const MonetaryParams& params)
{
    if (!params.founder_payout_enabled) {
        return std::nullopt;
    }

    if (!params.founder_multisig_enabled) {
        if (!crypto::is_valid_public_key(
                params.founder_public_key)) {
            return std::nullopt;
        }

        return make_p2pk_locking_script(
            params.founder_public_key
        );
    }

    const std::size_t count =
        params.founder_multisig_key_count;

    if (count == 0U ||
        count >
            params.
                founder_multisig_public_keys.
                size() ||
        params.founder_multisig_threshold ==
            0U ||
        params.founder_multisig_threshold >
            count) {
        return std::nullopt;
    }

    return make_multisig_locking_script(
        params.founder_multisig_threshold,
        std::span<const crypto::PublicKey>{
            params.
                founder_multisig_public_keys.
                data(),
            count
        }
    );
}

std::optional<Amount> transaction_output_total(
    const Transaction& tx) noexcept
{
    return transaction_output_total(
        tx,
        legacy_params()
    );
}

std::optional<Amount> transaction_output_total(
    const Transaction& tx,
    const MonetaryParams& params) noexcept
{
    Amount total{0U};

    for (const auto& output : tx.outputs) {
        if (!money_range(
                output.value,
                params)) {
            return std::nullopt;
        }

        if (output.value >
            params.max_money - total) {
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
    return coinbase_reward_is_valid(
        coinbase,
        height,
        fees,
        legacy_params()
    );
}

bool coinbase_reward_is_valid(
    const Transaction& coinbase,
    std::uint32_t height,
    Amount fees,
    const MonetaryParams& params) noexcept
{
    if (!coinbase.is_coinbase() ||
        !money_range(fees, params)) {
        return false;
    }

    const auto output_total =
        transaction_output_total(
            coinbase,
            params
        );

    if (!output_total) {
        return false;
    }

    const Amount subsidy =
        block_subsidy(
            height,
            params
        );

    if (!money_range(subsidy, params) ||
        fees > params.max_money - subsidy) {
        return false;
    }

    const Amount allowed =
        subsidy + fees;

    if (*output_total > allowed) {
        return false;
    }

    if (params.schedule !=
        MonetarySchedule::randomx_v1) {
        return true;
    }

    const Amount founder =
        founder_subsidy(
            height,
            params
        );

    if (founder == 0U) {
        return true;
    }

    if (!params.founder_payout_enabled ||
        coinbase.outputs.size() < 2U ||
        coinbase.outputs[1].value != founder) {
        return false;
    }

    const auto expected_script =
        founder_payout_script(params);

    return expected_script.has_value() &&
           coinbase.outputs[1].
                   locking_script ==
               *expected_script;
}

} // namespace quintum::consensus
