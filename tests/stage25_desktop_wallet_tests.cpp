#include "consensus/monetary.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "net/runtime.hpp"
#include "wallet/address.hpp"
#include "wallet/wallet.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

std::filesystem::path unique_dir(std::string_view suffix)
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    return std::filesystem::temp_directory_path() /
        ("quintum-stage25-" +
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
    const auto key =
        quintum::crypto::derive_public_key(
            key_from_scalar(scalar)
        );
    assert(key.has_value());
    return *key;
}

quintum::Bytes payout_from_scalar(
    quintum::Byte scalar)
{
    return quintum::consensus::make_p2pk_locking_script(
        public_key_from_scalar(scalar)
    );
}

void test_wallet_metadata_persists_and_validates_network()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto directory =
        unique_dir("metadata");
    const auto& params =
        consensus::regtest_params();

    const std::string address =
        encode_address(
            params.network,
            public_key_from_scalar(71U)
        );

    const std::string wrong_network =
        encode_address(
            consensus::Network::testnet,
            public_key_from_scalar(72U)
        );

    Hash256 txid{};
    txid.back() = 42U;

    {
        Wallet wallet{
            params,
            directory
        };

        const auto started =
            wallet.start(
                "stage25-password"
            );
        assert(started.ok());

        assert(wallet.set_address_label(
                   address,
                   "Alice") ==
               WalletMetadataError::none);

        std::string uppercase =
            address;

        std::transform(
            uppercase.begin(),
            uppercase.end(),
            uppercase.begin(),
            [](char ch) {
                if (ch >= 'a' &&
                    ch <= 'z') {
                    return static_cast<char>(
                        ch - 'a' + 'A'
                    );
                }

                return ch;
            }
        );

        assert(wallet.set_address_label(
                   uppercase,
                   "Alice") ==
               WalletMetadataError::none);

        assert(wallet.set_transaction_label(
                   txid,
                   "Invoice 42") ==
               WalletMetadataError::none);

        assert(wallet.set_address_label(
                   wrong_network,
                   "Wrong") ==
               WalletMetadataError::
                   wrong_network_address);

        // Key reservation reorders wallet key records. Metadata identity
        // must remain stable across ordinary New Address operations.
        const auto next_address =
            wallet.new_receive_address();
        assert(next_address.ok());

        const auto entries =
            wallet.address_book();

        assert(entries.size() == 1U);
        assert(entries.front().address ==
               address);
        assert(entries.front().label ==
               "Alice");

        const auto tx_label =
            wallet.transaction_label(txid);

        assert(tx_label ==
               std::optional<std::string>{
                   "Invoice 42"});
    }

    {
        Wallet reopened{
            params,
            directory
        };

        const auto started =
            reopened.start(
                "stage25-password"
            );
        assert(started.ok());

        const auto entries =
            reopened.address_book();

        assert(entries.size() == 1U);
        assert(entries.front().address ==
               address);
        assert(entries.front().label ==
               "Alice");

        assert(reopened.transaction_label(
                   txid) ==
               std::optional<std::string>{
                   "Invoice 42"});

        assert(reopened.set_address_label(
                   address,
                   "") ==
               WalletMetadataError::none);

        assert(reopened.address_book().empty());
    }

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

void test_metadata_is_bound_to_wallet()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto first_directory =
        unique_dir("meta-wallet-a");
    const auto second_directory =
        unique_dir("meta-wallet-b");
    const auto& params =
        consensus::regtest_params();

    {
        Wallet first{
            params,
            first_directory
        };

        assert(first.start(
                   "stage25-a").ok());

        const std::string address =
            encode_address(
                params.network,
                public_key_from_scalar(77U)
            );

        assert(first.set_address_label(
                   address,
                   "Wallet A") ==
               WalletMetadataError::none);
    }

    {
        Wallet second{
            params,
            second_directory
        };

        assert(second.start(
                   "stage25-b").ok());
    }

    std::error_code ec;

    std::filesystem::copy_file(
        first_directory /
            "wallet_meta.dat",
        second_directory /
            "wallet_meta.dat",
        std::filesystem::copy_options::
            overwrite_existing,
        ec
    );

    assert(!ec);

    {
        Wallet reopened{
            params,
            second_directory
        };

        const auto started =
            reopened.start(
                "stage25-b"
            );

        assert(!started.ok());
        assert(started.error ==
               WalletStartError::
                   metadata_failed);
        assert(started.metadata_error ==
               WalletMetadataError::
                   wrong_wallet);
    }

    std::filesystem::remove_all(
        first_directory,
        ec
    );
    ec.clear();
    std::filesystem::remove_all(
        second_directory,
        ec
    );
}

void test_corrupt_metadata_fails_loudly()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto directory =
        unique_dir("corrupt-meta");
    const auto& params =
        consensus::regtest_params();

    {
        Wallet wallet{
            params,
            directory
        };

        assert(wallet.start(
                   "stage25-password").ok());

        const std::string address =
            encode_address(
                params.network,
                public_key_from_scalar(73U)
            );

        assert(wallet.set_address_label(
                   address,
                   "Persistent") ==
               WalletMetadataError::none);
    }

    {
        std::ofstream output{
            directory / "wallet_meta.dat",
            std::ios::binary |
                std::ios::trunc
        };
        output << "corrupt";
    }

    {
        Wallet reopened{
            params,
            directory
        };

        const auto started =
            reopened.start(
                "stage25-password"
            );

        assert(!started.ok());
        assert(started.error ==
               WalletStartError::
                   metadata_failed);
        assert(started.metadata_error ==
               WalletMetadataError::
                   corrupt);
    }

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

