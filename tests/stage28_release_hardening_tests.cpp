#include "consensus/chainparams.hpp"
#include "net/runtime.hpp"
#include "wallet/wallet.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::filesystem::path unique_dir(std::string_view suffix)
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    return std::filesystem::temp_directory_path() /
        ("quintum-stage28-" +
         std::string{suffix} + "-" +
         std::to_string(stamp));
}

std::vector<unsigned char> read_bytes(
    const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    assert(in);
    return {
        std::istreambuf_iterator<char>{in},
        std::istreambuf_iterator<char>{}
    };
}

bool contains_text(
    const std::vector<unsigned char>& bytes,
    std::string_view text)
{
    return std::search(
               bytes.begin(),
               bytes.end(),
               text.begin(),
               text.end()
           ) != bytes.end();
}

void test_encrypted_metadata_roundtrip_and_tamper()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto directory =
        unique_dir("encrypted-meta");
    const auto& params =
        consensus::regtest_params();

    std::string address;
    Hash256 txid{};
    txid.back() = 0x42U;

    {
        Wallet wallet{params, directory};

        const auto started =
            wallet.start("stage28-password");

        assert(started.ok());
        address = started.receive_address;
        assert(!address.empty());

        assert(wallet.set_address_label(
                   address,
                   "Top Secret Counterparty") ==
               WalletMetadataError::none);

        assert(wallet.set_transaction_label(
                   txid,
                   "Private invoice note") ==
               WalletMetadataError::none);
    }

    const auto metadata_path =
        directory / "wallet_meta.dat";
    auto encrypted =
        read_bytes(metadata_path);

    assert(encrypted.size() > 64U);
    assert(!contains_text(
        encrypted,
        "Top Secret Counterparty"));
    assert(!contains_text(
        encrypted,
        "Private invoice note"));
    assert(!contains_text(
        encrypted,
        address));

    {
        Wallet reopened{params, directory};

        const auto started =
            reopened.start("stage28-password");

        assert(started.ok());

        const auto entries =
            reopened.address_book();

        assert(entries.size() == 1U);
        assert(entries.front().address == address);
        assert(entries.front().label ==
               "Top Secret Counterparty");

        assert(reopened.transaction_label(txid) ==
               std::optional<std::string>{
                   "Private invoice note"});
    }

    // Authenticated metadata must fail closed after any ciphertext/tag change.
    encrypted[encrypted.size() - 17U] ^=
        static_cast<unsigned char>(0x01U);

    {
        std::ofstream out(
            metadata_path,
            std::ios::binary |
                std::ios::trunc);
        assert(out);
        out.write(
            reinterpret_cast<const char*>(
                encrypted.data()),
            static_cast<std::streamsize>(
                encrypted.size()));
        assert(out);
    }

    {
        Wallet tampered{params, directory};

        const auto started =
            tampered.start("stage28-password");

        assert(!started.ok());
        assert(started.error ==
               WalletStartError::metadata_failed);
        assert(started.metadata_error ==
               WalletMetadataError::corrupt);
    }

    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
}

void test_legacy_plaintext_metadata_migrates_on_encrypted_open()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto directory =
        unique_dir("legacy-migration");
    const auto& params =
        consensus::regtest_params();

    std::string address;

    {
        Wallet legacy{params, directory};

        const auto started =
            legacy.start("");

        assert(started.ok());
        assert(!legacy.encrypted());

        address = started.receive_address;

        assert(legacy.set_address_label(
                   address,
                   "Legacy plaintext label") ==
               WalletMetadataError::none);

        const auto plaintext =
            read_bytes(
                directory / "wallet_meta.dat");

        assert(contains_text(
            plaintext,
            "Legacy plaintext label"));

        assert(legacy.encrypt_wallet(
                   "stage28-migration-password") ==
               WalletStoreError::none);

        const auto encrypted_now =
            read_bytes(
                directory / "wallet_meta.dat");

        assert(!contains_text(
            encrypted_now,
            "Legacy plaintext label"));
        assert(!contains_text(
            encrypted_now,
            address));
    }

    // Reopening verifies the encrypted metadata survives restart.
    {
        Wallet reopened{params, directory};

        const auto started =
            reopened.start(
                "stage28-migration-password");

        assert(started.ok());

        const auto entries =
            reopened.address_book();

        assert(entries.size() == 1U);
        assert(entries.front().label ==
               "Legacy plaintext label");
    }

    const auto migrated =
        read_bytes(
            directory / "wallet_meta.dat");

    assert(!contains_text(
        migrated,
        "Legacy plaintext label"));
    assert(!contains_text(
        migrated,
        address));

    std::error_code ec;
    std::filesystem::remove_all(directory, ec);
}

