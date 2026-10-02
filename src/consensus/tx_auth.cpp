#include "consensus/tx_auth.hpp"

#include "core/serialize.hpp"
#include "crypto/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace quintum::consensus {
namespace {

void append_sized_bytes(Bytes& out, const Bytes& bytes)
{
    append_compact_size(out, static_cast<std::uint64_t>(bytes.size()));
    out.insert(out.end(), bytes.begin(), bytes.end());
}

void append_domain(Bytes& out)
{
    constexpr std::string_view domain{"QUINTUM-SIGHASH-V1"};
    append_compact_size(out, static_cast<std::uint64_t>(domain.size()));

    for (const char ch : domain) {
        out.push_back(static_cast<Byte>(
            static_cast<unsigned char>(ch)
        ));
    }
}

} // namespace

Bytes make_p2pk_locking_script(
    const crypto::PublicKey& public_key)
{
    Bytes script;
    script.reserve(1U + public_key.size());
    script.push_back(kP2pkLockVersion);
    script.insert(
        script.end(),
        public_key.begin(),
        public_key.end()
    );
    return script;
}

std::optional<crypto::PublicKey> parse_p2pk_locking_script(
    const Bytes& script)
{
    if (script.size() != 34U ||
        script.front() != kP2pkLockVersion) {
        return std::nullopt;
    }

    crypto::PublicKey public_key{};
    std::copy(
        script.begin() + 1,
        script.end(),
        public_key.begin()
    );
    return public_key;
}

Bytes make_p2pk_unlocking_script(
    const crypto::CompactSignature& signature)
{
    Bytes script;
    script.reserve(1U + signature.size());
    script.push_back(kP2pkUnlockVersion);
    script.insert(
        script.end(),
        signature.begin(),
        signature.end()
    );
    return script;
}

std::optional<crypto::CompactSignature>
parse_p2pk_unlocking_script(const Bytes& script)
{
    if (script.size() != 65U ||
        script.front() != kP2pkUnlockVersion) {
        return std::nullopt;
    }

    crypto::CompactSignature signature{};
    std::copy(
        script.begin() + 1,
        script.end(),
        signature.begin()
    );
    return signature;
}

std::optional<Hash256> signature_hash(
    const Transaction& tx,
    std::size_t input_index,
    const TxOutput& previous_output)
{
    if (input_index >= tx.inputs.size() ||
        input_index >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint32_t>::max())) {
        return std::nullopt;
    }

    Bytes preimage;
    append_domain(preimage);

    append_little_endian(preimage, tx.version);

    append_compact_size(
        preimage,
        static_cast<std::uint64_t>(tx.inputs.size())
    );

    for (const auto& input : tx.inputs) {
        preimage.insert(
            preimage.end(),
            input.previous_output.txid.begin(),
            input.previous_output.txid.end()
        );
        append_little_endian(
            preimage,
            input.previous_output.index
        );
        append_little_endian(
            preimage,
            input.sequence
        );
    }

    append_compact_size(
        preimage,
        static_cast<std::uint64_t>(tx.outputs.size())
    );

    for (const auto& output : tx.outputs) {
        append_little_endian(preimage, output.value);
        append_sized_bytes(
            preimage,
            output.locking_script
        );
    }

    append_little_endian(preimage, tx.lock_time);
    append_little_endian(
        preimage,
        static_cast<std::uint32_t>(input_index)
    );

    append_little_endian(
        preimage,
        previous_output.value
    );
    append_sized_bytes(
        preimage,
        previous_output.locking_script
    );

    append_little_endian(preimage, kSighashAll);

    return crypto::double_sha256(preimage);
}

InputAuthError sign_p2pk_input(
    Transaction& tx,
    std::size_t input_index,
    const TxOutput& previous_output,
    const crypto::PrivateKey& private_key)
{
    if (input_index >= tx.inputs.size()) {
        return InputAuthError::input_index_out_of_range;
    }

    const auto expected_public_key =
        parse_p2pk_locking_script(
            previous_output.locking_script
        );

    if (!expected_public_key) {
        return InputAuthError::malformed_locking_script;
    }

    const auto public_key =
        crypto::derive_public_key(private_key);

    if (!public_key ||
        *public_key != *expected_public_key) {
        return InputAuthError::wrong_private_key;
    }

    const auto digest =
        signature_hash(tx, input_index, previous_output);

    if (!digest) {
        return InputAuthError::input_index_out_of_range;
    }

    const auto signature =
        crypto::sign_ecdsa(*digest, private_key);

    if (!signature) {
        return InputAuthError::invalid_signature;
    }

    tx.inputs[input_index].unlocking_script =
        make_p2pk_unlocking_script(*signature);

    return InputAuthError::none;
}

InputAuthError verify_input_authorization(
    const Transaction& tx,
    std::size_t input_index,
    const TxOutput& previous_output)
{
    if (input_index >= tx.inputs.size()) {
        return InputAuthError::input_index_out_of_range;
    }

    const auto public_key =
        parse_p2pk_locking_script(
            previous_output.locking_script
        );

    if (!public_key) {
        return InputAuthError::malformed_locking_script;
    }

    const auto signature =
        parse_p2pk_unlocking_script(
            tx.inputs[input_index].unlocking_script
        );

    if (!signature) {
        return InputAuthError::malformed_unlocking_script;
    }

    const auto digest =
        signature_hash(tx, input_index, previous_output);

    if (!digest) {
        return InputAuthError::input_index_out_of_range;
    }

    if (!crypto::verify_ecdsa(
            *digest,
            *signature,
            *public_key)) {
        return InputAuthError::invalid_signature;
    }

    return InputAuthError::none;
}

} // namespace quintum::consensus
