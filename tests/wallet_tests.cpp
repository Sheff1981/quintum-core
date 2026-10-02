#include "consensus/monetary.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "node/node.hpp"
#include "net/runtime.hpp"
#include "wallet/address.hpp"
#include "wallet/wallet.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::filesystem::path unique_dir(
    std::string_view suffix)
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    return std::filesystem::temp_directory_path() /
        ("quintum-wallet-" +
         std::string(suffix) + "-" +
         std::to_string(stamp));
}

quintum::crypto::PrivateKey key_from_scalar(
    quintum::Byte scalar)
{
    quintum::crypto::PrivateKey key{};
    key.back() = scalar;
    assert(quintum::crypto::is_valid_private_key(key));
    return key;
}

quintum::crypto::PublicKey public_key_from_scalar(
    quintum::Byte scalar)
{
    const auto public_key =
        quintum::crypto::derive_public_key(
            key_from_scalar(scalar)
        );

    assert(public_key.has_value());
    return *public_key;
}

quintum::Bytes payout_from_scalar(
    quintum::Byte scalar)
{
    return quintum::consensus::
        make_p2pk_locking_script(
            public_key_from_scalar(scalar)
        );
}

void test_addresses()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto public_key =
        public_key_from_scalar(1U);

    const std::array networks{
        consensus::Network::mainnet,
        consensus::Network::testnet,
        consensus::Network::regtest,
    };

    for (const auto network : networks) {
        const std::string address =
            encode_address(
                network,
                public_key
            );

        assert(!address.empty());
        assert(address.starts_with(
            std::string(address_hrp(network)) +
            "1"
        ));
        assert(address.size() <= 90U);

        const auto decoded =
            decode_address(
                network,
                address
            );

        assert(decoded.ok());
        assert(decoded.public_key == public_key);

        std::string upper = address;
        std::transform(
            upper.begin(),
            upper.end(),
            upper.begin(),
            [](char ch) {
                if (ch >= 'a' && ch <= 'z') {
                    return static_cast<char>(
                        ch - 'a' + 'A');
                }
                return ch;
            }
        );

        const auto upper_decoded =
            decode_address(
                network,
                upper
            );

        assert(upper_decoded.ok());
        assert(upper_decoded.public_key ==
               public_key);

        std::string damaged = address;
        damaged.back() =
            damaged.back() == 'q'
                ? 'p'
                : 'q';

        const auto bad_checksum =
            decode_address(
                network,
                damaged
            );

        assert(!bad_checksum.ok());
        assert(bad_checksum.error ==
               AddressError::invalid_checksum);

        std::string mixed = address;
        mixed.front() =
            static_cast<char>(
                mixed.front() - 'a' + 'A');

        const auto bad_case =
            decode_address(
                network,
                mixed
            );

        assert(!bad_case.ok());
        assert(bad_case.error ==
               AddressError::invalid_format);
    }

    const std::string main_address =
        encode_address(
            consensus::Network::mainnet,
            public_key
        );

    const auto wrong_network =
        decode_address(
            consensus::Network::testnet,
            main_address
        );

    assert(!wrong_network.ok());
    assert(wrong_network.error ==
           AddressError::wrong_network);
}