void test_full_backup_bundle_roundtrip_and_guards()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto source_dir =
        unique_dir("bundle-source");
    const auto target_dir =
        unique_dir("bundle-target");
    const auto existing_dir =
        unique_dir("bundle-existing");
    const auto bundle_path =
        unique_dir("bundle-file") /
        "wallet.qtmbackup";

    const auto& params =
        consensus::regtest_params();

    std::string address;

    {
        Wallet source{params, source_dir};

        const auto started =
            source.start("bundle-password");

        assert(started.ok());
        address = started.receive_address;

        assert(source.set_address_label(
                   address,
                   "Bundle contact") ==
               WalletMetadataError::none);

        assert(source.backup_bundle(
                   bundle_path) ==
               WalletStoreError::none);

        assert(source.backup_bundle(
                   bundle_path) ==
               WalletStoreError::target_exists);
    }

    {
        Wallet target{params, target_dir};

        assert(target.restore_bundle(
                   bundle_path) ==
               WalletStoreError::none);

        const auto started =
            target.start("bundle-password");

        assert(started.ok());

        const auto entries =
            target.address_book();

        assert(entries.size() == 1U);
        assert(entries.front().address == address);
        assert(entries.front().label ==
               "Bundle contact");
    }

    {
        Wallet wrong_network{
            consensus::testnet_params(),
            unique_dir("bundle-wrong-network")
        };

        assert(wrong_network.restore_bundle(
                   bundle_path) ==
               WalletStoreError::wrong_network);
    }

    {
        Wallet existing{params, existing_dir};

        assert(existing.start(
                   "existing-password").ok());

        // A restore API must never overwrite an existing wallet.dat.
        assert(existing.restore_bundle(
                   bundle_path) ==
               WalletStoreError::target_exists);
    }

    auto tampered =
        read_bytes(bundle_path);
    assert(tampered.size() > 40U);
    tampered[tampered.size() / 2U] ^=
        static_cast<unsigned char>(0x80U);

    const auto tampered_path =
        bundle_path.parent_path() /
        "tampered.qtmbackup";

    {
        std::ofstream out(
            tampered_path,
            std::ios::binary |
                std::ios::trunc);
        assert(out);
        out.write(
            reinterpret_cast<const char*>(
                tampered.data()),
            static_cast<std::streamsize>(
                tampered.size()));
        assert(out);
    }

    {
        Wallet tampered_target{
            params,
            unique_dir("bundle-tampered-target")
        };

        assert(tampered_target.restore_bundle(
                   tampered_path) ==
               WalletStoreError::corrupt);
    }

    std::error_code ec;
    std::filesystem::remove_all(source_dir, ec);
    ec.clear();
    std::filesystem::remove_all(target_dir, ec);
    ec.clear();
    std::filesystem::remove_all(existing_dir, ec);
    ec.clear();
    std::filesystem::remove_all(
        bundle_path.parent_path(),
        ec
    );
}

} // namespace

int main()
{
    test_encrypted_metadata_roundtrip_and_tamper();
    test_legacy_plaintext_metadata_migrates_on_encrypted_open();
    test_full_backup_bundle_roundtrip_and_guards();
    return 0;
}
