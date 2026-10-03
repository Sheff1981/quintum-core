#include "crypto/random.hpp"
#include "crypto/secp256k1.hpp"
#include "node/node.hpp"
#include "net/runtime.hpp"
#include "wallet/wallet.hpp"
#include "wallet/mnemonic.hpp"
#include "wallet/secure.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <cassert>
#include <string>
#include <string_view>

namespace {

quintum::wallet::RecoverySeed repeated_seed(
    quintum::Byte value)
{
    quintum::wallet::RecoverySeed seed{};
    seed.fill(value);
    return seed;
}

std::filesystem::path unique_dir(
    std::string_view suffix)
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    return std::filesystem::temp_directory_path() /
        ("quintum-stage23-" +
         std::string(suffix) + "-" +
         std::to_string(stamp));
}

quintum::crypto::PublicKey derived_public_key(
    const quintum::wallet::RecoverySeed& seed,
    bool internal,
    std::uint32_t index)
{
    using namespace quintum;

    auto private_key =
        wallet::derive_hd_private_key(
            seed,
            consensus::Network::regtest,
            internal,
            index
        );

    assert(private_key.has_value());

    const auto public_key =
        crypto::derive_public_key(
            *private_key
        );

    crypto::secure_erase(
        *private_key
    );

    assert(public_key.has_value());
    return *public_key;
}

quintum::Bytes derived_script(
    const quintum::wallet::RecoverySeed& seed,
    bool internal,
    std::uint32_t index)
{
    return quintum::consensus::
        make_p2pk_locking_script(
            derived_public_key(
                seed,
                internal,
                index
            )
        );
}

void test_bip39_256_entropy_vector()
{
    using namespace quintum::wallet;

    const RecoverySeed seed =
        repeated_seed(0x7fU);

    constexpr std::string_view expected =
        "legal winner thank year wave sausage worth useful "
        "legal winner thank year wave sausage worth useful "
        "legal winner thank year wave sausage worth title";

    const auto encoded =
        encode_recovery_mnemonic(seed);

    assert(encoded.ok());
    assert(encoded.words == expected);

    const auto decoded =
        decode_recovery_mnemonic(expected);

    assert(decoded.ok());
    assert(decoded.seed == seed);
}

void test_mnemonic_roundtrip_preserves_quintum_keys()
{
    using namespace quintum;
    using namespace quintum::wallet;

    RecoverySeed seed{};
    for (std::size_t i = 0U;
         i < seed.size();
         ++i) {
        seed[i] =
            static_cast<Byte>(i);
    }

    const auto phrase =
        encode_recovery_mnemonic(seed);

    assert(phrase.ok());

    const auto decoded =
        decode_recovery_mnemonic(
            phrase.words
        );

    assert(decoded.ok());

    for (const auto network : {
             consensus::Network::mainnet,
             consensus::Network::testnet,
             consensus::Network::regtest}) {
        for (const bool internal : {
                 false,
                 true}) {
            for (const std::uint32_t index : {
                     0U,
                     1U,
                     100U,
                     999U}) {
                auto original =
                    derive_hd_private_key(
                        seed,
                        network,
                        internal,
                        index
                    );
                auto restored =
                    derive_hd_private_key(
                        decoded.seed,
                        network,
                        internal,
                        index
                    );

                assert(original.has_value());
                assert(restored.has_value());
                assert(*original == *restored);

                crypto::secure_erase(
                    *original
                );
                crypto::secure_erase(
                    *restored
                );
            }
        }
    }

    crypto::secure_erase(seed);
}

