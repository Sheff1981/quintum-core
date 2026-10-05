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

Bytes make_provably_unspendable_script(
    std::span<const Byte> payload)
{
    Bytes script;
    script.reserve(1U + payload.size());
    script.push_back(kProvablyUnspendableLockVersion);
    script.insert(
        script.end(),
        payload.begin(),
        payload.end()
    );
    return script;
}

bool is_provably_unspendable(
    const Bytes& script) noexcept
{
    return !script.empty() &&
           script.front() ==
               kProvablyUnspendableLockVersion;
}

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


bool MultisigPolicy::valid() const noexcept
{
    if (threshold == 0U ||
        public_keys.empty() ||
        public_keys.size() >
            kMaxMultisigKeys ||
        threshold > public_keys.size()) {
        return false;
    }

    for (std::size_t i = 0U;
         i < public_keys.size();
         ++i) {
        if (!crypto::is_valid_public_key(
                public_keys[i])) {
            return false;
        }

        for (std::size_t j = i + 1U;
             j < public_keys.size();
             ++j) {
            if (public_keys[i] ==
                public_keys[j]) {
                return false;
            }
        }
    }

    return true;
}

std::optional<Bytes>
make_multisig_locking_script(
    std::uint8_t threshold,
    std::span<const crypto::PublicKey> public_keys)
{
    MultisigPolicy policy{
        .threshold = threshold,
        .public_keys =
            std::vector<crypto::PublicKey>(
                public_keys.begin(),
                public_keys.end()
            ),
    };

    if (!policy.valid()) {
        return std::nullopt;
    }

    Bytes script;
    script.reserve(
        3U +
        policy.public_keys.size() *
            crypto::PublicKey{}.size()
    );

    script.push_back(
        kMultisigLockVersion
    );
    script.push_back(threshold);
    script.push_back(
        static_cast<Byte>(
            policy.public_keys.size())
    );

    for (const auto& key :
         policy.public_keys) {
        script.insert(
            script.end(),
            key.begin(),
            key.end()
        );
    }

    return script;
}

std::optional<MultisigPolicy>
parse_multisig_locking_script(
    const Bytes& script)
{
    if (script.size() < 3U ||
        script[0] !=
            kMultisigLockVersion) {
        return std::nullopt;
    }

    const std::uint8_t threshold =
        script[1];
    const std::size_t count =
        script[2];

    if (count == 0U ||
        count > kMaxMultisigKeys ||
        threshold == 0U ||
        threshold > count) {
        return std::nullopt;
    }

    const std::size_t key_size =
        crypto::PublicKey{}.size();

    if (script.size() !=
        3U + count * key_size) {
        return std::nullopt;
    }

    MultisigPolicy policy;
    policy.threshold = threshold;
    policy.public_keys.reserve(count);

    std::size_t offset{3U};

    for (std::size_t i = 0U;
         i < count;
         ++i) {
        crypto::PublicKey key{};

        std::copy_n(
            script.begin() +
                static_cast<
                    std::ptrdiff_t>(offset),
            key_size,
            key.begin()
        );

        policy.public_keys.push_back(
            key
        );
        offset += key_size;
    }

    if (!policy.valid()) {
        return std::nullopt;
    }

    return policy;
}

std::optional<Bytes>
make_multisig_unlocking_script(
    std::span<const MultisigSignature> signatures)
{
    if (signatures.empty() ||
        signatures.size() >
            kMaxMultisigKeys) {
        return std::nullopt;
    }

    std::uint8_t previous{0U};
    bool have_previous{false};

    Bytes script;
    script.reserve(
        2U +
        signatures.size() *
            (1U +
             crypto::CompactSignature{}.
                 size())
    );

    script.push_back(
        kMultisigUnlockVersion
    );
    script.push_back(
        static_cast<Byte>(
            signatures.size())
    );

    for (const auto& item :
         signatures) {
        if (have_previous &&
            item.key_index <= previous) {
            return std::nullopt;
        }

        previous = item.key_index;
        have_previous = true;

        script.push_back(
            item.key_index
        );
        script.insert(
            script.end(),
            item.signature.begin(),
            item.signature.end()
        );
    }

    return script;
}

