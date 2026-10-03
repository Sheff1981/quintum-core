#include "crypto/secp256k1.hpp"
#include "wallet/mnemonic.hpp"
#include "wallet/secure.hpp"

#include <array>
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

} // namespace

int main()
{
    test_bip39_256_entropy_vector();
    test_mnemonic_roundtrip_preserves_quintum_keys();
    test_mnemonic_validation_rejects_bad_input();
    return 0;
}
