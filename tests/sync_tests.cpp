#include "consensus/pow.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "net/peer.hpp"
#include "net/diagnostics.hpp"
#include "net/sync.hpp"
#include "node/node.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <mutex>
#include <atomic>
#include <string>
#include <thread>
#ifndef _WIN32
#include <sys/socket.h>
#endif

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


void serve_requests_with_interleaved_inv(
    quintum::net::PeerListener& listener,
    const quintum::Chainstate& chain,
    quintum::net::VersionMessage local,
    std::size_t requests,
    quintum::net::SyncError& error)
{
    using namespace quintum;
    using namespace quintum::net;

    auto accepted =
        listener.accept_and_handshake(
            local,
            5'000U
        );

    if (!accepted.ok()) {
        error = SyncError::transport_failed;
        return;
    }

    const auto tip = chain.tip_hash();
    if (!tip) {
        error = SyncError::malformed_message;
        accepted.session->close();
        return;
    }

    const std::array<InventoryItem, 1> announcement{
        InventoryItem{
            .type = kInventoryBlock,
            .hash = *tip,
        }
    };
    const auto inv_payload =
        serialize_inventory(announcement);

    for (std::size_t i = 0U;
         i < requests;
         ++i) {
        WireMessage request;
        const auto receive =
            accepted.session->receive_command(
                request
            );

        if (receive != PeerError::none) {
            error = SyncError::transport_failed;
            accepted.session->close();
            return;
        }

        // Reproduce the live failure mode: a freshly mined block inventory
        // arrives between a sync request and its expected response.
        if (accepted.session->send_command(
                "inv",
                inv_payload) !=
            PeerError::none) {
            error = SyncError::transport_failed;
            accepted.session->close();
            return;
        }

        const auto served =
            serve_sync_message(
                *accepted.session,
                chain,
                request
            );

        if (!served.ok()) {
            error = served.error;
            accepted.session->close();
            return;
        }
    }

    accepted.session->close();
    error = SyncError::none;
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

    auto diagnostics = std::make_shared<Diagnostics>(client_dir / "diagnostics");
    const auto synced =
        sync_from_peer(
            *connected.session,
            client,
            now + 100U,
            diagnostics
        );

    assert(synced.ok());
    const auto snapshot = diagnostics->snapshot_json();
    for (const auto field : {
             "\"headers_received\":5", "\"headers_verified\":5",
             "\"blocks_requested\":5", "\"blocks_received\":5",
             "\"blocks_accepted\":5", "\"blocks_rejected\":0",
             "\"local_height\":5", "\"header_batches_accepted\":1"}) {
        assert(snapshot.find(field) != std::string::npos);
    }
    const auto events = diagnostics->export_log();
    for (const auto phase : {
             "headers_sync", "header_wait", "header_validation",
             "blocks_sync", "block_wait", "block_validation"}) {
        assert(events.find(phase) != std::string::npos);
    }
    // The sync helper cannot declare global runtime synchronization.
    assert(events.find("fully_synchronized") == std::string::npos);

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


void test_sync_tolerates_interleaved_block_inventory()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto server_dir =
        unique_dir("interleaved-inv-server");
    const auto client_dir =
        unique_dir("interleaved-inv-client");

    const std::uint64_t now =
        params.genesis.timestamp + 15'000U;

    NodeRuntime server{params, server_dir};
    NodeRuntime client{params, client_dir};

    assert(server.start_at(now).ok());
    assert(client.start_at(now).ok());

    mine_blocks(
        server,
        payout_script(12U),
        now,
        5U
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
        // One getheaders and one getdata batch. Inject an unsolicited block
        // inv before each response, as happens when the remote peer keeps
        // mining while this node is catching up.
        serve_requests_with_interleaved_inv(
            listener,
            server.chain(),
            version(0x1703U, 5U),
            2U,
            server_error
        );
    });

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            version(0x1704U, 0U),
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
    assert(synced.blocks_accepted == 5U);
    assert(client.chain().height() ==
           server.chain().height());
    assert(client.chain().tip_hash() ==
           server.chain().tip_hash());

    connected.session->close();
    server_thread.join();

    assert(server_error == SyncError::none);

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


