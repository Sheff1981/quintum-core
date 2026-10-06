#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "net/peer.hpp"
#include "net/sync.hpp"
#include "node/node.hpp"

#include <array>
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
        ("quintum-stage17-" +
         std::string(suffix) + "-" +
         std::to_string(stamp));
}

quintum::Bytes payout_script(
    quintum::Byte scalar)
{
    quintum::crypto::PrivateKey key{};
    key.back() = scalar;

    const auto public_key =
        quintum::crypto::derive_public_key(key);

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
        .timestamp = 1'790'980'000ULL,
        .nonce = nonce,
        .start_height = height,
    };
}

void mine_blocks(
    quintum::NodeRuntime& node,
    const quintum::Bytes& payout,
    std::uint64_t base_time,
    std::uint32_t count)
{
    for (std::uint32_t i = 0U; i < count; ++i) {
        const auto mined =
            node.mine_block_at(
                payout,
                base_time +
                    static_cast<std::uint64_t>(i) +
                    1U,
                4'096U
            );

        assert(mined.ok());
    }
}

void serve_requests(
    quintum::net::PeerListener& listener,
    const quintum::Chainstate& chain,
    quintum::net::VersionMessage local,
    std::size_t requests,
    quintum::net::SyncError& error)
{
    auto accepted =
        listener.accept_and_handshake(
            local,
            5'000U
        );

    if (!accepted.ok()) {
        error =
            quintum::net::SyncError::transport_failed;
        return;
    }

    for (std::size_t i = 0U;
         i < requests;
         ++i) {
        const auto served =
            quintum::net::serve_sync_once(
                *accepted.session,
                chain
            );

        if (!served.ok()) {
            error = served.error;
            accepted.session->close();
            return;
        }
    }

    accepted.session->close();
    error = quintum::net::SyncError::none;
}

void test_wire_codecs_and_locator()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto dir =
        unique_dir("wire");

    NodeRuntime node{params, dir};
    assert(node.start_at(
        params.genesis.timestamp + 100U).ok());

    mine_blocks(
        node,
        payout_script(1U),
        params.genesis.timestamp + 100U,
        3U
    );

    const auto locator =
        build_block_locator(node.chain());

    assert(!locator.empty());
    assert(locator.front() ==
           *node.chain().tip_hash());
    assert(locator.back() ==
           params.genesis.hash);

    const GetHeadersRequest request{
        .locator = locator,
        .stop = {},
    };

    const auto request_bytes =
        serialize_getheaders(request);
    const auto parsed_request =
        parse_getheaders(request_bytes);

    assert(parsed_request.has_value());
    assert(parsed_request->locator == locator);

    std::array<BlockHeader, 2> headers{
        *node.chain().active_header(1U),
        *node.chain().active_header(2U),
    };

    const auto header_bytes =
        serialize_headers(headers);
    const auto parsed_headers =
        parse_headers(header_bytes);

    assert(parsed_headers.has_value());
    assert(parsed_headers->size() == 2U);
    assert(block_hash((*parsed_headers)[0]) ==
           block_hash(headers[0]));
    assert(block_hash((*parsed_headers)[1]) ==
           block_hash(headers[1]));

    const std::array<InventoryItem, 1> inv{
        InventoryItem{
            .type = kInventoryBlock,
            .hash = block_hash(headers[1]),
        }
    };

    const auto inv_bytes =
        serialize_inventory(inv);
    const auto parsed_inv =
        parse_inventory(inv_bytes);

    assert(parsed_inv.has_value());
    assert(parsed_inv->size() == 1U);
    assert(parsed_inv->front().type ==
           kInventoryBlock);
    assert(parsed_inv->front().hash ==
           inv.front().hash);

    const Block* block =
        node.chain().block(inv.front().hash);
    assert(block != nullptr);

    const auto block_bytes =
        serialize_block_payload(*block);
    const auto parsed_block =
        parse_block_payload(
            block_bytes,
            params.limits
        );

    assert(parsed_block.has_value());
    assert(block_hash(parsed_block->header) ==
           inv.front().hash);

    auto malformed_headers = header_bytes;
    malformed_headers.push_back(0U);
    assert(!parse_headers(
        malformed_headers).has_value());

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void test_genesis_to_tip_sync_and_restart()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto server_dir =
        unique_dir("server");
    const auto client_dir =
        unique_dir("client");

    const std::uint64_t now =
        params.genesis.timestamp + 10'000U;

    NodeRuntime server{params, server_dir};
    NodeRuntime client{params, client_dir};

    assert(server.start_at(now).ok());
    assert(client.start_at(now).ok());

    mine_blocks(
        server,
        payout_script(2U),
        now,
        5U
    );

    assert(*server.chain().height() == 5U);
    assert(*client.chain().height() == 0U);

    PeerListener listener{params};
    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    SyncError server_error{
        SyncError::transport_failed
    };

    std::thread server_thread([&] {
        serve_requests(
            listener,
            server.chain(),
            version(0x1701U, 5U),
            2U,
            server_error
        );
    });

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            version(0x1702U, 0U),
            5'000U
        );

    assert(connected.ok());

    const auto synced =
        sync_from_peer(
            *connected.session,
            client,
            now + 100U
        );

    assert(synced.ok());
    assert(synced.headers_received == 5U);
    assert(synced.blocks_requested == 5U);
    assert(synced.block_request_batches == 1U);
    assert(synced.blocks_accepted == 5U);
    assert(!synced.reorganized);

    connected.session->close();
    server_thread.join();

    assert(server_error == SyncError::none);
    assert(client.chain().height() ==
           server.chain().height());
    assert(client.chain().tip_hash() ==
           server.chain().tip_hash());

    const auto expected_tip =
        *server.chain().tip_hash();

    NodeRuntime restarted{
        params,
        client_dir
    };

    assert(restarted.start_at(
        now + 200U).ok());
    assert(*restarted.chain().height() == 5U);
    assert(restarted.chain().tip_hash() ==
           expected_tip);

    std::error_code ec;
    std::filesystem::remove_all(server_dir, ec);
    std::filesystem::remove_all(client_dir, ec);
}

