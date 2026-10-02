#pragma once

#include "crypto/secp256k1.hpp"

#include <optional>
#include <span>

namespace quintum::crypto {

[[nodiscard]] bool secure_random_bytes(
    std::span<Byte> output
) noexcept;

[[nodiscard]] std::optional<PrivateKey>
generate_private_key() noexcept;

void secure_erase(
    std::span<Byte> bytes
) noexcept;

} // namespace quintum::crypto
