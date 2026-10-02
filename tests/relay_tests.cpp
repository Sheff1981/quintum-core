#include "consensus/monetary.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "net/peer.hpp"
#include "net/relay.hpp"
#include "node/node.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>

namespace {

std::filesystem::path unique_dir(
    std::string_view suffix)
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    return std::filesystem::temp_directory_path() /
        ("quintum-stage18-" +
         std::string(suffix) + "-" +
         std::to_string(stamp));
}

quintum::crypto::PrivateKey private_key(
    quintum::Byte scalar)
{
    quintum::crypto::PrivateKey key{};
    key.back() = scalar;
    return key;
}

quintum::Bytes payout_script(
    quintum::Byte scalar)
{
    const auto public_key =
        quintum::crypto::derive_public_key(
            private_key(scalar)
        );

    assert(public_key.has_value());

    return quintum::consensus::make_p2pk_locking_script(
        *public_key
    );
}

quintum::net::VersionMessage version(
    std::uint64_t nonce,
    std::uint32_t height)
{
    return quintum::net::VersionMessage{
        .protocol_version =
            quintum::net::kProtocolVersion,
        .services = 1U,
        .timestamp = 1'790'990'000ULL,
        .nonce = nonce,
        .start_height = height,
    };
}

struct MatureCoin {
    quintum::Hash256 txid{};
    quintum::TxOutput output{};
};

MatureCoin build_shared_mature_chain(
    quintum::NodeRuntime& first,
    quintum::NodeRuntime& second,
    const quintum::Bytes& payout,
    std::uint64_t base_time)
{
    MatureCoin mature;

    for (std::uint32_t height = 1U;
         height <= 100U;
         ++height) {
        const auto mined =
            first.mine_block_at(
                payout,
                base_time +
                    static_cast<std::uint64_t>(height),
                4'096U
            );

        assert(mined.ok());
        assert(mined.height == height);

        if (height == 1U) {
            mature.txid =
                quintum::transaction_id(
                    mined.block.transactions.front()
                );
            mature.output =
                mined.block.transactions.front()
                    .outputs.front();
        }

        const auto submitted =
            second.submit_block_at(
                mined.block,
                base_time +
                    static_cast<std::uint64_t>(height)
            );

        assert(submitted.ok());
    }

    assert(first.chain().tip_hash() ==
           second.chain().tip_hash());
    assert(*first.chain().height() == 100U);
    assert(*second.chain().height() == 100U);

    return mature;
}

quintum::Transaction make_spend(
    const MatureCoin& coin,
    quintum::Byte key_scalar,
    quintum::Amount fee)
{
    using namespace quintum;

    Transaction spend;
    spend.inputs.push_back(
        TxInput{
            .previous_output = OutPoint{
                .txid = coin.txid,
                .index = 0U,
            },
        }
    );

    spend.outputs.push_back(
        TxOutput{
            .value = coin.output.value - fee,
            .locking_script =
                payout_script(key_scalar),
        }
    );

    assert(consensus::sign_p2pk_input(
               spend,
               0U,
               coin.output,
               private_key(key_scalar)) ==
           consensus::InputAuthError::none);

    return spend;
}

void test_local_mempool_validation_and_mining()
{
    using namespace quintum;

    const auto& params =
        consensus::regtest_params();
    const auto dir_a =
        unique_dir("local-a");
    const auto dir_b =
        unique_dir("local-b");
    const std::uint64_t now =
        params.genesis.timestamp + 30'000U;
    const auto payout = payout_script(1U);

    NodeRuntime node{params, dir_a};
    NodeRuntime mirror{params, dir_b};

    assert(node.start_at(now).ok());
    assert(mirror.start_at(now).ok());

    const auto coin =
        build_shared_mature_chain(
            node,
            mirror,
            payout,
            now
        );

    constexpr Amount fee{321U};
    auto spend =
        make_spend(coin, 1U, fee);

    const auto accepted =
        node.submit_transaction(spend);

    assert(accepted.ok());
    assert(accepted.mempool.fee == fee);
    assert(node.mempool().size() == 1U);
    assert(node.mempool().contains(
        transaction_id(spend)));

    const auto duplicate =
        node.submit_transaction(spend);

    assert(!duplicate.ok());
    assert(duplicate.mempool.error ==
           MempoolError::duplicate);

    auto conflict =
        make_spend(coin, 1U, fee + 1U);

    const auto conflict_result =
        node.submit_transaction(conflict);

    assert(!conflict_result.ok());
    assert(conflict_result.mempool.error ==
           MempoolError::transaction_rejected);
    assert(conflict_result.mempool.transaction_error ==
           UtxoApplyError::missing_input);

    auto oversized_script =
        make_spend(coin, 1U, fee + 2U);
    oversized_script.outputs.front().locking_script.assign(
        static_cast<std::size_t>(
            params.limits.max_script_bytes) + 1U,
        0x51U
    );
    assert(consensus::sign_p2pk_input(
               oversized_script,
               0U,
               coin.output,
               private_key(1U)) ==
           consensus::InputAuthError::none);

    const auto oversized_result =
        mirror.submit_transaction(
            oversized_script
        );

    assert(!oversized_result.ok());
    assert(oversized_result.mempool.error ==
           MempoolError::script_too_large);

    const auto mined =
        node.mine_mempool_block_at(
            payout,
            now + 1'000U,
            4'096U
        );

    assert(mined.ok());
    assert(mined.height == 101U);
    assert(mined.total_fees == fee);
    assert(mined.block.transactions.size() == 2U);
    assert(transaction_id(
               mined.block.transactions[1]) ==
           transaction_id(spend));
    assert(node.mempool().size() == 0U);
    assert(mined.block.transactions.front()
               .outputs.front().value ==
           consensus::block_subsidy(101U) + fee);

    std::error_code ec;
    std::filesystem::remove_all(dir_a, ec);
    std::filesystem::remove_all(dir_b, ec);
}

