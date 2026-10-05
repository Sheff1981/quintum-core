#pragma once

#include "core/types.hpp"

#include <array>
#include <optional>

namespace quintum::crypto {

using PrivateKey = std::array<Byte, 32>;
using PublicKey = std::array<Byte, 33>;
using CompactSignature = std::array<Byte, 64>;
using EllSwiftPublicKey = std::array<Byte, 64>;

[[nodiscard]] bool is_valid_private_key(const PrivateKey& key) noexcept;
[[nodiscard]] bool is_valid_public_key(const PublicKey& key) noexcept;

[[nodiscard]] std::optional<PublicKey> derive_public_key(
    const PrivateKey& key
) noexcept;

[[nodiscard]] std::optional<PrivateKey>
tweak_add_private_key(
    const PrivateKey& key,
    const PrivateKey& tweak
) noexcept;

[[nodiscard]] std::optional<EllSwiftPublicKey> ellswift_public_key(
    const PrivateKey& key,
    const std::array<Byte, 32>& aux_random
) noexcept;

[[nodiscard]] std::optional<Hash256> ellswift_xdh_bip324(
    const PrivateKey& key,
    const EllSwiftPublicKey& initiator_public,
    const EllSwiftPublicKey& responder_public,
    bool initiating
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
