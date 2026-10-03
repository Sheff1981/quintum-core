#pragma once

#include "consensus/chainparams.hpp"
#include "core/types.hpp"
#include "core/serialize.hpp"
#include "crypto/secp256k1.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace quintum::wallet {

using RecoverySeed = std::array<Byte, 32>;
using WalletEncryptionKey = std::array<Byte, 32>;
using WalletSalt = std::array<Byte, 16>;
using WalletNonce = std::array<Byte, 24>;
using WalletTag = std::array<Byte, 16>;

inline constexpr std::uint32_t kWalletArgon2MemoryBlocks = 65'536U;
inline constexpr std::uint32_t kWalletArgon2Passes = 3U;

[[nodiscard]] bool derive_wallet_encryption_key(
    std::string_view passphrase,
    const WalletSalt& salt,
    std::uint32_t memory_blocks,
    std::uint32_t passes,
    WalletEncryptionKey& output
);

[[nodiscard]] bool encrypt_wallet_payload(
    std::span<const Byte> plaintext,
    std::span<const Byte> associated_data,
    const WalletEncryptionKey& key,
    const WalletNonce& nonce,
    Bytes& ciphertext,
    WalletTag& tag
);

[[nodiscard]] bool decrypt_wallet_payload(
    std::span<const Byte> ciphertext,
    std::span<const Byte> associated_data,
    const WalletEncryptionKey& key,
    const WalletNonce& nonce,
    const WalletTag& tag,
    Bytes& plaintext
);

[[nodiscard]] std::optional<crypto::PrivateKey>
derive_hd_private_key(
    const RecoverySeed& seed,
    consensus::Network network,
    bool internal,
    std::uint32_t index
) noexcept;

} // namespace quintum::wallet
