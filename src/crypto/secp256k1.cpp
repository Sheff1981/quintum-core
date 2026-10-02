#include "crypto/secp256k1.hpp"

#include <secp256k1.h>

#include <cstddef>
#include <memory>

namespace quintum::crypto {
namespace {

struct ContextDeleter {
    void operator()(secp256k1_context* context) const noexcept
    {
        secp256k1_context_destroy(context);
    }
};

secp256k1_context* context() noexcept
{
    static const std::unique_ptr<secp256k1_context, ContextDeleter> instance{
        secp256k1_context_create(SECP256K1_CONTEXT_NONE)
    };
    return instance.get();
}

} // namespace

bool is_valid_private_key(const PrivateKey& key) noexcept
{
    return context() != nullptr &&
           secp256k1_ec_seckey_verify(context(), key.data()) == 1;
}

bool is_valid_public_key(const PublicKey& key) noexcept
{
    if (context() == nullptr) {
        return false;
    }

    secp256k1_pubkey parsed{};
    return secp256k1_ec_pubkey_parse(
        context(),
        &parsed,
        key.data(),
        key.size()
    ) == 1;
}

std::optional<PublicKey> derive_public_key(
    const PrivateKey& key) noexcept
{
    if (!is_valid_private_key(key)) {
        return std::nullopt;
    }

    secp256k1_pubkey parsed{};
    if (secp256k1_ec_pubkey_create(
            context(),
            &parsed,
            key.data()) != 1) {
        return std::nullopt;
    }

    PublicKey serialized{};
    std::size_t size = serialized.size();

    if (secp256k1_ec_pubkey_serialize(
            context(),
            serialized.data(),
            &size,
            &parsed,
            SECP256K1_EC_COMPRESSED) != 1 ||
        size != serialized.size()) {
        return std::nullopt;
    }

    return serialized;
}

std::optional<CompactSignature> sign_ecdsa(
    const Hash256& digest,
    const PrivateKey& key) noexcept
{
    if (!is_valid_private_key(key)) {
        return std::nullopt;
    }

    secp256k1_ecdsa_signature signature{};
    if (secp256k1_ecdsa_sign(
            context(),
            &signature,
            digest.data(),
            key.data(),
            nullptr,
            nullptr) != 1) {
        return std::nullopt;
    }

    // libsecp256k1 signing already emits low-S signatures.
    CompactSignature serialized{};
    secp256k1_ecdsa_signature_serialize_compact(
        context(),
        serialized.data(),
        &signature
    );

    return serialized;
}

bool verify_ecdsa(
    const Hash256& digest,
    const CompactSignature& signature,
    const PublicKey& public_key) noexcept
{
    if (context() == nullptr) {
        return false;
    }

    secp256k1_pubkey parsed_public_key{};
    if (secp256k1_ec_pubkey_parse(
            context(),
            &parsed_public_key,
            public_key.data(),
            public_key.size()) != 1) {
        return false;
    }

    secp256k1_ecdsa_signature parsed_signature{};
    if (secp256k1_ecdsa_signature_parse_compact(
            context(),
            &parsed_signature,
            signature.data()) != 1) {
        return false;
    }

    secp256k1_ecdsa_signature normalized{};
    if (secp256k1_ecdsa_signature_normalize(
            context(),
            &normalized,
            &parsed_signature) == 1) {
        // Reject high-S malleable form instead of silently normalizing it.
        return false;
    }

    return secp256k1_ecdsa_verify(
        context(),
        &parsed_signature,
        digest.data(),
        &parsed_public_key
    ) == 1;
}

} // namespace quintum::crypto