void test_randomx_headers_first_uses_randomx_pow()
{
    using namespace quintum;
    using namespace quintum::net;

    auto params =
        consensus::randomx_testnet_params();

    // Keep public RandomX consensus parameters and Genesis intact while the
    // test remains completely isolated from public discovery.
    params.network =
        consensus::Network::regtest;
    params.name =
        "randomx-headers-first-test";
    params.p2p_port = 0U;
    params.rpc_port = 0U;

    const auto server_dir =
        unique_dir("randomx-header-server");
    const auto client_dir =
        unique_dir("randomx-header-client");

    const std::uint64_t now =
        params.genesis.timestamp + 10'000U;

    NodeRuntime server{params, server_dir};
    NodeRuntime client{params, client_dir};

    assert(server.start_at(now).ok());
    assert(client.start_at(now).ok());

    constexpr std::uint32_t kBlocks = 3U;

    for (std::uint32_t height = 1U;
         height <= kBlocks;
         ++height) {
        const auto mined =
            server.mine_block_at(
                payout_script(10U),
                params.genesis.timestamp +
                    static_cast<std::uint64_t>(
                        height) *
                    params.pow.
                        target_spacing_seconds,
                20'000U
            );

        assert(mined.ok());
        assert(mined.height == height);
    }

    bool has_sha256d_mismatch{false};

    for (std::uint32_t height = 1U;
         height <= kBlocks;
         ++height) {
        const auto header =
            server.chain().
                active_header(height);
        assert(header);

        if (consensus::check_proof_of_work(
                *header,
                params.pow) !=
            consensus::PowCheckError::none) {
            has_sha256d_mismatch = true;
        }
    }

    // This guarantees the fixture would have been rejected by the old
    // headers-first path that incorrectly applied SHA-256d to RandomX.
    assert(has_sha256d_mismatch);

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
            version(0x1771U, kBlocks),
            2U,
            server_error
        );
    });

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            version(0x1772U, 0U),
            10'000U
        );

    assert(connected.ok());

    const auto synced =
        sync_from_peer(
            *connected.session,
            client,
            now
        );

    assert(synced.ok());
    assert(synced.headers_received ==
           kBlocks);
    assert(synced.blocks_requested ==
           kBlocks);
    assert(synced.block_request_batches ==
           1U);
    assert(synced.blocks_accepted ==
           kBlocks);
    assert(client.chain().height() ==
           server.chain().height());
    assert(client.chain().tip_hash() ==
           server.chain().tip_hash());

    connected.session->close();
    server_thread.join();

    assert(server_error ==
           SyncError::none);

    std::error_code ec;
    std::filesystem::remove_all(
        server_dir,
        ec
    );
    std::filesystem::remove_all(
        client_dir,
        ec
    );
}


enum class InvalidHeaderCase { difficulty, proof_of_work, malformed };