void test_heavier_remote_branch_reorg()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto server_dir =
        unique_dir("fork-server");
    const auto client_dir =
        unique_dir("fork-client");

    const std::uint64_t now =
        params.genesis.timestamp + 20'000U;

    NodeRuntime server{params, server_dir};
    NodeRuntime client{params, client_dir};

    assert(server.start_at(now).ok());
    assert(client.start_at(now).ok());

    mine_blocks(
        client,
        payout_script(3U),
        now,
        2U
    );

    mine_blocks(
        server,
        payout_script(4U),
        now,
        4U
    );

    const auto old_tip =
        *client.chain().tip_hash();

    assert(*client.chain().height() == 2U);
    assert(*server.chain().height() == 4U);
    assert(client.chain().tip_hash() !=
           server.chain().tip_hash());

    PeerListener listener{params};
    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    SyncError server_error{
        SyncError::transport_failed
    };

    std::thread server_thread([&] {
        serve_requests(
            listener,
            server.chain(),
            version(0x1711U, 4U),
            2U,
            server_error
        );
    });

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            version(0x1712U, 2U),
            5'000U
        );

    assert(connected.ok());

    const auto synced =
        sync_from_peer(
            *connected.session,
            client,
            now + 100U
        );

    assert(synced.ok());
    assert(synced.headers_received == 4U);
    assert(synced.blocks_requested == 4U);
    assert(synced.block_request_batches == 1U);
    assert(synced.blocks_accepted == 4U);
    assert(synced.reorganized);

    connected.session->close();
    server_thread.join();

    assert(server_error == SyncError::none);
    assert(*client.chain().height() == 4U);
    assert(client.chain().tip_hash() ==
           server.chain().tip_hash());
    assert(client.chain().tip_hash() !=
           std::optional<Hash256>{old_tip});
    assert(client.chain().has_block(old_tip));

    NodeRuntime restarted{
        params,
        client_dir
    };
    assert(restarted.start_at(
        now + 200U).ok());
    assert(*restarted.chain().height() == 4U);
    assert(restarted.chain().tip_hash() ==
           server.chain().tip_hash());

    std::error_code ec;
    std::filesystem::remove_all(server_dir, ec);
    std::filesystem::remove_all(client_dir, ec);
}


