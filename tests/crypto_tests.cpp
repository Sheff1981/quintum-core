#include "crypto/secp256k1.hpp"
#include "crypto/sha256.hpp"

#include <array>
#include <cassert>
#include <string>

namespace {

std::string to_hex(const quintum::crypto::PublicKey& key)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(key.size() * 2U);

    for (const auto byte : key) {
        out.push_back(digits[(byte >> 4U) & 0x0fU]);
        out.push_back(digits[byte & 0x0fU]);
    }
    return out;
}

quintum::crypto::PrivateKey private_key_one()
{
    quintum::crypto::PrivateKey key{};
    key.back() = 1U;
    return key;
}

void test_private_and_public_key_vector()
{
    const auto private_key = private_key_one();
    assert(quintum::crypto::is_valid_private_key(private_key));

    const auto public_key =
        quintum::crypto::derive_public_key(private_key);

    assert(public_key);
    assert(
        to_hex(*public_key) ==
        "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"
    );

    quintum::crypto::PrivateKey zero{};
    assert(!quintum::crypto::is_valid_private_key(zero));
    assert(!quintum::crypto::derive_public_key(zero));
}

void test_sign_verify_and_tamper()
{
    const auto private_key = private_key_one();
    const auto public_key =
        quintum::crypto::derive_public_key(private_key);
    assert(public_key);

    constexpr std::array<quintum::Byte, 4> message{
        0x51U, 0x55U, 0x49U, 0x4eU
    };

    const auto digest = quintum::crypto::sha256(message);
    const auto signature =
        quintum::crypto::sign_ecdsa(digest, private_key);

    assert(signature);
    assert(
        quintum::crypto::verify_ecdsa(
            digest,
            *signature,
            *public_key
        )
    );

    auto tampered_digest = digest;
    tampered_digest[0] ^= 0x01U;
    assert(
        !quintum::crypto::verify_ecdsa(
            tampered_digest,
            *signature,
            *public_key
        )
    );

    auto tampered_signature = *signature;
    tampered_signature[0] ^= 0x01U;
    assert(
        !quintum::crypto::verify_ecdsa(
            digest,
            tampered_signature,
            *public_key
        )
    );
}

} // namespace

int main()
{
    test_private_and_public_key_vector();
    test_sign_verify_and_tamper();
    return 0;
}