void test_invalid_headers_stop_before_block_download(InvalidHeaderCase test_case)
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto client_dir =
        unique_dir("bad-header-difficulty");

    const std::uint64_t now =
        params.genesis.timestamp + 10'000U;

    NodeRuntime client{params, client_dir};
    assert(client.start_at(now).ok());
    assert(client.chain().height() ==
           std::optional<std::uint32_t>{0U});

    BlockHeader invalid;
    invalid.previous_block =
        params.genesis.hash;
    invalid.timestamp =
        params.genesis.timestamp + 1U;

    if (test_case == InvalidHeaderCase::proof_of_work) {
        invalid.bits = *client.chain().next_work_required(invalid.timestamp);
        bool invalid_pow = false;
        for (std::uint64_t nonce = 0U; nonce < 1'000'000U; ++nonce) {
            invalid.nonce = nonce;
            if (consensus::check_proof_of_work(invalid, params.pow) ==
                consensus::PowCheckError::hash_above_target) {
                invalid_pow = true;
                break;
            }
        }
        assert(invalid_pow);
    } else {
        // Regtest expects the parent's unchanged 0x2100ffff target here. This
        // harder target is still below the PoW limit and can therefore carry a
        // perfectly valid hash while remaining consensus-invalid bad-diffbits.
        invalid.bits = 0x207fffffU;
    
        const auto mined =
            consensus::mine_header(
                invalid,
                4'096U
            );
        assert(mined.found());
        assert(consensus::check_proof_of_work(
                   invalid,
                   params.pow) ==
               consensus::PowCheckError::none);
        assert(client.chain().
                   next_work_required(
                       invalid.timestamp) !=
               std::optional<std::uint32_t>{
                   invalid.bits});
    }

    PeerListener listener{params};
    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    bool received_getdata{false};
    PeerError server_error{
        PeerError::none
    };

    std::thread server_thread([&] {
        auto accepted =
            listener.accept_and_handshake(
                version(0x1761U, 1U),
                5'000U
            );

        if (!accepted.ok()) {
            server_error =
                accepted.error;
            return;
        }

        WireMessage request;
        server_error =
            accepted.session->receive_command(
                request
            );

        if (server_error !=
                PeerError::none ||
            request.command !=
                "getheaders") {
            accepted.session->close();
            return;
        }

        const std::array<BlockHeader, 1>
            headers{invalid};

        server_error =
            accepted.session->send_command(
                "headers",
                test_case == InvalidHeaderCase::malformed
                    ? Bytes{0xffU} : serialize_headers(headers)
            );

        if (server_error ==
                PeerError::none &&
            accepted.session->wait_readable(
                500U)) {
            WireMessage unexpected;
            if (accepted.session->
                    receive_command(
                        unexpected) ==
                    PeerError::none &&
                unexpected.command ==
                    "getdata") {
                received_getdata = true;
            }
        }

        accepted.session->close();
    });

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            version(0x1762U, 0U),
            5'000U
        );
    assert(connected.ok());

    auto diagnostics = std::make_shared<Diagnostics>(client_dir / "diagnostics");
    const auto synced =
        sync_from_peer(
            *connected.session,
            client,
            now,
            diagnostics
        );

    if (test_case == InvalidHeaderCase::malformed) {
        assert(synced.error == SyncError::malformed_message);
        assert(synced.headers_received == 0U);
    } else if (test_case == InvalidHeaderCase::proof_of_work) {
        assert(synced.error == SyncError::invalid_header_pow);
        assert(synced.chain_error == ChainConnectError::invalid_proof_of_work);
        assert(synced.headers_received == 1U);
    } else {
        assert(synced.error == SyncError::invalid_header_consensus);
        assert(synced.chain_error == ChainConnectError::unexpected_difficulty);
        assert(synced.headers_received == 1U);
    }
    const auto snapshot = diagnostics->snapshot_json();
    for (const auto field : {
             "\"headers_verified\":0", "\"header_batches_received\":1",
             "\"header_batches_rejected\":1", "\"blocks_requested\":0",
             "\"blocks_accepted\":0"}) {
        assert(snapshot.find(field) != std::string::npos);
    }
    if (test_case != InvalidHeaderCase::malformed) {
        assert(snapshot.find("\"headers_rejected\":1") != std::string::npos);
    }
    const auto events = diagnostics->export_log();
    assert(events.find("\"event\":\"failure\"") != std::string::npos);
    assert(events.find("fully_synchronized") == std::string::npos);

    assert(synced.blocks_requested == 0U);
    assert(synced.block_request_batches == 0U);
    assert(client.chain().height() ==
           std::optional<std::uint32_t>{0U});

    connected.session->close();
    server_thread.join();

    assert(server_error ==
           PeerError::none);
    assert(!received_getdata);

    std::error_code ec;
    std::filesystem::remove_all(
        client_dir,
        ec
    );
}