void test_pruned_deep_reorg_redownloads_missing_bodies()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto server_dir =
        unique_dir("pruned-deep-reorg-server");
    const auto client_dir =
        unique_dir("pruned-deep-reorg-client");

    const PrunePolicy policy{
        .enabled = true,
        .keep_recent_blocks = 2U,
    };

    const std::uint64_t now =
        params.genesis.timestamp + 40'000U;

    NodeRuntime server{params, server_dir};
    NodeRuntime client{
        params,
        client_dir,
        policy
    };

    assert(server.start_at(now).ok());
    assert(client.start_at(now).ok());

    // Establish an identical chain through height 3.
    mine_blocks(
        server,
        payout_script(5U),
        now,
        3U
    );

    {
        PeerListener listener{params};
        assert(listener.listen(
                   "127.0.0.1",
                   0U) ==
               PeerError::none);

        SyncError server_error{
            SyncError::transport_failed
        };

        std::thread server_thread([&] {
            serve_requests(
                listener,
                server.chain(),
                version(0x1721U, 3U),
                2U,
                server_error
            );
        });

        auto connected =
            connect_and_handshake(
                params,
                "127.0.0.1",
                listener.local_port(),
                version(0x1722U, 0U),
                5'000U
            );
        assert(connected.ok());

        const auto synced =
            sync_from_peer(
                *connected.session,
                client,
                now + 50U
            );
        assert(synced.ok());
        assert(synced.block_request_batches == 1U);
        assert(synced.blocks_accepted == 3U);

        connected.session->close();
        server_thread.join();
        assert(server_error == SyncError::none);
    }

    assert(client.chain().tip_hash() ==
           server.chain().tip_hash());

    // Client builds branch A to height 8. Server builds branch B only to
    // height 6 first, so B remains a side branch on the client.
    mine_blocks(
        client,
        payout_script(6U),
        now + 100U,
        5U
    );
    mine_blocks(
        server,
        payout_script(7U),
        now + 200U,
        3U
    );

    assert(client.chain().height() &&
           *client.chain().height() == 8U);
    assert(server.chain().height() &&
           *server.chain().height() == 6U);

    std::array<Hash256, 3> old_side_hashes{};
    for (std::uint32_t height = 4U;
         height <= 6U;
         ++height) {
        const auto hash =
            server.chain().active_hash(height);
        assert(hash);
        old_side_hashes[
            static_cast<std::size_t>(
                height - 4U)] = *hash;
    }

    {
        PeerListener listener{params};
        assert(listener.listen(
                   "127.0.0.1",
                   0U) ==
               PeerError::none);

        SyncError server_error{
            SyncError::transport_failed
        };

        std::thread server_thread([&] {
            serve_requests(
                listener,
                server.chain(),
                version(0x1731U, 6U),
                2U,
                server_error
            );
        });

        auto connected =
            connect_and_handshake(
                params,
                "127.0.0.1",
                listener.local_port(),
                version(0x1732U, 8U),
                5'000U
            );
        assert(connected.ok());

        const auto synced =
            sync_from_peer(
                *connected.session,
                client,
                now + 350U
            );

        assert(synced.ok());
        assert(synced.headers_received == 3U);
        assert(synced.blocks_requested == 3U);
        assert(synced.block_request_batches == 1U);
        assert(synced.blocks_accepted == 3U);
        assert(!synced.reorganized);

        connected.session->close();
        server_thread.join();
        assert(server_error == SyncError::none);
    }

    for (const auto& hash : old_side_hashes) {
        assert(client.chain().has_block(hash));
        assert(!client.chain().has_block_body(hash));
    }

    // Restart deliberately drops the transient recovery cache. The block
    // metadata remains on disk, but the old side-branch bodies are gone.
    {
        NodeRuntime restarted{
            params,
            client_dir,
            policy
        };
        assert(restarted.start_at(
            now + 400U).ok());

        for (const auto& hash :
             old_side_hashes) {
            assert(
                restarted.chain().
                    has_block(hash));
            assert(
                !restarted.chain().
                    has_block_body(hash));
        }

        // Branch B now becomes heavier than branch A.
        mine_blocks(
            server,
            payout_script(7U),
            now + 500U,
            3U
        );
        assert(server.chain().height() &&
               *server.chain().height() == 9U);

        PeerListener listener{params};
        assert(listener.listen(
                   "127.0.0.1",
                   0U) ==
               PeerError::none);

        SyncError server_error{
            SyncError::transport_failed
        };

        std::thread server_thread([&] {
            serve_requests(
                listener,
                server.chain(),
                version(0x1741U, 9U),
                2U,
                server_error
            );
        });

        auto connected =
            connect_and_handshake(
                params,
                "127.0.0.1",
                listener.local_port(),
                version(0x1742U, 8U),
                5'000U
            );
        assert(connected.ok());

        const auto synced =
            sync_from_peer(
                *connected.session,
                restarted,
                now + 600U
            );

        assert(synced.ok());
        assert(synced.headers_received == 6U);
        assert(synced.blocks_requested == 6U);
        assert(synced.block_request_batches == 1U);
        assert(synced.block_bodies_restored == 3U);
        assert(synced.blocks_accepted == 3U);
        assert(synced.reorganized);

        connected.session->close();
        server_thread.join();
        assert(server_error == SyncError::none);

        assert(restarted.chain().height() ==
               server.chain().height());
        assert(restarted.chain().tip_hash() ==
               server.chain().tip_hash());
    }

    // The successful reorg must survive another restart from pruned storage.
    {
        NodeRuntime verified{
            params,
            client_dir,
            policy
        };
        assert(verified.start_at(
            now + 700U).ok());
        assert(verified.chain().height() ==
               server.chain().height());
        assert(verified.chain().tip_hash() ==
               server.chain().tip_hash());
    }

    std::error_code ec;
    std::filesystem::remove_all(server_dir, ec);
    std::filesystem::remove_all(client_dir, ec);
}