void mine_spendable_wallet_coin(
    quintum::net::NetworkRuntime& runtime,
    const quintum::consensus::ChainParams& params,
    std::uint64_t base_time)
{
    using namespace quintum;

    const auto status =
        runtime.status();

    const auto owned =
        wallet::decode_address(
            params.network,
            status.receive_address
        );
    assert(owned.ok());

    const Bytes owned_payout =
        consensus::make_p2pk_locking_script(
            owned.public_key
        );
    const Bytes external_payout =
        payout_from_scalar(74U);

    const auto first =
        runtime.mine_mempool_block_at(
            owned_payout,
            base_time + 1U,
            4'096U
        );
    assert(first.ok());

    for (std::uint32_t height = 2U;
         height <= 100U;
         ++height) {
        const auto mined =
            runtime.mine_mempool_block_at(
                external_payout,
                base_time +
                    static_cast<std::uint64_t>(
                        height
                    ),
                4'096U
            );
        assert(mined.ok());
    }
}

void test_desktop_snapshot_preview_confirm_and_stale_guard()
{
    using namespace quintum;
    using namespace quintum::net;
    using namespace quintum::wallet;

    const auto directory =
        unique_dir("desktop");
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
    config.wallet_passphrase =
        "stage25-runtime-password";

    assert(runtime.start(
               std::move(config)).ok());

    const std::uint64_t base_time =
        params.genesis.timestamp +
        120'000U;

    mine_spendable_wallet_coin(
        runtime,
        params,
        base_time
    );

    const std::string destination =
        encode_address(
            params.network,
            public_key_from_scalar(75U)
        );

    assert(runtime.set_address_label(
               destination,
               "Supplier") ==
           WalletMetadataError::none);

    const Amount amount =
        consensus::kAtomicUnitsPerCoin;

    const auto preview =
        runtime.preview_send(
            destination,
            amount
        );

    assert(preview.ok());
    assert(preview.destination ==
           destination);
    assert(preview.amount == amount);
    assert(preview.quote.fee > 0U);
    assert(preview.recipient_label ==
           std::optional<std::string>{
               "Supplier"});

    const auto snapshot =
        runtime.desktop_snapshot();

    assert(snapshot.status.running);
    assert(snapshot.status.height ==
           std::optional<std::uint32_t>{100U});
    assert(snapshot.address_book.size() ==
           1U);
    assert(snapshot.address_book.front().label ==
           "Supplier");

    const auto external_payout =
        payout_from_scalar(76U);

    const auto extra =
        runtime.mine_mempool_block_at(
            external_payout,
            base_time + 101U,
            4'096U
        );
    assert(extra.ok());

    const auto stale =
        runtime.confirm_send(preview);

    assert(!stale.ok());
    assert(stale.error ==
           NetworkWalletSendError::
               stale_preview);
    assert(runtime.status()
               .mempool_transactions == 0U);

    const auto fresh =
        runtime.preview_send(
            destination,
            amount
        );
    assert(fresh.ok());

    auto tampered = fresh;
    ++tampered.amount;

    const auto invalid =
        runtime.confirm_send(
            tampered
        );

    assert(!invalid.ok());
    assert(invalid.error ==
           NetworkWalletSendError::
               invalid_preview);

    const auto missing_password =
        runtime.confirm_send(
            fresh
        );

    assert(!missing_password.ok());
    assert(missing_password.error ==
           NetworkWalletSendError::
               passphrase_required);
    assert(runtime.status()
               .mempool_transactions == 0U);

    const auto wrong_password =
        runtime.confirm_send(
            fresh,
            "wrong-stage25-password"
        );

    assert(!wrong_password.ok());
    assert(wrong_password.error ==
           NetworkWalletSendError::
               invalid_passphrase);
    assert(runtime.status()
               .mempool_transactions == 0U);

    const auto sent =
        runtime.confirm_send(
            fresh,
            "stage25-runtime-password"
        );

    assert(sent.ok());
    assert(sent.wallet.amount ==
           amount);
    assert(sent.wallet.fee ==
           fresh.quote.fee);

    assert(runtime.set_transaction_label(
               sent.wallet.txid,
               "Supplier payment") ==
           WalletMetadataError::none);

    const auto after =
        runtime.desktop_snapshot();

    bool found{false};

    for (const auto& tx :
         after.transactions) {
        if (tx.record.txid ==
            sent.wallet.txid) {
            found = true;
            assert(tx.label ==
                   std::optional<std::string>{
                       "Supplier payment"});
        }
    }

    assert(found);

    runtime.stop();

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

} // namespace

int main()
{
    test_wallet_metadata_persists_and_validates_network();
    test_metadata_is_bound_to_wallet();
    test_corrupt_metadata_fails_loudly();
    test_desktop_snapshot_preview_confirm_and_stale_guard();
    return 0;
}
