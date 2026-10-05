#include "net/v2_transport.hpp"

#include "core/serialize.hpp"
#include "crypto/random.hpp"
#include "crypto/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

namespace quintum::net {
namespace {

using Key32 = std::array<Byte, 32>;

Hash256 hmac_sha256(
    std::span<const Byte> key,
    std::span<const Byte> data)
{
    std::array<Byte, 64> normalized{};
    if (key.size() > normalized.size()) {
        const auto digest = crypto::sha256(key);
        std::copy(
            digest.begin(),
            digest.end(),
            normalized.begin()
        );
    } else {
        std::copy(
            key.begin(),
            key.end(),
            normalized.begin()
        );
    }

    std::array<Byte, 64> inner_pad{};
    std::array<Byte, 64> outer_pad{};
    for (std::size_t i = 0U; i < normalized.size(); ++i) {
        inner_pad[i] =
            static_cast<Byte>(normalized[i] ^ 0x36U);
        outer_pad[i] =
            static_cast<Byte>(normalized[i] ^ 0x5cU);
    }

    Bytes inner;
    inner.reserve(inner_pad.size() + data.size());
    inner.insert(inner.end(), inner_pad.begin(), inner_pad.end());
    inner.insert(inner.end(), data.begin(), data.end());
    const auto inner_hash = crypto::sha256(inner);

    Bytes outer;
    outer.reserve(outer_pad.size() + inner_hash.size());
    outer.insert(outer.end(), outer_pad.begin(), outer_pad.end());
    outer.insert(outer.end(), inner_hash.begin(), inner_hash.end());
    const auto result = crypto::sha256(outer);

    crypto::secure_erase(normalized);
    crypto::secure_erase(inner_pad);
    crypto::secure_erase(outer_pad);
    crypto::secure_erase(inner);
    crypto::secure_erase(outer);

    return result;
}

Hash256 hkdf_extract(
    std::span<const Byte> salt,
    std::span<const Byte> input_key_material)
{
    return hmac_sha256(salt, input_key_material);
}

Key32 hkdf_expand_32(
    const Hash256& pseudorandom_key,
    std::string_view info)
{
    Bytes input;
    input.reserve(info.size() + 1U);
    for (const char ch : info) {
        input.push_back(static_cast<Byte>(
            static_cast<unsigned char>(ch)
        ));
    }
    input.push_back(1U);

    const auto result =
        hmac_sha256(pseudorandom_key, input);

    crypto::secure_erase(input);
    return result;
}

Bytes network_salt(
    const consensus::ChainParams& params)
{
    constexpr std::string_view domain{
        "qmu_v2_shared_secret"
    };

    Bytes salt;
    salt.reserve(domain.size() + params.message_start.size());

    for (const char ch : domain) {
        salt.push_back(static_cast<Byte>(
            static_cast<unsigned char>(ch)
        ));
    }

    salt.insert(
        salt.end(),
        params.message_start.begin(),
        params.message_start.end()
    );
    return salt;
}

std::array<Byte, 8> packet_aad(
    const std::array<Byte, 4>& network_magic,
    std::uint32_t plaintext_size)
{
    std::array<Byte, 8> aad{};
    std::copy(
        network_magic.begin(),
        network_magic.end(),
        aad.begin()
    );

    aad[4] = static_cast<Byte>(plaintext_size);
    aad[5] = static_cast<Byte>(plaintext_size >> 8U);
    aad[6] = static_cast<Byte>(plaintext_size >> 16U);
    aad[7] = static_cast<Byte>(plaintext_size >> 24U);
    return aad;
}

void wipe_context(crypto_aead_ctx& context) noexcept
{
    crypto_wipe(&context, sizeof(context));
}

} // namespace

V2EphemeralKey::~V2EphemeralKey()
{
    crypto::secure_erase(secret);
}

V2EphemeralKey::V2EphemeralKey(
    V2EphemeralKey&& other) noexcept
    : secret(other.secret),
      public_key(other.public_key)
{
    crypto::secure_erase(other.secret);
    other.public_key.fill(0U);
}

V2EphemeralKey& V2EphemeralKey::operator=(
    V2EphemeralKey&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    crypto::secure_erase(secret);
    secret = other.secret;
    public_key = other.public_key;

    crypto::secure_erase(other.secret);
    other.public_key.fill(0U);
    return *this;
}

V2Transport::V2Transport(
    const std::array<Byte, 32>& send_key,
    const std::array<Byte, 32>& receive_key,
    const Hash256& session_id,
    const std::array<Byte, 4>& network_magic) noexcept
    : session_id_(session_id),
      network_magic_(network_magic)
{
    std::array<Byte, 12> nonce{};

    crypto_aead_init_ietf(
        &send_ctx_,
        send_key.data(),
        nonce.data()
    );
    crypto_aead_init_ietf(
        &receive_ctx_,
        receive_key.data(),
        nonce.data()
    );
}

V2Transport::~V2Transport()
{
    wipe_context(send_ctx_);
    wipe_context(receive_ctx_);
    crypto::secure_erase(session_id_);
}

V2Transport::V2Transport(
    V2Transport&& other) noexcept
    : send_ctx_(other.send_ctx_),
      receive_ctx_(other.receive_ctx_),
      session_id_(other.session_id_),
      network_magic_(other.network_magic_)
{
    wipe_context(other.send_ctx_);
    wipe_context(other.receive_ctx_);
    crypto::secure_erase(other.session_id_);
    other.network_magic_.fill(0U);
}

V2Transport& V2Transport::operator=(
    V2Transport&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    wipe_context(send_ctx_);
    wipe_context(receive_ctx_);
    crypto::secure_erase(session_id_);

    send_ctx_ = other.send_ctx_;
    receive_ctx_ = other.receive_ctx_;
    session_id_ = other.session_id_;
    network_magic_ = other.network_magic_;

    wipe_context(other.send_ctx_);
    wipe_context(other.receive_ctx_);
    crypto::secure_erase(other.session_id_);
    other.network_magic_.fill(0U);
    return *this;
}

const Hash256& V2Transport::session_id() const noexcept
{
    return session_id_;
}

Bytes V2Transport::encrypt(
    std::span<const Byte> plaintext,
    V2TransportError& error)
{
    error = V2TransportError::none;

    if (plaintext.size() > kV2MaxPlaintext ||
        plaintext.size() >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint32_t>::max())) {
        error = V2TransportError::payload_too_large;
        return {};
    }