void test_ibd_batches_block_requests()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto server_dir =
        unique_dir("ibd-batch-server");
    const auto client_dir =
        unique_dir("ibd-batch-client");

    const std::uint64_t now =
        params.genesis.timestamp + 50'000U;

    NodeRuntime server{params, server_dir};
    NodeRuntime client{params, client_dir};

    assert(server.start_at(now).ok());
    assert(client.start_at(now).ok());

    constexpr std::uint32_t kBlocks = 20U;

    mine_blocks(
        server,
        payout_script(8U),
        now,
        kBlocks
    );

    PeerListener listener{params};
    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    SyncError server_error{
        SyncError::transport_failed
    };

    std::thread server_thread([&] {
        // One getheaders plus two getdata batches:
        // 16 blocks, then the remaining 4.
        serve_requests(
            listener,
            server.chain(),
            version(0x1751U, kBlocks),
            3U,
            server_error
        );
    });

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            version(0x1752U, 0U),
            5'000U
        );

    assert(connected.ok());

    const auto synced =
        sync_from_peer(
            *connected.session,
            client,
            now + 100U
        );

    assert(synced.ok());
    assert(synced.headers_received == kBlocks);
    assert(synced.blocks_requested == kBlocks);
    assert(synced.block_request_batches == 2U);
    assert(synced.blocks_accepted == kBlocks);
    assert(client.chain().tip_hash() ==
           server.chain().tip_hash());

    connected.session->close();
    server_thread.join();

    assert(server_error == SyncError::none);

    std::error_code ec;
    std::filesystem::remove_all(server_dir, ec);
    std::filesystem::remove_all(client_dir, ec);
}


void test_full_known_header_batch_is_not_treated_as_stalled()
{
    using namespace quintum::net;

    assert(!header_batch_may_continue(0U));
    assert(!header_batch_may_continue(
        kMaxHeadersPerMessage - 1U
    ));

    // Regression: a full batch may be entirely composed of blocks already
    // stored on a side branch. Sync must request the continuation instead
    // of declaring the peer stalled merely because no block was accepted
    // and the active tip did not change in this batch.
    assert(header_batch_may_continue(
        kMaxHeadersPerMessage
    ));
}

} // namespace

int main()
{
    test_wire_codecs_and_locator();
    test_genesis_to_tip_sync_and_restart();
    test_heavier_remote_branch_reorg();
    test_pruned_deep_reorg_redownloads_missing_bodies();
    test_ibd_batches_block_requests();
    test_full_known_header_batch_is_not_treated_as_stalled();
    return 0;
}
