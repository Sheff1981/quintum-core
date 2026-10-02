#pragma once

#include "crypto/secp256k1.hpp"
#include "primitives/transaction.hpp"

#include <cstddef>
#include <optional>

namespace quintum::consensus {

inline constexpr Byte kP2pkLockVersion = 0x01U;
inline constexpr Byte kP2pkUnlockVersion = 0x01U;
inline constexpr std::uint32_t kSighashAll = 1U;

enum class InputAuthError {
    none,
    input_index_out_of_range,
    malformed_locking_script,
    malformed_unlocking_script,
    wrong_private_key,
    invalid_signature,
};

[[nodiscard]] Bytes make_p2pk_locking_script(
    const crypto::PublicKey& public_key
);

[[nodiscard]] std::optional<crypto::PublicKey> parse_p2pk_locking_script(
    const Bytes& script
);

[[nodiscard]] Bytes make_p2pk_unlocking_script(
    const crypto::CompactSignature& signature
);

[[nodiscard]] std::optional<crypto::CompactSignature>
parse_p2pk_unlocking_script(const Bytes& script);

[[nodiscard]] std::optional<Hash256> signature_hash(
    const Transaction& tx,
    std::size_t input_index,
    const TxOutput& previous_output
);

[[nodiscard]] InputAuthError sign_p2pk_input(
    Transaction& tx,
    std::size_t input_index,
    const TxOutput& previous_output,
    const crypto::PrivateKey& private_key
);

[[nodiscard]] InputAuthError verify_input_authorization(
    const Transaction& tx,
    std::size_t input_index,
    const TxOutput& previous_output
);

} // namespace quintum::consensus