    const auto size =
        static_cast<std::uint32_t>(plaintext.size());
    const auto aad = packet_aad(network_magic_, size);

    Bytes packet;
    packet.resize(
        kV2PacketLengthSize +
        plaintext.size() +
        kV2PacketTagSize
    );

    packet[0] = static_cast<Byte>(size);
    packet[1] = static_cast<Byte>(size >> 8U);
    packet[2] = static_cast<Byte>(size >> 16U);
    packet[3] = static_cast<Byte>(size >> 24U);

    Byte* cipher_text =
        packet.data() + kV2PacketLengthSize;
    Byte* mac =
        cipher_text + plaintext.size();

    crypto_aead_write(
        &send_ctx_,
        cipher_text,
        mac,
        aad.data(),
        aad.size(),
        plaintext.data(),
        plaintext.size()
    );

    return packet;
}

std::optional<Bytes> V2Transport::decrypt(
    std::span<const Byte> packet,
    V2TransportError& error)
{
    error = V2TransportError::none;

    if (packet.size() <
        kV2PacketLengthSize + kV2PacketTagSize) {
        error = V2TransportError::malformed_packet;
        return std::nullopt;
    }

    const std::uint32_t size =
        static_cast<std::uint32_t>(packet[0]) |
        (static_cast<std::uint32_t>(packet[1]) << 8U) |
        (static_cast<std::uint32_t>(packet[2]) << 16U) |
        (static_cast<std::uint32_t>(packet[3]) << 24U);

    if (size > kV2MaxPlaintext) {
        error = V2TransportError::payload_too_large;
        return std::nullopt;
    }

    const std::size_t expected =
        kV2PacketLengthSize +
        static_cast<std::size_t>(size) +
        kV2PacketTagSize;

    if (packet.size() != expected) {
        error = V2TransportError::malformed_packet;
        return std::nullopt;
    }

    const auto aad = packet_aad(network_magic_, size);
    const Byte* cipher_text =
        packet.data() + kV2PacketLengthSize;
    const Byte* mac =
        cipher_text + static_cast<std::size_t>(size);

    Bytes plaintext(static_cast<std::size_t>(size));
    const int result = crypto_aead_read(
        &receive_ctx_,
        plaintext.data(),
        mac,
        aad.data(),
        aad.size(),
        cipher_text,
        plaintext.size()
    );

    if (result != 0) {
        crypto::secure_erase(plaintext);
        error = V2TransportError::authentication_failed;
        return std::nullopt;
    }

    return plaintext;
}

std::optional<V2EphemeralKey>
create_v2_ephemeral() noexcept
{
    auto secret = crypto::generate_private_key();
    if (!secret) {
        return std::nullopt;
    }

    std::array<Byte, 32> aux_random{};
    if (!crypto::secure_random_bytes(aux_random)) {
        crypto::secure_erase(*secret);
        return std::nullopt;
    }

    const auto public_key =
        crypto::ellswift_public_key(
            *secret,
            aux_random
        );

    crypto::secure_erase(aux_random);

    if (!public_key) {
        crypto::secure_erase(*secret);
        return std::nullopt;
    }

    V2EphemeralKey result;
    result.secret = *secret;
    result.public_key = *public_key;
    crypto::secure_erase(*secret);
    return result;
}

std::optional<V2Transport>
derive_v2_transport(
    const consensus::ChainParams& params,
    const V2EphemeralKey& local,
    const crypto::EllSwiftPublicKey& initiator_public,
    const crypto::EllSwiftPublicKey& responder_public,
    bool initiating) noexcept
{
    auto shared =
        crypto::ellswift_xdh_bip324(
            local.secret,
            initiator_public,
            responder_public,
            initiating
        );

    if (!shared) {
        return std::nullopt;
    }

    auto salt = network_salt(params);
    auto pseudorandom_key =
        hkdf_extract(salt, *shared);

    auto initiator_key =
        hkdf_expand_32(
            pseudorandom_key,
            "qmu_v2_initiator"
        );
    auto responder_key =
        hkdf_expand_32(
            pseudorandom_key,
            "qmu_v2_responder"
        );
    auto session_id =
        hkdf_expand_32(
            pseudorandom_key,
            "qmu_v2_session_id"
        );

    auto send_key =
        initiating ? initiator_key : responder_key;
    auto receive_key =
        initiating ? responder_key : initiator_key;

    V2Transport transport{
        send_key,
        receive_key,
        session_id,
        params.message_start,
    };

    crypto::secure_erase(salt);
    crypto::secure_erase(*shared);
    crypto::secure_erase(pseudorandom_key);
    crypto::secure_erase(initiator_key);
    crypto::secure_erase(responder_key);
    crypto::secure_erase(send_key);
    crypto::secure_erase(receive_key);
    crypto::secure_erase(session_id);

    return transport;
}

} // namespace quintum::net