void test_mnemonic_validation_rejects_bad_input()
{
    using namespace quintum::wallet;

    constexpr std::string_view valid =
        "legal winner thank year wave sausage worth useful "
        "legal winner thank year wave sausage worth useful "
        "legal winner thank year wave sausage worth title";

    std::string wrong_checksum{valid};
    const auto last_space =
        wrong_checksum.rfind(' ');
    assert(last_space !=
           std::string::npos);
    wrong_checksum.replace(
        last_space + 1U,
        std::string::npos,
        "zoo"
    );

    const auto checksum =
        decode_recovery_mnemonic(
            wrong_checksum
        );
    assert(!checksum.ok());
    assert(checksum.error ==
           MnemonicError::invalid_checksum);

    const auto unknown =
        decode_recovery_mnemonic(
            "legal winner thank year wave sausage worth useful "
            "legal winner thank year wave sausage worth useful "
            "legal winner thank year wave sausage worth nope"
        );
    assert(!unknown.ok());
    assert(unknown.error ==
           MnemonicError::unknown_word);

    const auto count =
        decode_recovery_mnemonic(
            "legal winner thank"
        );
    assert(!count.ok());
    assert(count.error ==
           MnemonicError::wrong_word_count);
}

void test_mnemonic_recovery_discovers_gap_and_persists()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto chain_directory =
        unique_dir("recovery-chain");
    const auto wallet_directory =
        unique_dir("recovery-wallet");

    const auto& params =
        consensus::regtest_params();

    const std::uint64_t base_time =
        params.genesis.timestamp + 60'000U;

    NodeRuntime node{
        params,
        chain_directory
    };

    assert(node.start_at(base_time).ok());

    RecoverySeed seed{};
    for (std::size_t i = 0U;
         i < seed.size();
         ++i) {
        seed[i] =
            static_cast<Byte>(
                0x40U + i
            );
    }

    struct FundedKey {
        bool internal;
        std::uint32_t index;
        Hash256 txid{};
    };

    std::array<FundedKey, 4> funded{{
        {false, 100U, {}},
        {false, 150U, {}},
        {true, 99U, {}},
        {true, 120U, {}},
    }};

    for (std::size_t i = 0U;
         i < funded.size();
         ++i) {
        const auto mined =
            node.mine_block_at(
                derived_script(
                    seed,
                    funded[i].internal,
                    funded[i].index
                ),
                base_time +
                    static_cast<std::uint64_t>(
                        i + 1U
                    ),
                4'096U
            );

        assert(mined.ok());
        funded[i].txid =
            transaction_id(
                mined.block.transactions.front()
            );
    }

    const auto encoded =
        encode_recovery_mnemonic(seed);

    assert(encoded.ok());

    Wallet recovered{
        params,
        wallet_directory
    };

    const auto recovery =
        recovered.recover_from_mnemonic(
            encoded.words,
            "stage23-password",
            node.chain(),
            node.mempool(),
            100U
        );

    assert(recovery.ok());
    assert(recovery.receive_keys == 251U);
    assert(recovery.change_keys == 221U);
    assert(recovery.sync.ok());
    assert(recovery.sync.index_rebuilt);
    assert(recovery.sync.blocks_scanned == 5U);

    assert(recovered.encrypted());
    assert(recovered.owns_public_key(
        derived_public_key(
            seed,
            false,
            150U
        )
    ));
    assert(recovered.owns_public_key(
        derived_public_key(
            seed,
            true,
            120U
        )
    ));

    const auto phrase =
        recovered.recovery_mnemonic();

    assert(phrase.has_value());
    assert(*phrase == encoded.words);

    const auto history =
        recovered.history();

    for (const auto& expected :
         funded) {
        const bool found =
            std::any_of(
                history.begin(),
                history.end(),
                [&](const auto& record) {
                    return record.txid ==
                           expected.txid &&
                           record.status ==
                               WalletTransactionStatus::
                                   confirmed;
                }
            );

        assert(found);
    }

    {
        Wallet restarted{
            params,
            wallet_directory
        };

        assert(restarted.start(
                   "stage23-password").ok());

        const auto sync =
            restarted.sync(
                node.chain(),
                node.mempool()
            );

        assert(sync.ok());
        assert(!sync.index_rebuilt);
        assert(sync.blocks_scanned == 0U);
        assert(restarted.history().size() ==
               history.size());

        const auto restarted_phrase =
            restarted.recovery_mnemonic();

        assert(restarted_phrase.has_value());
        assert(*restarted_phrase ==
               encoded.words);
    }

    crypto::secure_erase(seed);

    std::error_code ec;
    std::filesystem::remove_all(
        chain_directory,
        ec
    );
    std::filesystem::remove_all(
        wallet_directory,
        ec
    );
}