void test_wallet_persistence_backup_and_network_binding()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto directory =
        unique_dir("persist");
    const auto backup_directory =
        unique_dir("backup");
    const auto corrupt_directory =
        unique_dir("corrupt");

    const auto& params =
        consensus::regtest_params();

    std::vector<std::string> expected_addresses;

    {
        Wallet wallet{
            params,
            directory
        };

        const auto started =
            wallet.start();

        assert(started.ok());
        assert(started.created);
        assert(!started.receive_address.empty());
        assert(std::filesystem::exists(
            wallet.path()
        ));

        const auto imported =
            wallet.import_private_key(
                key_from_scalar(9U)
            );

        assert(imported.ok());
        assert(!imported.address.empty());

        const auto duplicate =
            wallet.import_private_key(
                key_from_scalar(9U)
            );

        assert(!duplicate.ok());
        assert(duplicate.error ==
               WalletKeyError::duplicate_key);

        const auto another =
            wallet.new_receive_address();

        assert(another.ok());

        expected_addresses =
            wallet.addresses();

        assert(expected_addresses.size() == 3U);

        const auto backup_path =
            backup_directory /
            "wallet.dat";

        assert(wallet.backup(
                   backup_path) ==
               WalletStoreError::none);

        assert(wallet.backup(
                   backup_path) ==
               WalletStoreError::target_exists);
    }

    {
        Wallet reopened{
            params,
            directory
        };

        const auto started =
            reopened.start();

        assert(started.ok());
        assert(!started.created);
        assert(reopened.addresses() ==
               expected_addresses);
    }

    {
        Wallet recovered{
            params,
            backup_directory
        };

        const auto started =
            recovered.start();

        assert(started.ok());
        assert(!started.created);
        assert(recovered.addresses() ==
               expected_addresses);
    }

    {
        Wallet wrong_network{
            consensus::testnet_params(),
            backup_directory
        };

        const auto started =
            wrong_network.start();

        assert(!started.ok());
        assert(started.error ==
               WalletStartError::store_failed);
        assert(started.store_error ==
               WalletStoreError::wrong_network);
    }

    std::filesystem::create_directories(
        corrupt_directory
    );

    std::filesystem::copy_file(
        backup_directory / "wallet.dat",
        corrupt_directory / "wallet.dat",
        std::filesystem::copy_options::
            overwrite_existing
    );

    {
        std::fstream file(
            corrupt_directory / "wallet.dat",
            std::ios::in |
            std::ios::out |
            std::ios::binary
        );

        assert(file);

        char byte{0};
        file.seekg(12, std::ios::beg);
        file.read(&byte, 1);
        assert(file.gcount() == 1);
        byte = static_cast<char>(
            static_cast<unsigned char>(byte) ^
            0x01U
        );
        file.seekp(12, std::ios::beg);
        file.write(&byte, 1);
        file.flush();
        assert(file);
    }

    {
        Wallet corrupt{
            params,
            corrupt_directory
        };

        const auto started =
            corrupt.start();

        assert(!started.ok());
        assert(started.store_error ==
               WalletStoreError::corrupt);
    }

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
    std::filesystem::remove_all(
        backup_directory,
        ec
    );
    std::filesystem::remove_all(
        corrupt_directory,
        ec
    );
}


void test_prebacked_keypool_recovers_future_address()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto wallet_directory =
        unique_dir("keypool-live");
    const auto backup_directory =
        unique_dir("keypool-backup");
    const auto chain_directory =
        unique_dir("keypool-chain");

    const auto& params =
        consensus::regtest_params();

    std::string future_address;
    crypto::PublicKey future_public_key{};

    {
        Wallet wallet{
            params,
            wallet_directory
        };

        const auto started =
            wallet.start();

        assert(started.ok());
        assert(started.created);
        assert(started.backup_recommended);

        const auto backup_path =
            backup_directory /
            "wallet.dat";

        assert(wallet.backup(
                   backup_path) ==
               WalletStoreError::none);

        const auto next =
            wallet.new_receive_address();

        assert(next.ok());
        assert(!next.backup_recommended);

        future_address =
            next.address;
        future_public_key =
            next.public_key;
    }

    const std::uint64_t base_time =
        params.genesis.timestamp + 3'000U;

    NodeRuntime node{
        params,
        chain_directory
    };

    assert(node.start_at(base_time).ok());

    const auto mined =
        node.mine_block_at(
            consensus::make_p2pk_locking_script(
                future_public_key
            ),
            base_time + 1U,
            4'096U
        );

    assert(mined.ok());
    assert(mined.height == 1U);

    {
        Wallet recovered{
            params,
            backup_directory
        };

        const auto started =
            recovered.start();

        assert(started.ok());
        assert(!started.created);

        const auto before =
            recovered.addresses();

        assert(std::find(
                   before.begin(),
                   before.end(),
                   future_address) ==
               before.end());

        const auto synced =
            recovered.sync(
                node.chain(),
                node.mempool()
            );

        assert(synced.ok());
        assert(synced.balance.immature ==
               consensus::kInitialSubsidy);

        const auto after =
            recovered.addresses();

        assert(std::find(
                   after.begin(),
                   after.end(),
                   future_address) !=
               after.end());
    }

    std::error_code ec;
    std::filesystem::remove_all(
        wallet_directory,
        ec
    );
    std::filesystem::remove_all(
        backup_directory,
        ec
    );
    std::filesystem::remove_all(
        chain_directory,
        ec
    );
}

