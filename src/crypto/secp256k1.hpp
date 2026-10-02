#pragma once

#include "core/types.hpp"

#include <array>
#include <optional>

namespace quintum::crypto {

using PrivateKey = std::array<Byte, 32>;
using PublicKey = std::array<Byte, 33>;
using CompactSignature = std::array<Byte, 64>;

[[nodiscard]] bool is_valid_private_key(const PrivateKey& key) noexcept;
[[nodiscard]] bool is_valid_public_key(const PublicKey& key) noexcept;

[[nodiscard]] std::optional<PublicKey> derive_public_key(
    const PrivateKey& key
) noexcept;

[[nodiscard]] std::optional<CompactSignature> sign_ecdsa(
    const Hash256& digest,
    const PrivateKey& key
) noexcept;

[[nodiscard]] bool verify_ecdsa(
    const Hash256& digest,
    const CompactSignature& signature,
    const PublicKey& public_key
) noexcept;

} // namespace quintum::crypto
