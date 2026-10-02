#include "chain/chainstate.hpp"
#include "consensus/block_limits.hpp"
#include "consensus/chainparams.hpp"
#include "consensus/monetary.hpp"
#include "consensus/pow.hpp"
#include "primitives/block.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

quintum::Transaction make_coinbase(
    std::uint32_t height,
    quintum::Byte tag,
    std::size_t unlock_size = 6U,
    std::size_t output_script_size = 1U)
{
    quintum::Transaction tx;

    quintum::TxInput input;
    input.unlocking_script.assign(unlock_size, tag);
    if (!input.unlocking_script.empty()) {
        input.unlocking_script.front() = 0x01U;
    }
    tx.inputs.push_back(input);

    quintum::TxOutput output;
    output.value =
        quintum::consensus::block_subsidy(height);
    output.locking_script.assign(
        output_script_size,
        0x51U
    );
    tx.outputs.push_back(std::move(output));

    return tx;
}

quintum::Block make_block(
    const quintum::Hash256& previous,
    std::uint32_t height,
    std::uint64_t timestamp,
    quintum::Byte tag,
    bool mine = true)
{
    quintum::Block block;
    block.header.previous_block = previous;
    block.header.timestamp = timestamp;
    block.header.bits =
        quintum::consensus::regtest_params()
            .pow.pow_limit_bits;
    block.transactions.push_back(
        make_coinbase(height, tag)
    );
    quintum::update_merkle_root(block);

    if (mine) {
        const auto result =
            quintum::consensus::mine_header(
                block.header,
                100'000U
            );
        assert(result.found());
    }

    return block;
}

void test_median_time_past()
{
    quintum::Chainstate chain{
        quintum::consensus::regtest_params()
    };

    quintum::Hash256 previous{};
    constexpr std::uint64_t adjusted_time = 100'000U;

    for (std::uint32_t height = 0U;
         height < 11U;
         ++height) {
        const auto block = make_block(
            previous,
            height,
            100U + height,
            static_cast<quintum::Byte>(height + 1U)
        );

        const auto result =
            chain.connect_block(
                block,
                adjusted_time
            );

        assert(result.ok());
        previous =
            quintum::block_hash(block.header);
    }

    // Timestamps 100..110 have median 105.
    const auto equal_median = make_block(
        previous,
        11U,
        105U,
        0x40U
    );

    const auto old_result =
        chain.connect_block(
            equal_median,
            adjusted_time
        );

    assert(
        old_result.error ==
        quintum::ChainConnectError::timestamp_too_old
    );

    // Bitcoin-style MTP allows a time below the direct parent
    // as long as it is strictly greater than the median.
    const auto above_median = make_block(
        previous,
        11U,
        106U,
        0x41U
    );

    const auto good_result =
        chain.connect_block(
            above_median,
            adjusted_time
        );

    assert(good_result.ok());
    assert(good_result.activated);
}

void test_future_time_limit()
{
    const auto params =
        quintum::consensus::regtest_params();

    constexpr std::uint64_t adjusted_time = 10'000U;
    const auto maximum =
        adjusted_time +
        params.time.max_future_seconds;

    quintum::Hash256 zero{};

    {
        quintum::Chainstate chain{params};

        const auto too_future = make_block(
            zero,
            0U,
            maximum + 1U,
            0x50U
        );

        const auto result =
            chain.connect_block(
                too_future,
                adjusted_time
            );

        assert(
            result.error ==
            quintum::ChainConnectError::
                timestamp_too_far_future
        );
        assert(chain.empty());
    }

    {
        quintum::Chainstate chain{params};

        const auto boundary = make_block(
            zero,
            0U,
            maximum,
            0x51U
        );

        const auto result =
            chain.connect_block(
                boundary,
                adjusted_time
            );

        assert(result.ok());
        assert(result.activated);
    }
}

void test_resource_limits()
{
    const auto& params =
        quintum::consensus::regtest_params();

    {
        quintum::Block block;
        block.transactions.resize(
            static_cast<std::size_t>(
                params.limits.max_block_transactions) +
            1U
        );

        assert(
            quintum::consensus::validate_block_resources(
                block,
                params.limits
            ) ==
            quintum::consensus::BlockResourceError::
                too_many_transactions
        );
    }

    {
        quintum::Block block;
        block.transactions.push_back(
            make_coinbase(
                0U,
                0x60U,
                6U,
                static_cast<std::size_t>(
                    params.limits.max_script_bytes) +
                    1U
            )
        );

        assert(
            quintum::consensus::validate_block_resources(
                block,
                params.limits
            ) ==
            quintum::consensus::BlockResourceError::
                script_too_large
        );
    }

    {
        quintum::Block block;
        block.transactions.push_back(
            make_coinbase(
                0U,
                0x61U,
                static_cast<std::size_t>(
                    params.limits
                        .max_coinbase_script_bytes) +
                    1U,
                1U
            )
        );

        assert(
            quintum::consensus::validate_block_resources(
                block,
                params.limits
            ) ==
            quintum::consensus::BlockResourceError::
                coinbase_script_too_large
        );
    }

    {
        quintum::Block block;
        auto tx = make_coinbase(
            0U,
            0x62U
        );

        tx.outputs.clear();

        for (std::size_t i = 0U; i < 101U; ++i) {
            quintum::TxOutput output;
            output.value = 0U;
            output.locking_script.assign(
                9'999U,
                0x51U
            );
            tx.outputs.push_back(
                std::move(output)
            );
        }

        block.transactions.push_back(
            std::move(tx)
        );

        const auto size =
            quintum::serialized_block_size(block);

        assert(size);
        assert(
            *size >
            params.limits.max_block_serialized_bytes
        );

        assert(
            quintum::consensus::validate_block_resources(
                block,
                params.limits
            ) ==
            quintum::consensus::BlockResourceError::
                block_too_large
        );
    }
}

void test_chainstate_rejects_resource_violation()
{
    auto params =
        quintum::consensus::regtest_params();

    quintum::Chainstate chain{params};
    quintum::Hash256 zero{};

    auto block = make_block(
        zero,
        0U,
        1'000U,
        0x70U,
        false
    );

    block.transactions.front()
        .inputs.front()
        .unlocking_script.assign(
            static_cast<std::size_t>(
                params.limits
                    .max_coinbase_script_bytes) +
                1U,
            0x01U
        );

    quintum::update_merkle_root(block);

    const auto result =
        chain.connect_block(
            block,
            1'000U
        );

    assert(
        result.error ==
        quintum::ChainConnectError::
            resource_limits_exceeded
    );
    assert(
        result.resource_error ==
        quintum::consensus::BlockResourceError::
            coinbase_script_too_large
    );
    assert(chain.empty());
    assert(chain.block_index_size() == 0U);
}

} // namespace

int main()
{
    test_median_time_past();
    test_future_time_limit();
    test_resource_limits();
    test_chainstate_rejects_resource_violation();
    return 0;
}