void test_wallet_balance_build_sign_confirm_and_recover()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto chain_directory =
        unique_dir("chain");
    const auto wallet_directory =
        unique_dir("live");
    const auto recovered_directory =
        unique_dir("recovered");

    const auto& params =
        consensus::regtest_params();

    const std::uint64_t base_time =
        params.genesis.timestamp + 1'000U;

    NodeRuntime node{
        params,
        chain_directory
    };

    assert(node.start_at(base_time).ok());

    Wallet wallet{
        params,
        wallet_directory
    };

    assert(wallet.start().ok());

    const auto imported =
        wallet.import_private_key(
            key_from_scalar(9U)
        );

    assert(imported.ok());

    const auto owned_public_key =
        public_key_from_scalar(9U);

    assert(wallet.owns_public_key(
        owned_public_key
    ));

    const Bytes owned_payout =
        consensus::make_p2pk_locking_script(
            owned_public_key
        );

    const Bytes other_payout =
        payout_from_scalar(11U);

    const auto first =
        node.mine_block_at(
            owned_payout,
            base_time + 1U,
            4'096U
        );

    assert(first.ok());
    assert(first.height == 1U);

    {
        const auto synced =
            wallet.sync(
                node.chain(),
                node.mempool()
            );

        assert(synced.ok());
        assert(synced.balance.confirmed == 0U);
        assert(synced.balance.available == 0U);
        assert(synced.balance.pending == 0U);
        assert(synced.balance.immature ==
               consensus::kInitialSubsidy);
    }

    for (std::uint32_t height = 2U;
         height <= 100U;
         ++height) {
        const auto mined =
            node.mine_block_at(
                other_payout,
                base_time +
                    static_cast<std::uint64_t>(
                        height),
                4'096U
            );

        assert(mined.ok());
        assert(mined.height == height);
    }

    const auto mature =
        wallet.sync(
            node.chain(),
            node.mempool()
        );

    assert(mature.ok());
    assert(mature.balance.confirmed ==
           consensus::kInitialSubsidy);
    assert(mature.balance.available ==
           consensus::kInitialSubsidy);
    assert(mature.balance.pending == 0U);
    assert(mature.balance.immature == 0U);

    const auto recipient_public_key =
        public_key_from_scalar(10U);

    const std::string recipient =
        encode_address(
            consensus::Network::regtest,
            recipient_public_key
        );

    const std::string wrong_network =
        encode_address(
            consensus::Network::testnet,
            recipient_public_key
        );

    const auto wrong =
        wallet.create_transaction(
            wrong_network,
            consensus::kAtomicUnitsPerCoin,
            123U,
            node.chain(),
            node.mempool()
        );

    assert(!wrong.ok());
    assert(wrong.error ==
           WalletCreateError::
               wrong_network_address);

    constexpr Amount amount{
        10U *
        consensus::kAtomicUnitsPerCoin
    };
    constexpr Amount fee{123U};

    const auto created =
        wallet.create_transaction(
            recipient,
            amount,
            fee,
            node.chain(),
            node.mempool()
        );

    assert(created.ok());
    assert(created.amount == amount);
    assert(created.fee == fee);
    assert(created.selected_value ==
           consensus::kInitialSubsidy);
    assert(created.change ==
           consensus::kInitialSubsidy -
               amount - fee);
    assert(created.transaction.inputs.size() ==
           1U);
    assert(created.transaction.outputs.size() ==
           2U);
    assert(transaction_id(
               created.transaction) ==
           created.txid);

    const auto submitted =
        node.submit_transaction(
            created.transaction
        );

    assert(submitted.ok());
    assert(submitted.mempool.fee == fee);

    const auto pending =
        wallet.sync(
            node.chain(),
            node.mempool()
        );

    assert(pending.ok());
    assert(pending.balance.confirmed ==
           consensus::kInitialSubsidy);
    assert(pending.balance.available == 0U);
    assert(pending.balance.pending ==
           created.change);
    assert(pending.balance.immature == 0U);

    const auto confirmed =
        node.mine_mempool_block_at(
            other_payout,
            base_time + 101U,
            4'096U
        );

    assert(confirmed.ok());
    assert(confirmed.height == 101U);
    assert(confirmed.total_fees == fee);

    const auto final_sync =
        wallet.sync(
            node.chain(),
            node.mempool()
        );

    assert(final_sync.ok());
    assert(final_sync.balance.confirmed ==
           created.change);
    assert(final_sync.balance.available ==
           created.change);
    assert(final_sync.balance.pending == 0U);
    assert(final_sync.balance.immature == 0U);

    const auto backup_path =
        recovered_directory /
        "wallet.dat";

    assert(wallet.backup(
               backup_path) ==
           WalletStoreError::none);

    {
        Wallet recovered{
            params,
            recovered_directory
        };

        const auto started =
            recovered.start();

        assert(started.ok());

        const auto recovered_sync =
            recovered.sync(
                node.chain(),
                node.mempool()
            );

        assert(recovered_sync.ok());
        assert(recovered_sync.balance ==
               final_sync.balance);
        assert(recovered.owns_public_key(
            owned_public_key
        ));
    }

    std::error_code ec;
    std::filesystem::remove_all(
        chain_directory,
        ec
    );
    std::filesystem::remove_all(
        wallet_directory,
        ec
    );
    std::filesystem::remove_all(
        recovered_directory,
        ec
    );
}