std::optional<std::vector<MultisigSignature>>
parse_multisig_unlocking_script(
    const Bytes& script)
{
    if (script.size() < 2U ||
        script[0] !=
            kMultisigUnlockVersion) {
        return std::nullopt;
    }

    const std::size_t count =
        script[1];

    if (count == 0U ||
        count > kMaxMultisigKeys) {
        return std::nullopt;
    }

    const std::size_t signature_size =
        crypto::CompactSignature{}.
            size();

    if (script.size() !=
        2U +
            count *
                (1U + signature_size)) {
        return std::nullopt;
    }

    std::vector<MultisigSignature> out;
    out.reserve(count);

    std::size_t offset{2U};
    std::uint8_t previous{0U};
    bool have_previous{false};

    for (std::size_t i = 0U;
         i < count;
         ++i) {
        const std::uint8_t index =
            script[offset++];

        if (have_previous &&
            index <= previous) {
            return std::nullopt;
        }

        previous = index;
        have_previous = true;

        MultisigSignature item;
        item.key_index = index;

        std::copy_n(
            script.begin() +
                static_cast<
                    std::ptrdiff_t>(offset),
            signature_size,
            item.signature.begin()
        );

        offset += signature_size;
        out.push_back(item);
    }

    return out;
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

    if (is_provably_unspendable(
            previous_output.locking_script)) {
        return InputAuthError::provably_unspendable;
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

    if (is_provably_unspendable(
            previous_output.locking_script)) {
        return InputAuthError::provably_unspendable;
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

std::optional<MultisigSignature>
sign_multisig_signature(
    const Transaction& tx,
    std::size_t input_index,
    const TxOutput& previous_output,
    std::uint8_t key_index,
    const crypto::PrivateKey& private_key)
{
    if (input_index >= tx.inputs.size()) {
        return std::nullopt;
    }

    const auto policy =
        parse_multisig_locking_script(
            previous_output.locking_script
        );

    if (!policy ||
        key_index >=
            policy->public_keys.size()) {
        return std::nullopt;
    }

    const auto public_key =
        crypto::derive_public_key(
            private_key
        );

    if (!public_key ||
        *public_key !=
            policy->public_keys[
                key_index]) {
        return std::nullopt;
    }

    const auto digest =
        signature_hash(
            tx,
            input_index,
            previous_output
        );

    if (!digest) {
        return std::nullopt;
    }

    const auto signature =
        crypto::sign_ecdsa(
            *digest,
            private_key
        );

    if (!signature) {
        return std::nullopt;
    }

    return MultisigSignature{
        .key_index = key_index,
        .signature = *signature,
    };
}

InputAuthError verify_multisig_authorization(
    const Transaction& tx,
    std::size_t input_index,
    const TxOutput& previous_output)
{
    if (input_index >=
        tx.inputs.size()) {
        return InputAuthError::
            input_index_out_of_range;
    }

    const auto policy =
        parse_multisig_locking_script(
            previous_output.locking_script
        );

    if (!policy) {
        return InputAuthError::
            malformed_locking_script;
    }

    const auto signatures =
        parse_multisig_unlocking_script(
            tx.inputs[input_index].
                unlocking_script
        );

    if (!signatures) {
        return InputAuthError::
            malformed_unlocking_script;
    }

    if (signatures->size() <
        policy->threshold) {
        return InputAuthError::
            insufficient_signatures;
    }

    if (signatures->size() >
        policy->threshold) {
        return InputAuthError::
            malformed_unlocking_script;
    }

    const auto digest =
        signature_hash(
            tx,
            input_index,
            previous_output
        );

    if (!digest) {
        return InputAuthError::
            input_index_out_of_range;
    }

    for (const auto& item :
         *signatures) {
        if (item.key_index >=
            policy->public_keys.size()) {
            return InputAuthError::
                malformed_unlocking_script;
        }

        if (!crypto::verify_ecdsa(
                *digest,
                item.signature,
                policy->public_keys[
                    item.key_index])) {
            return InputAuthError::
                invalid_signature;
        }
    }

    return InputAuthError::none;
}

} // namespace quintum::consensus