void test_runtime_exposes_mnemonic_only_explicitly()
{
    using namespace quintum;
    using namespace quintum::net;
    using namespace quintum::wallet;

    const auto directory =
        unique_dir("runtime-mnemonic");

    NetworkRuntime runtime{
        consensus::regtest_params(),
        directory
    };

    NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.target_outbound = 0U;
    config.accept_poll_ms = 10U;
    config.wallet_passphrase =
        "stage23-runtime-password";

    const auto started =
        runtime.start(
            std::move(config)
        );

    assert(started.ok());

    const auto phrase =
        runtime.wallet_recovery_mnemonic();

    assert(phrase.has_value());

    const auto decoded =
        decode_recovery_mnemonic(
            *phrase
        );

    assert(decoded.ok());

    runtime.stop();

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

void test_recovery_sync_failure_does_not_commit_wallet()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto chain_directory =
        unique_dir("atomic-chain");
    const auto wallet_directory =
        unique_dir("atomic-wallet");

    const auto& params =
        consensus::regtest_params();

    NodeRuntime node{
        params,
        chain_directory
    };

    assert(node.start_at(
               params.genesis.timestamp +
               65'000U).ok());

    RecoverySeed seed{};
    seed.fill(0x55U);

    const auto encoded =
        encode_recovery_mnemonic(seed);

    assert(encoded.ok());

    std::error_code ec;
    std::filesystem::create_directories(
        wallet_directory /
            "wallet_state.dat",
        ec
    );
    assert(!ec);

    Wallet wallet{
        params,
        wallet_directory
    };

    const auto recovery =
        wallet.recover_from_mnemonic(
            encoded.words,
            "stage23-password",
            node.chain(),
            node.mempool(),
            100U
        );

    assert(!recovery.ok());
    assert(recovery.error ==
           WalletRecoveryError::sync_failed);
    assert(!wallet.started());
    assert(!std::filesystem::exists(
        wallet_directory /
            "wallet.dat"
    ));

    crypto::secure_erase(seed);

    std::filesystem::remove_all(
        chain_directory,
        ec
    );
    std::filesystem::remove_all(
        wallet_directory,
        ec
    );
}

void test_invalid_mnemonic_never_creates_wallet()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto chain_directory =
        unique_dir("invalid-chain");
    const auto wallet_directory =
        unique_dir("invalid-wallet");

    const auto& params =
        consensus::regtest_params();

    NodeRuntime node{
        params,
        chain_directory
    };

    assert(node.start_at(
               params.genesis.timestamp +
               70'000U).ok());

    Wallet wallet{
        params,
        wallet_directory
    };

    constexpr std::string_view bad =
        "legal winner thank year wave sausage worth useful "
        "legal winner thank year wave sausage worth useful "
        "legal winner thank year wave sausage worth zoo";

    const auto recovery =
        wallet.recover_from_mnemonic(
            bad,
            "stage23-password",
            node.chain(),
            node.mempool(),
            100U
        );

    assert(!recovery.ok());
    assert(recovery.error ==
           WalletRecoveryError::
               invalid_mnemonic);
    assert(recovery.mnemonic_error ==
           MnemonicError::invalid_checksum);
    assert(!std::filesystem::exists(
        wallet_directory /
        "wallet.dat"
    ));

    std::error_code ec;
    std::filesystem::remove_all(
        chain_directory,
        ec
    );
    std::filesystem::remove_all(
        wallet_directory,
        ec
    );
}

} // namespace

int main()
{
    test_bip39_256_entropy_vector();
    test_mnemonic_roundtrip_preserves_quintum_keys();
    test_mnemonic_validation_rejects_bad_input();
    test_mnemonic_recovery_discovers_gap_and_persists();
    test_runtime_exposes_mnemonic_only_explicitly();
    test_recovery_sync_failure_does_not_commit_wallet();
    test_invalid_mnemonic_never_creates_wallet();
    return 0;
}
