#include "wallet/secure.hpp"

#include "crypto/random.hpp"

#include <monocypher.h>
#include <monocypher-ed25519.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

namespace quintum::wallet {
namespace {

constexpr std::uint32_t kBip32Hardened{0x80000000U};
constexpr std::uint32_t kQuintumPurpose{5'329'997U};
constexpr std::array<Byte, 12> kBip32SeedKey{
    'B', 'i', 't', 'c', 'o', 'i', 'n',
    ' ', 's', 'e', 'e', 'd'
};

using ChainCode = std::array<Byte, 32>;

struct ExtendedPrivateKey {
    crypto::PrivateKey private_key{};
    ChainCode chain_code{};
};

void store_be32(
    std::uint32_t value,
    Byte* output) noexcept
{
    output[0] = static_cast<Byte>(value >> 24U);
    output[1] = static_cast<Byte>(value >> 16U);
    output[2] = static_cast<Byte>(value >> 8U);
    output[3] = static_cast<Byte>(value);
}

void wipe_extended_key(
    ExtendedPrivateKey& key) noexcept
{
    crypto::secure_erase(key.private_key);
    crypto::secure_erase(key.chain_code);
}

bool bip32_master(
    const RecoverySeed& seed,
    ExtendedPrivateKey& output) noexcept
{
    std::array<Byte, 64> digest{};

    crypto_sha512_hmac(
        digest.data(),
        kBip32SeedKey.data(),
        kBip32SeedKey.size(),
        seed.data(),
        seed.size()
    );

    std::copy_n(
        digest.begin(),
        static_cast<std::ptrdiff_t>(
            output.private_key.size()),
        output.private_key.begin()
    );

    std::copy_n(
        digest.begin() +
            static_cast<std::ptrdiff_t>(
                output.private_key.size()),
        static_cast<std::ptrdiff_t>(
            output.chain_code.size()),
        output.chain_code.begin()
    );

    crypto_wipe(
        digest.data(),
        digest.size()
    );

    if (!crypto::is_valid_private_key(
            output.private_key)) {
        wipe_extended_key(output);
        return false;
    }

    return true;
}

bool bip32_child(
    const ExtendedPrivateKey& parent,
    std::uint32_t child_number,
    ExtendedPrivateKey& output) noexcept
{
    std::array<Byte, 37> data{};

    if ((child_number &
         kBip32Hardened) != 0U) {
        data[0] = 0U;
        std::copy(
            parent.private_key.begin(),
            parent.private_key.end(),
            data.begin() + 1
        );
    } else {
        const auto public_key =
            crypto::derive_public_key(
                parent.private_key
            );

        if (!public_key) {
            return false;
        }

        std::copy(
            public_key->begin(),
            public_key->end(),
            data.begin()
        );
    }

    store_be32(
        child_number,
        data.data() + 33U
    );

    std::array<Byte, 64> digest{};

    crypto_sha512_hmac(
        digest.data(),
        parent.chain_code.data(),
        parent.chain_code.size(),
        data.data(),
        data.size()
    );

    crypto_wipe(
        data.data(),
        data.size()
    );

    crypto::PrivateKey tweak{};
    std::copy_n(
        digest.begin(),
        static_cast<std::ptrdiff_t>(
            tweak.size()),
        tweak.begin()
    );

    if (!crypto::is_valid_private_key(
            tweak)) {
        crypto_wipe(
            digest.data(),
            digest.size()
        );
        crypto::secure_erase(tweak);
        return false;
    }

    const auto child_private =
        crypto::tweak_add_private_key(
            parent.private_key,
            tweak
        );

    crypto::secure_erase(tweak);

    if (!child_private) {
        crypto_wipe(
            digest.data(),
            digest.size()
        );
        return false;
    }

    output.private_key =
        *child_private;
    crypto::secure_erase(
        *child_private
    );

    std::copy_n(
        digest.begin() + 32,
        static_cast<std::ptrdiff_t>(
            output.chain_code.size()),
        output.chain_code.begin()
    );

    crypto_wipe(
        digest.data(),
        digest.size()
    );

    return true;
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
    } catch (...) {
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
    if (index >= kBip32Hardened) {
        return std::nullopt;
    }

    ExtendedPrivateKey current{};

    if (!bip32_master(
            seed,
            current)) {
        return std::nullopt;
    }

    const std::array<std::uint32_t, 4> path{
        kBip32Hardened |
            kQuintumPurpose,
        kBip32Hardened |
            static_cast<std::uint32_t>(
                network),
        internal ? 1U : 0U,
        index,
    };

    for (const auto child_number : path) {
        ExtendedPrivateKey child{};

        if (!bip32_child(
                current,
                child_number,
                child)) {
            wipe_extended_key(current);
            wipe_extended_key(child);
            return std::nullopt;
        }

        wipe_extended_key(current);
        current = child;
        wipe_extended_key(child);
    }

    crypto::PrivateKey result =
        current.private_key;

    wipe_extended_key(current);
    return result;
}

} // namespace quintum::wallet
