#include "chain/storage.hpp"
#include "consensus/chainparams.hpp"
#include "consensus/monetary.hpp"
#include "consensus/pow.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "primitives/block.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

quintum::consensus::ChainParams storage_regtest_params()
{
    auto params = quintum::consensus::regtest_params();
    params.genesis.enforce = false;
    return params;
}

quintum::Transaction make_coinbase(
    std::uint32_t height,
    quintum::Byte tag)
{
    quintum::Transaction tx;

    quintum::TxInput input;
    input.unlocking_script = {
        0x01U,
        static_cast<quintum::Byte>(height & 0xffU),
        static_cast<quintum::Byte>((height >> 8U) & 0xffU),
        static_cast<quintum::Byte>((height >> 16U) & 0xffU),
        static_cast<quintum::Byte>((height >> 24U) & 0xffU),
        tag,
    };
    tx.inputs.push_back(std::move(input));

    tx.outputs.push_back(quintum::TxOutput{
        .value = quintum::consensus::block_subsidy(height),
        .locking_script = {0x01U, tag},
    });

    return tx;
}

quintum::Block make_block(
    const quintum::Hash256& previous,
    std::uint32_t height,
    quintum::Byte tag)
{
    quintum::Block block;
    block.header.previous_block = previous;
    block.header.timestamp = 1'700'000'000ULL + height;
    block.header.bits = 0x2100ffffU;
    block.transactions.push_back(
        make_coinbase(height, tag)
    );
    quintum::update_merkle_root(block);

    const auto mined =
        quintum::consensus::mine_header(
            block.header,
            10'000U);

    assert(mined.found());
    return block;
}

quintum::crypto::PrivateKey pruning_test_key()
{
    quintum::crypto::PrivateKey key{};
    key.back() = 1U;
    return key;
}

quintum::Bytes pruning_test_script()
{
    const auto public_key =
        quintum::crypto::derive_public_key(pruning_test_key());
    assert(public_key);
    return quintum::consensus::make_p2pk_locking_script(*public_key);
}

quintum::Block make_block_with_transactions(
    const quintum::Hash256& previous,
    std::uint32_t height,
    std::vector<quintum::Transaction> transactions)
{
    quintum::Block block;
    block.header.previous_block = previous;
    block.header.timestamp = 1'700'000'000ULL + height;
    block.header.bits = 0x2100ffffU;
    block.transactions = std::move(transactions);
    quintum::update_merkle_root(block);
    const auto mined =
        quintum::consensus::mine_header(block.header, 10'000U);
    assert(mined.found());
    return block;
}

std::filesystem::path fresh_directory(
    const char* name)
{
    const auto path =
        std::filesystem::temp_directory_path() /
        name;

    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    assert(!ec);

    std::filesystem::create_directories(path, ec);
    assert(!ec);

    return path;
}

void test_restart_side_branch_and_undo()
{
    const auto params =
        storage_regtest_params();
    const auto directory =
        fresh_directory(
            "quintum-storage-restart");

    quintum::Hash256 zero{};

    const auto genesis =
        make_block(zero, 0U, 0x10U);
    const auto genesis_hash =
        quintum::block_hash(genesis.header);

    const auto a1 =
        make_block(genesis_hash, 1U, 0x11U);
    const auto a1_hash =
        quintum::block_hash(a1.header);

    const auto a2 =
        make_block(a1_hash, 2U, 0x12U);
    const auto a2_hash =
        quintum::block_hash(a2.header);

    const auto b2 =
        make_block(a1_hash, 2U, 0x22U);
    const auto b2_hash =
        quintum::block_hash(b2.header);

    {
        quintum::PersistentChainstate node{
            params,
            directory
        };

        assert(
            node.load() ==
            quintum::StorageError::not_found
        );

        assert(node.connect_block(genesis).ok());
        assert(node.connect_block(a1).ok());
        assert(node.connect_block(a2).ok());

        const auto side =
            node.connect_block(b2);

        assert(side.ok());
        assert(!side.chain.activated);

        assert(
            node.chain().tip_hash() &&
            *node.chain().tip_hash() == a2_hash
        );
        assert(
            node.chain().height() &&
            *node.chain().height() == 2U
        );
        assert(
            node.chain().block_index_size() == 4U
        );
        assert(node.chain().utxos().size() == 3U);
    }

    quintum::Hash256 work_before{};

    {
        quintum::PersistentChainstate restarted{
            params,
            directory
        };

        assert(
            restarted.load() ==
            quintum::StorageError::none
        );
        assert(
            restarted.chain().tip_hash() &&
            *restarted.chain().tip_hash() ==
                a2_hash
        );
        assert(
            restarted.chain().height() &&
            *restarted.chain().height() == 2U
        );
        assert(
            restarted.chain().block_index_size() ==
                4U
        );
        assert(
            restarted.chain().utxos().size() == 3U
        );

        work_before =
            restarted.chain().cumulative_work();

        const auto b3 =
            make_block(b2_hash, 3U, 0x23U);
        const auto b3_hash =
            quintum::block_hash(b3.header);

        const auto reorg =
            restarted.connect_block(b3);

        assert(reorg.ok());
        assert(reorg.chain.activated);
        assert(reorg.chain.reorganized);
        assert(
            restarted.chain().tip_hash() &&
            *restarted.chain().tip_hash() ==
                b3_hash
        );
        assert(
            restarted.chain().height() &&
            *restarted.chain().height() == 3U
        );
        assert(
            restarted.chain().utxos().size() == 4U
        );
        assert(
            restarted.chain().cumulative_work() !=
                work_before
        );

        const auto disconnected =
            restarted.disconnect_tip();

        assert(disconnected.ok());
        assert(
            restarted.chain().tip_hash() &&
            *restarted.chain().tip_hash() ==
                b2_hash
        );
        assert(
            restarted.chain().height() &&
            *restarted.chain().height() == 2U
        );
        assert(
            restarted.chain().utxos().size() == 3U
        );
    }

    {
        quintum::PersistentChainstate restarted{
            params,
            directory
        };

        assert(
            restarted.load() ==
            quintum::StorageError::none
        );
        assert(
            restarted.chain().tip_hash() &&
            *restarted.chain().tip_hash() ==
                b2_hash
        );
        assert(
            restarted.chain().height() &&
            *restarted.chain().height() == 2U
        );
        assert(
            restarted.chain().block_index_size() ==
                5U
        );
        assert(
            restarted.chain().utxos().size() == 3U
        );
    }

    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
}

void test_uncommitted_block_tail_is_ignored()
{
    const auto params =
        storage_regtest_params();
    const auto directory =
        fresh_directory(
            "quintum-storage-tail");

    quintum::PersistentChainstate node{
        params,
        directory
    };

    quintum::Hash256 zero{};
    const auto genesis =
        make_block(zero, 0U, 0x30U);

    assert(node.connect_block(genesis).ok());

    {
        std::ofstream output(
            node.store().blocks_path(),
            std::ios::binary |
                std::ios::app);

        assert(output);
        const char garbage[] = {
            'B', 'A', 'D', 'T', 'A', 'I', 'L'
        };
        output.write(
            garbage,
            static_cast<std::streamsize>(
                sizeof(garbage))
        );
        assert(output);
    }

    quintum::PersistentChainstate restarted{
        params,
        directory
    };

    assert(
        restarted.load() ==
        quintum::StorageError::none
    );
    assert(
        restarted.chain().height() &&
        *restarted.chain().height() == 0U
    );

    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
}

void test_corruption_and_wrong_network_are_rejected()
{
    const auto params =
        storage_regtest_params();
    const auto directory =
        fresh_directory(
            "quintum-storage-corruption");

    quintum::PersistentChainstate node{
        params,
        directory
    };

    quintum::Hash256 zero{};
    const auto genesis =
        make_block(zero, 0U, 0x40U);

    assert(node.connect_block(genesis).ok());

    auto wrong_params = params;
    wrong_params.message_start[0] ^= 0xffU;

    quintum::PersistentChainstate wrong_network{
        wrong_params,
        directory
    };

    assert(
        wrong_network.load() ==
        quintum::StorageError::wrong_network
    );

    {
        std::fstream state(
            node.store().state_path(),
            std::ios::binary |
                std::ios::in |
                std::ios::out);

        assert(state);
        char byte{0};
        state.read(&byte, 1);
        assert(state);
        byte = static_cast<char>(
            static_cast<unsigned char>(byte) ^
            0x01U
        );
        state.seekp(0);
        state.write(&byte, 1);
        assert(state);
    }

    quintum::PersistentChainstate corrupted{
        params,
        directory
    };

    assert(
        corrupted.load() ==
        quintum::StorageError::checksum_mismatch
    );

    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
}

void test_truncated_committed_block_is_rejected()
{
    const auto params =
        storage_regtest_params();
    const auto directory =
        fresh_directory(
            "quintum-storage-truncated-block");

    quintum::PersistentChainstate node{
        params,
        directory
    };

    quintum::Hash256 zero{};
    const auto genesis =
        make_block(zero, 0U, 0x50U);

    assert(node.connect_block(genesis).ok());

    std::error_code ec;
    const auto size =
        std::filesystem::file_size(
            node.store().blocks_path(),
            ec);

    assert(!ec);
    assert(size > 0U);

    std::filesystem::resize_file(
        node.store().blocks_path(),
        size - 1U,
        ec);

    assert(!ec);

    quintum::PersistentChainstate truncated{
        params,
        directory
    };

    assert(
        truncated.load() ==
        quintum::StorageError::truncated
    );

    std::filesystem::remove_all(directory, ec);
}


void test_physical_prune_restart_and_continue()
{
    const auto params = storage_regtest_params();
    const auto directory =
        fresh_directory("quintum-storage-physical-prune");
    const quintum::PrunePolicy policy{
        .enabled = true,
        .keep_recent_blocks = 2U,
    };

    quintum::Hash256 tip{};
    {
        quintum::PersistentChainstate node{
            params, directory, policy};

        quintum::Hash256 zero{};
        const auto genesis =
            make_block(zero, 0U, 0x70U);
        const auto h0 =
            quintum::block_hash(genesis.header);
        const auto b1 =
            make_block(h0, 1U, 0x71U);
        const auto h1 =
            quintum::block_hash(b1.header);
        const auto b2 =
            make_block(h1, 2U, 0x72U);
        const auto h2 =
            quintum::block_hash(b2.header);
        const auto b3 =
            make_block(h2, 3U, 0x73U);

        assert(node.connect_block(genesis).ok());
        assert(node.connect_block(b1).ok());
        assert(node.connect_block(b2).ok());
        assert(node.connect_block(b3).ok());

        tip = quintum::block_hash(b3.header);
        assert(node.chain().block(h0) == nullptr);
        assert(node.chain().block(h1) == nullptr);
    }

    {
        quintum::PersistentChainstate restarted{
            params, directory, policy};
        assert(
            restarted.load() ==
            quintum::StorageError::none);
        assert(restarted.chain().height());
        assert(*restarted.chain().height() == 3U);
        assert(restarted.chain().tip_hash());
        assert(*restarted.chain().tip_hash() == tip);
        assert(restarted.chain().utxos().size() == 4U);

        const auto b4 =
            make_block(tip, 4U, 0x74U);
        assert(restarted.connect_block(b4).ok());
        assert(restarted.chain().height());
        assert(*restarted.chain().height() == 4U);

        // A successful second pruned commit switches the snapshot to
        // generation 5 and may only then delete generation 4.
        assert(!std::filesystem::exists(
            directory / "blocks.4.dat"));
        assert(std::filesystem::exists(
            directory / "blocks.5.dat"));
    }

    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
}

void test_spend_utxo_from_pruned_block_after_restart()
{
    const auto params = storage_regtest_params();
    const auto directory =
        fresh_directory("quintum-storage-pruned-utxo-spend");
    const quintum::PrunePolicy policy{
        .enabled = true,
        .keep_recent_blocks = 2U,
    };

    quintum::Transaction funding_coinbase;
    funding_coinbase.inputs.push_back(quintum::TxInput{
        .unlocking_script = {0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0xa0U},
    });
    funding_coinbase.outputs.push_back(quintum::TxOutput{
        .value = quintum::consensus::block_subsidy(0U),
        .locking_script = pruning_test_script(),
    });

    quintum::Hash256 tip{};
    const auto funding_txid =
        quintum::transaction_id(funding_coinbase);

    {
        quintum::PersistentChainstate node{
            params, directory, policy};
        quintum::Hash256 zero{};
        const auto genesis = make_block_with_transactions(
            zero, 0U, {funding_coinbase});
        const auto genesis_hash =
            quintum::block_hash(genesis.header);
        assert(node.connect_block(genesis).ok());
        tip = genesis_hash;

        for (std::uint32_t height = 1U;
             height <= 100U;
             ++height) {
            const auto block =
                make_block(tip, height,
                           static_cast<quintum::Byte>(height));
            assert(node.connect_block(block).ok());
            tip = quintum::block_hash(block.header);
        }

        assert(node.chain().block(genesis_hash) == nullptr);
        assert(node.chain().utxos().contains(
            quintum::OutPoint{.txid = funding_txid, .index = 0U}));
    }

    {
        quintum::PersistentChainstate restarted{
            params, directory, policy};
        assert(restarted.load() == quintum::StorageError::none);

        const quintum::OutPoint funding{
            .txid = funding_txid,
            .index = 0U,
        };
        assert(restarted.chain().utxos().contains(funding));

        quintum::Transaction spend;
        spend.inputs.push_back(quintum::TxInput{
            .previous_output = funding,
        });
        spend.outputs.push_back(quintum::TxOutput{
            .value = funding_coinbase.outputs.front().value - 100U,
            .locking_script = pruning_test_script(),
        });
        assert(
            quintum::consensus::sign_p2pk_input(
                spend,
                0U,
                funding_coinbase.outputs.front(),
                pruning_test_key()) ==
            quintum::consensus::InputAuthError::none);

        const auto block101 = make_block_with_transactions(
            tip,
            101U,
            {
                make_coinbase(101U, 0xb1U),
                spend,
            });
        const auto result =
            restarted.connect_block(block101);
        assert(result.ok());
        assert(result.chain.total_fees == 100U);
        assert(!restarted.chain().utxos().contains(funding));
    }

    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
}

void test_pruned_reorg_within_retained_window()
{
    const auto params = storage_regtest_params();
    const auto directory =
        fresh_directory("quintum-storage-pruned-reorg");
    const quintum::PrunePolicy policy{
        .enabled = true,
        .keep_recent_blocks = 3U,
    };

    quintum::Hash256 zero{};
    const auto genesis = make_block(zero, 0U, 0x80U);
    const auto h0 = quintum::block_hash(genesis.header);
    const auto a1 = make_block(h0, 1U, 0x81U);
    const auto h1 = quintum::block_hash(a1.header);
    const auto a2 = make_block(h1, 2U, 0x82U);
    const auto h2 = quintum::block_hash(a2.header);
    const auto a3 = make_block(h2, 3U, 0x83U);
    const auto h3 = quintum::block_hash(a3.header);
    const auto b2 = make_block(h1, 2U, 0x92U);
    const auto bh2 = quintum::block_hash(b2.header);
    const auto b3 = make_block(bh2, 3U, 0x93U);
    const auto bh3 = quintum::block_hash(b3.header);
    const auto b4 = make_block(bh3, 4U, 0x94U);
    const auto bh4 = quintum::block_hash(b4.header);

    {
        quintum::PersistentChainstate node{
            params, directory, policy};
        assert(node.connect_block(genesis).ok());
        assert(node.connect_block(a1).ok());
        assert(node.connect_block(a2).ok());
        assert(node.connect_block(a3).ok());
        assert(node.connect_block(b2).ok());
        assert(node.connect_block(b3).ok());
        assert(node.chain().tip_hash());
        assert(*node.chain().tip_hash() == h3);
    }

    {
        quintum::PersistentChainstate restarted{
            params, directory, policy};
        assert(restarted.load() == quintum::StorageError::none);
        assert(restarted.chain().block(h0) == nullptr);
        assert(restarted.chain().block(h1) != nullptr);
        const auto reorg = restarted.connect_block(b4);
        assert(reorg.ok());
        assert(reorg.chain.activated);
        assert(reorg.chain.reorganized);
        assert(restarted.chain().tip_hash());
        assert(*restarted.chain().tip_hash() == bh4);
        assert(restarted.chain().height());
        assert(*restarted.chain().height() == 4U);
    }

    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
}

void test_prune_policy_boundary()
{
    const auto params = storage_regtest_params();
    const auto directory =
        fresh_directory("quintum-storage-prune-policy");

    quintum::PersistentChainstate node{
        params,
        directory,
        quintum::PrunePolicy{
            .enabled = true,
            .keep_recent_blocks = 2U,
        }
    };

    quintum::Hash256 zero{};
    const auto genesis = make_block(zero, 0U, 0x60U);
    const auto h0 = quintum::block_hash(genesis.header);
    const auto b1 = make_block(h0, 1U, 0x61U);
    const auto h1 = quintum::block_hash(b1.header);
    const auto b2 = make_block(h1, 2U, 0x62U);

    assert(node.connect_block(genesis).ok());
    assert(node.connect_block(b1).ok());
    assert(node.connect_block(b2).ok());

    const auto status =
        node.store().prune_status(node.chain());

    assert(status.enabled);
    assert(status.keep_recent_blocks == 2U);
    assert(status.prune_height);
    assert(*status.prune_height == 0U);

    quintum::PersistentChainstate restarted{
        params,
        directory,
        quintum::PrunePolicy{
            .enabled = true,
            .keep_recent_blocks = 2U,
        }
    };

    assert(restarted.load() == quintum::StorageError::none);
    assert(restarted.chain().height());
    assert(*restarted.chain().height() == 2U);

    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
}

} // namespace

int main()
{
    test_restart_side_branch_and_undo();
    test_uncommitted_block_tail_is_ignored();
    test_corruption_and_wrong_network_are_rejected();
    test_truncated_committed_block_is_rejected();
    test_prune_policy_boundary();
    test_physical_prune_restart_and_continue();
    return 0;
}