void test_live_transaction_and_block_relay()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto dir_a =
        unique_dir("relay-a");
    const auto dir_b =
        unique_dir("relay-b");
    const std::uint64_t now =
        params.genesis.timestamp + 40'000U;
    const auto payout = payout_script(2U);

    NodeRuntime node_a{params, dir_a};
    NodeRuntime node_b{params, dir_b};

    assert(node_a.start_at(now).ok());
    assert(node_b.start_at(now).ok());

    const auto coin =
        build_shared_mature_chain(
            node_a,
            node_b,
            payout,
            now
        );

    constexpr Amount fee{777U};
    const auto spend =
        make_spend(coin, 2U, fee);
    const auto txid =
        transaction_id(spend);

    assert(node_a.submit_transaction(spend).ok());
    assert(node_a.mempool().size() == 1U);
    assert(node_b.mempool().size() == 0U);

    PeerListener listener{params};
    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    RelayResult transaction_received;
    RelayResult block_received;
    PeerError server_handshake{
        PeerError::accept_failed
    };

    std::thread receiver([&] {
        auto accepted =
            listener.accept_and_handshake(
                version(0x1801U, 100U),
                5'000U
            );

        server_handshake = accepted.error;

        if (!accepted.ok()) {
            return;
        }

        transaction_received =
            receive_relay_once(
                *accepted.session,
                node_b,
                now + 2'000U
            );

        if (!transaction_received.ok()) {
            accepted.session->close();
            return;
        }

        block_received =
            receive_relay_once(
                *accepted.session,
                node_b,
                now + 2'001U
            );

        accepted.session->close();
    });

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            version(0x1802U, 100U),
            5'000U
        );

    assert(connected.ok());

    assert(announce_transaction(
               *connected.session,
               txid) ==
           PeerError::none);

    const auto served_tx =
        serve_relay_once(
            *connected.session,
            node_a
        );

    assert(served_tx.ok());

    const auto mined =
        node_a.mine_mempool_block_at(
            payout,
            now + 2'000U,
            4'096U
        );

    assert(mined.ok());
    assert(node_a.mempool().size() == 0U);

    const auto new_block_hash =
        block_hash(mined.block.header);

    assert(announce_block(
               *connected.session,
               new_block_hash) ==
           PeerError::none);

    const auto served_block =
        serve_relay_once(
            *connected.session,
            node_a
        );

    assert(served_block.ok());

    connected.session->close();
    receiver.join();

    assert(server_handshake == PeerError::none);
    assert(transaction_received.ok());
    assert(transaction_received.accepted_transaction ==
           std::optional<Hash256>{txid});
    assert(block_received.ok());
    assert(block_received.accepted_block ==
           std::optional<Hash256>{
               new_block_hash});

    assert(node_b.mempool().size() == 0U);
    assert(node_b.chain().height() ==
           node_a.chain().height());
    assert(node_b.chain().tip_hash() ==
           node_a.chain().tip_hash());

    NodeRuntime restarted_b{
        params,
        dir_b
    };

    assert(restarted_b.start_at(
        now + 3'000U).ok());
    assert(restarted_b.chain().tip_hash() ==
           node_a.chain().tip_hash());
    assert(*restarted_b.chain().height() == 101U);
    assert(restarted_b.mempool().size() == 0U);

    std::error_code ec;
    std::filesystem::remove_all(dir_a, ec);
    std::filesystem::remove_all(dir_b, ec);
}

void test_mempool_inventory_catchup()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto dir_a =
        unique_dir("mempool-a");
    const auto dir_b =
        unique_dir("mempool-b");
    const std::uint64_t now =
        params.genesis.timestamp + 50'000U;
    const auto payout = payout_script(3U);

    NodeRuntime node_a{params, dir_a};
    NodeRuntime node_b{params, dir_b};

    assert(node_a.start_at(now).ok());
    assert(node_b.start_at(now).ok());

    const auto coin =
        build_shared_mature_chain(
            node_a,
            node_b,
            payout,
            now
        );

    const auto spend =
        make_spend(coin, 3U, 222U);
    const auto txid =
        transaction_id(spend);

    assert(node_a.submit_transaction(spend).ok());

    PeerListener listener{params};
    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    RelayResult served_inventory;
    RelayResult served_transaction;

    std::thread server([&] {
        auto accepted =
            listener.accept_and_handshake(
                version(0x1811U, 100U),
                5'000U
            );

        if (!accepted.ok()) {
            served_inventory.error =
                RelayError::transport_failed;
            return;
        }

        served_inventory =
            serve_relay_once(
                *accepted.session,
                node_a
            );

        if (served_inventory.ok()) {
            served_transaction =
                serve_relay_once(
                    *accepted.session,
                    node_a
                );
        }

        accepted.session->close();
    });

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            version(0x1812U, 100U),
            5'000U
        );

    assert(connected.ok());

    const auto caught_up =
        sync_mempool_from_peer(
            *connected.session,
            node_b
        );

    connected.session->close();
    server.join();

    assert(caught_up.ok());
    assert(served_inventory.ok());
    assert(served_transaction.ok());
    assert(node_b.mempool().size() == 1U);
    assert(node_b.mempool().contains(txid));

    std::error_code ec;
    std::filesystem::remove_all(dir_a, ec);
    std::filesystem::remove_all(dir_b, ec);
}

} // namespace

int main()
{
    test_local_mempool_validation_and_mining();
    test_live_transaction_and_block_relay();
    test_mempool_inventory_catchup();
    return 0;
}
