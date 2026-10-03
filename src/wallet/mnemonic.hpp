#pragma once

#include "wallet/secure.hpp"

#include <string>
#include <string_view>

namespace quintum::wallet {

enum class MnemonicError {
    none,
    wrong_word_count,
    unknown_word,
    invalid_checksum,
};

struct MnemonicEncodeResult {
    MnemonicError error{MnemonicError::none};
    std::string words{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == MnemonicError::none;
    }
};

struct MnemonicDecodeResult {
    MnemonicError error{MnemonicError::none};
    RecoverySeed seed{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == MnemonicError::none;
    }
};

// QUINTUM preserves the existing 256-bit RecoverySeed as the BIP32 input.
// These helpers use the BIP39 English entropy->mnemonic/checksum encoding
// as a reversible human representation of that exact 32-byte value.
[[nodiscard]] MnemonicEncodeResult encode_recovery_mnemonic(
    const RecoverySeed& seed
);

[[nodiscard]] MnemonicDecodeResult decode_recovery_mnemonic(
    std::string_view words
);

} // namespace quintum::wallet