void test_network_runtime_wallet_bridge()
{
    using namespace quintum;
    using namespace quintum::net;
    using namespace quintum::wallet;

    const auto directory =
        unique_dir("runtime");
    const auto backup_directory =
        unique_dir("runtime-backup");

    const auto& params =
        consensus::regtest_params();

    NetworkRuntime runtime{
        params,
        directory
    };

    NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.target_outbound = 0U;
    config.accept_poll_ms = 10U;

    const auto started =
        runtime.start(config);

    assert(started.ok());
    assert(started.wallet.ok());

    const auto initial =
        runtime.status();

    assert(initial.running);
    assert(!initial.receive_address.empty());
    assert(initial.wallet_balance ==
           WalletBalance{});

    const auto decoded =
        decode_address(
            consensus::Network::regtest,
            initial.receive_address
        );

    assert(decoded.ok());

    const Bytes owned_payout =
        consensus::make_p2pk_locking_script(
            decoded.public_key
        );

    const Bytes external_payout =
        payout_from_scalar(12U);

    const std::uint64_t base_time =
        params.genesis.timestamp + 2'000U;

    const auto first =
        runtime.mine_mempool_block_at(
            owned_payout,
            base_time + 1U,
            4'096U
        );

    assert(first.ok());
    assert(first.height == 1U);

    for (std::uint32_t height = 2U;
         height <= 100U;
         ++height) {
        const auto mined =
            runtime.mine_mempool_block_at(
                external_payout,
                base_time +
                    static_cast<std::uint64_t>(
                        height),
                4'096U
            );

        assert(mined.ok());
        assert(mined.height == height);
    }

    const auto mature =
        runtime.status();

    assert(mature.height ==
           std::optional<std::uint32_t>{100U});
    assert(mature.wallet_balance.confirmed ==
           consensus::kInitialSubsidy);
    assert(mature.wallet_balance.available ==
           consensus::kInitialSubsidy);

    constexpr Amount amount{
        5U *
        consensus::kAtomicUnitsPerCoin
    };
    constexpr Amount fee{77U};

    const std::string destination =
        encode_address(
            consensus::Network::regtest,
            public_key_from_scalar(13U)
        );

    const auto sent =
        runtime.send_to_address(
            destination,
            amount,
            fee
        );

    assert(sent.ok());
    assert(sent.wallet.fee == fee);
    assert(sent.node.mempool.fee == fee);

    const auto pending =
        runtime.status();

    assert(pending.mempool_transactions == 1U);
    assert(pending.wallet_balance.available == 0U);
    assert(pending.wallet_balance.pending ==
           consensus::kInitialSubsidy -
               amount - fee);

    const auto confirmed =
        runtime.mine_mempool_block_at(
            external_payout,
            base_time + 101U,
            4'096U
        );

    assert(confirmed.ok());
    assert(confirmed.height == 101U);
    assert(confirmed.total_fees == fee);

    const auto final_status =
        runtime.status();

    assert(final_status.mempool_transactions == 0U);
    assert(final_status.wallet_balance.pending == 0U);
    assert(final_status.wallet_balance.available ==
           consensus::kInitialSubsidy -
               amount - fee);

    const auto extra_address =
        runtime.new_receive_address();

    assert(extra_address.ok());

    const auto backup_path =
        backup_directory /
        "wallet.dat";

    assert(runtime.backup_wallet(
               backup_path) ==
           WalletStoreError::none);
    assert(std::filesystem::exists(
        backup_path
    ));

    runtime.stop();
    assert(!runtime.running());

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
    std::filesystem::remove_all(
        backup_directory,
        ec
    );
}

} // namespace

int main()
{
    test_addresses();
    test_wallet_persistence_backup_and_network_binding();
    test_prebacked_keypool_recovers_future_address();
    test_wallet_balance_build_sign_confirm_and_recover();
    test_network_runtime_wallet_bridge();
    return 0;
}