void test_chainwork_sync_selection()
{
    using namespace quintum;
    using namespace quintum::net;

    Hash256 light_work{};
    Hash256 heavy_work{};
    light_work.back() = 10U;
    heavy_work.back() = 20U;

    const auto payload =
        serialize_chain_work(heavy_work);
    const auto parsed =
        parse_chain_work(payload);

    assert(parsed.has_value());
    assert(*parsed == heavy_work);

    Bytes malformed(payload.begin(), payload.end() - 1);
    assert(!parse_chain_work(malformed).has_value());

    // A shorter peer can still have more cumulative work. Fork choice must
    // follow work, not height.
    assert(sync_driver_should_run(
        light_work,
        heavy_work,
        200U,
        150U,
        false
    ));

    // A taller peer can still be the weaker chain. Do not chase it merely
    // because its height is larger.
    assert(!sync_driver_should_run(
        heavy_work,
        light_work,
        150U,
        200U,
        true
    ));

    // Equal work uses TCP direction as a deterministic tie-breaker so only
    // one side drives the synchronous setup exchange.
    assert(sync_driver_should_run(
        heavy_work,
        heavy_work,
        150U,
        150U,
        true
    ));
    assert(!sync_driver_should_run(
        heavy_work,
        heavy_work,
        150U,
        150U,
        false
    ));

    // Legacy peers without the capability preserve Stage 37 behavior.
    assert(sync_driver_should_run(
        light_work,
        std::nullopt,
        10U,
        11U,
        false
    ));
    assert(!sync_driver_should_run(
        light_work,
        std::nullopt,
        11U,
        10U,
        true
    ));
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

#ifndef _WIN32
void test_sync_transport_failure_evidence(unsigned int test_case)
{
    using namespace quintum;
    using namespace quintum::net;
    const auto& params = consensus::regtest_params();
    const auto directory = unique_dir("sync-io-evidence");
    const auto now = params.genesis.timestamp + 10'000U;
    NodeRuntime node{params, directory};
    assert(node.start_at(now).ok());
    auto diagnostics = std::make_shared<Diagnostics>(directory / "diagnostics");
    int sockets[2]{};
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    if (test_case == 2U) {
        timeval timeout{0, 50'000};
        assert(setsockopt(sockets[0], SOL_SOCKET, SO_RCVTIMEO,
            &timeout, sizeof(timeout)) == 0);
    }
    PeerSession client(params, static_cast<std::uintptr_t>(sockets[0]),
        false, version(0x3001U, 0U));
    PeerSession server(params, static_cast<std::uintptr_t>(sockets[1]),
        true, version(0x3002U, 1U));
    std::thread responder([&] {
        WireMessage request;
        assert(server.receive_command(request) == PeerError::none);
        assert(request.command == "getheaders");
        if (test_case == 1U) {
            const std::array<Byte, 3U> prefix{0U, 1U, 2U};
            assert(send(sockets[1], prefix.data(), prefix.size(), 0) == 3);
        } else if (test_case == 2U) {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
        server.close();
    });
    const auto result = sync_from_peer(client, node, now, diagnostics);
    responder.join();
    assert(result.error == SyncError::transport_failed);
    assert(result.peer_error == PeerError::receive_failed);
    const auto evidence = client.last_io_failure();
    const auto events = diagnostics->export_log();
    assert(events.find("header_wait") != std::string::npos);
    assert(events.find("socket_error=" + std::to_string(evidence.socket_error)) !=
        std::string::npos);
    assert(events.find(test_case == 2U ? "eof=0" : "eof=1") != std::string::npos);
    assert(events.find(test_case == 1U ? "partial_bytes=3" : "partial_bytes=0") !=
        std::string::npos);
    assert(events.find("fully_synchronized") == std::string::npos);
    assert(node.chain().height() == std::optional<std::uint32_t>{0U});
    client.close();
    std::error_code error;
    std::filesystem::remove_all(directory, error);
}
#endif

void test_header_validation_services_ping_outside_chain_lock()
{
    using namespace quintum;
    using namespace quintum::net;
    const auto& params = consensus::regtest_params();
    const auto directory = unique_dir("header-ping-progress");
    const auto now = params.genesis.timestamp + 100'000U;
    NodeRuntime source{params, directory / "source"};
    NodeRuntime client{params, directory / "client"};
    assert(source.start_at(now).ok());
    assert(client.start_at(now).ok());
    assert(source.mine_block_at(payout_script(11U), params.genesis.timestamp + 1U, 100U).ok());
    const auto header = *source.chain().active_header(1U);
    const auto body = *source.chain().block(*source.chain().tip_hash());
    PeerListener listener{params};
    assert(listener.listen("127.0.0.1", 0U) == PeerError::none);
    std::promise<void> ping_sent;
    auto ping_ready = ping_sent.get_future();
    std::thread server([&] {
        auto accepted = listener.accept_and_handshake(version(0x7171U, 1U), 5'000U);
        assert(accepted.ok());
        WireMessage message;
        assert(accepted.session->receive_command(message) == PeerError::none);
        assert(message.command == "getheaders");
        const std::array<BlockHeader, 1U> headers{header};
        assert(accepted.session->send_command("headers", serialize_headers(headers)) == PeerError::none);
        assert(accepted.session->send_command("ping", serialize_nonce(0x1234U)) == PeerError::none);
        ping_sent.set_value();
        assert(accepted.session->receive_command(message) == PeerError::none);
        assert(message.command == "pong");
        assert(parse_nonce(message.payload) == std::optional<std::uint64_t>{0x1234U});
        assert(accepted.session->receive_command(message) == PeerError::none);
        assert(message.command == "getdata");
        assert(accepted.session->send_command("block", serialize_block_payload(body)) == PeerError::none);
        accepted.session->close();
    });
    auto connected = connect_and_handshake(params, "127.0.0.1", listener.local_port(),
        version(0x7172U, 0U), 5'000U);
    assert(connected.ok());
    std::mutex state;
    std::atomic<bool> cancel{false};
    auto diagnostics = std::make_shared<Diagnostics>(directory / "diagnostics");
    const auto result = sync_from_peer(*connected.session, client, now, diagnostics,
        SyncOptions{.state_mutex = &state, .cancel = &cancel,
            .header_progress = [&](std::size_t, bool complete) {
                if (!complete) ping_ready.wait();
                assert(state.try_lock()); state.unlock();
                return true;
            }});
    server.join();
    assert(result.ok());
    assert(result.blocks_accepted == 1U);
    const auto snapshot = diagnostics->snapshot_json();
    for (const auto field : {"\"headers_validated\":1", "\"headers_committed\":1",
        "\"header_batch_progress\":1", "\"headers_validating\":0"})
        assert(snapshot.find(field) != std::string::npos);
    std::error_code error;
    std::filesystem::remove_all(directory, error);
}

} // namespace

int main()
{
    test_header_validation_services_ping_outside_chain_lock();
#ifndef _WIN32
    test_sync_transport_failure_evidence(0U);
    test_sync_transport_failure_evidence(1U);
    test_sync_transport_failure_evidence(2U);
#endif
    test_wire_codecs_and_locator();
    test_genesis_to_tip_sync_and_restart();
    test_sync_tolerates_interleaved_block_inventory();
    test_heavier_remote_branch_reorg();
    test_pruned_deep_reorg_redownloads_missing_bodies();
    test_ibd_batches_block_requests();
    test_randomx_headers_first_uses_randomx_pow();
    test_invalid_headers_stop_before_block_download(InvalidHeaderCase::difficulty);
    test_invalid_headers_stop_before_block_download(InvalidHeaderCase::proof_of_work);
    test_invalid_headers_stop_before_block_download(InvalidHeaderCase::malformed);
    test_chainwork_sync_selection();
    test_full_known_header_batch_is_not_treated_as_stalled();
    return 0;
}

