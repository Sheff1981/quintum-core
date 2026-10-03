#include "wallet/secure.hpp"

#include "crypto/random.hpp"

#include <monocypher.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

namespace quintum::wallet {
namespace {

constexpr std::array<Byte, 13> kHdDomain{
    'Q', 'U', 'I', 'N', 'T', 'U', 'M',
    '-', 'H', 'D', '-', 'v', '1'
};

void store_le32(
    std::uint32_t value,
    Byte* output) noexcept
{
    output[0] = static_cast<Byte>(value);
    output[1] = static_cast<Byte>(value >> 8U);
    output[2] = static_cast<Byte>(value >> 16U);
    output[3] = static_cast<Byte>(value >> 24U);
}

} // namespace

bool derive_wallet_encryption_key(
    std::string_view passphrase,
    const WalletSalt& salt,
    std::uint32_t memory_blocks,
    std::uint32_t passes,
    WalletEncryptionKey& output)
{
    crypto::secure_erase(output);

    if (passphrase.empty() ||
        passphrase.size() >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint32_t>::max()) ||
        memory_blocks < 8U ||
        passes == 0U ||
        memory_blocks >
            std::numeric_limits<std::size_t>::max() /
                1024U) {
        return false;
    }

    const std::size_t work_size =
        static_cast<std::size_t>(memory_blocks) * 1024U;

    std::vector<Byte> work_area;

    try {
        work_area.resize(work_size);
    } catch (const std::bad_alloc&) {
        return false;
    }

    const crypto_argon2_config config{
        .algorithm = CRYPTO_ARGON2_ID,
        .nb_blocks = memory_blocks,
        .nb_passes = passes,
        .nb_lanes = 1U,
    };

    const crypto_argon2_inputs inputs{
        .pass = reinterpret_cast<const std::uint8_t*>(
            passphrase.data()),
        .salt = salt.data(),
        .pass_size = static_cast<std::uint32_t>(
            passphrase.size()),
        .salt_size = static_cast<std::uint32_t>(
            salt.size()),
    };

    crypto_argon2(
        output.data(),
        static_cast<std::uint32_t>(output.size()),
        work_area.data(),
        config,
        inputs,
        crypto_argon2_no_extras
    );

    crypto_wipe(
        work_area.data(),
        work_area.size()
    );

    return true;
}

bool encrypt_wallet_payload(
    std::span<const Byte> plaintext,
    std::span<const Byte> associated_data,
    const WalletEncryptionKey& key,
    const WalletNonce& nonce,
    Bytes& ciphertext,
    WalletTag& tag)
{
    ciphertext.assign(
        plaintext.begin(),
        plaintext.end()
    );

    crypto_aead_lock(
        ciphertext.data(),
        tag.data(),
        key.data(),
        nonce.data(),
        associated_data.empty()
            ? nullptr
            : associated_data.data(),
        associated_data.size(),
        plaintext.empty()
            ? nullptr
            : plaintext.data(),
        plaintext.size()
    );

    return true;
}

bool decrypt_wallet_payload(
    std::span<const Byte> ciphertext,
    std::span<const Byte> associated_data,
    const WalletEncryptionKey& key,
    const WalletNonce& nonce,
    const WalletTag& tag,
    Bytes& plaintext)
{
    plaintext.assign(
        ciphertext.size(),
        0U
    );

    const int result =
        crypto_aead_unlock(
            plaintext.data(),
            tag.data(),
            key.data(),
            nonce.data(),
            associated_data.empty()
                ? nullptr
                : associated_data.data(),
            associated_data.size(),
            ciphertext.empty()
                ? nullptr
                : ciphertext.data(),
            ciphertext.size()
        );

    if (result != 0) {
        crypto::secure_erase(plaintext);
        plaintext.clear();
        return false;
    }

    return true;
}

std::optional<crypto::PrivateKey>
derive_hd_private_key(
    const RecoverySeed& seed,
    consensus::Network network,
    bool internal,
    std::uint32_t index) noexcept
{
    std::array<Byte, 23> input{};

    std::copy(
        kHdDomain.begin(),
        kHdDomain.end(),
        input.begin()
    );

    input[13] =
        static_cast<Byte>(network);
    input[14] =
        internal ? 1U : 0U;

    store_le32(index, input.data() + 15U);

    crypto::PrivateKey candidate{};

    for (std::uint32_t attempt = 0U;
         attempt < 1'024U;
         ++attempt) {
        store_le32(
            attempt,
            input.data() + 19U
        );

        crypto_blake2b_keyed(
            candidate.data(),
            candidate.size(),
            seed.data(),
            seed.size(),
            input.data(),
            input.size()
        );

        if (crypto::is_valid_private_key(
                candidate)) {
            return candidate;
        }
    }

    crypto::secure_erase(candidate);
    return std::nullopt;
}

} // namespace quintum::wallet
