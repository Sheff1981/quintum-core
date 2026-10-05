#include "consensus/chainparams.hpp"
#include "crypto/secp256k1.hpp"
#include "net/v2_transport.hpp"

#include <cassert>
#include <cstddef>

using namespace quintum;

namespace {

crypto::PrivateKey private_key(Byte value)
{
    crypto::PrivateKey key{};
    key.back() = value;
    assert(crypto::is_valid_private_key(key));
    return key;
}

net::V2EphemeralKey make_key(
    Byte secret_value,
    Byte aux_value)
{
    net::V2EphemeralKey result;
    result.secret = private_key(secret_value);

    std::array<Byte, 32> aux{};
    aux.fill(aux_value);

    const auto public_key =
        crypto::ellswift_public_key(
            result.secret,
            aux
        );
    assert(public_key.has_value());
    result.public_key = *public_key;
    return result;
}

void roundtrip_and_tamper()
{
    const auto params =
        consensus::testnet_params();

    auto initiator = make_key(1U, 0x11U);
    auto responder = make_key(2U, 0x22U);

    auto a = net::derive_v2_transport(
        params,
        initiator,
        initiator.public_key,
        responder.public_key,
        true
    );
    auto b = net::derive_v2_transport(
        params,
        responder,
        initiator.public_key,
        responder.public_key,
        false
    );

    assert(a.has_value());
    assert(b.has_value());
    assert(a->session_id() == b->session_id());

    const Bytes message{
        0x51U, 0x4dU, 0x55U, 0x20U,
        0x76U, 0x32U,
    };

    net::V2TransportError error{};
    const auto packet = a->encrypt(message, error);
    assert(error == net::V2TransportError::none);
    assert(packet.size() ==
        net::kV2PacketLengthSize +
        message.size() +
        net::kV2PacketTagSize);

    const auto decoded = b->decrypt(packet, error);
    assert(decoded.has_value());
    assert(error == net::V2TransportError::none);
    assert(*decoded == message);

    const Bytes reply{0x01U, 0x02U, 0x03U};
    const auto reply_packet = b->encrypt(reply, error);
    assert(error == net::V2TransportError::none);

    const auto reply_decoded =
        a->decrypt(reply_packet, error);
    assert(reply_decoded.has_value());
    assert(*reply_decoded == reply);

    auto next_packet = a->encrypt(message, error);
    assert(error == net::V2TransportError::none);
    next_packet.back() ^= 0x01U;

    const auto tampered =
        b->decrypt(next_packet, error);
    assert(!tampered.has_value());
    assert(error ==
        net::V2TransportError::authentication_failed);
}

void network_domain_separation()
{
    auto initiator = make_key(3U, 0x33U);
    auto responder = make_key(4U, 0x44U);

    const auto testnet =
        consensus::testnet_params();
    const auto regtest =
        consensus::regtest_params();

    auto sender = net::derive_v2_transport(
        testnet,
        initiator,
        initiator.public_key,
        responder.public_key,
        true
    );
    auto wrong_network_receiver =
        net::derive_v2_transport(
            regtest,
            responder,
            initiator.public_key,
            responder.public_key,
            false
        );

    assert(sender.has_value());
    assert(wrong_network_receiver.has_value());
    assert(sender->session_id() !=
        wrong_network_receiver->session_id());

    const Bytes message{0xaaU, 0xbbU};
    net::V2TransportError error{};
    const auto packet =
        sender->encrypt(message, error);
    assert(error == net::V2TransportError::none);

    const auto decoded =
        wrong_network_receiver->decrypt(
            packet,
            error
        );
    assert(!decoded.has_value());
    assert(error ==
        net::V2TransportError::authentication_failed);
}

void malformed_packets_are_bounded()
{
    auto initiator = make_key(5U, 0x55U);
    auto responder = make_key(6U, 0x66U);

    auto receiver = net::derive_v2_transport(
        consensus::testnet_params(),
        responder,
        initiator.public_key,
        responder.public_key,
        false
    );
    assert(receiver.has_value());

    net::V2TransportError error{};
    const Bytes too_short{0U, 0U, 0U};
    assert(!receiver->decrypt(
        too_short,
        error
    ));
    assert(error ==
        net::V2TransportError::malformed_packet);

    Bytes oversized(
        net::kV2PacketLengthSize +
        net::kV2PacketTagSize,
        0U
    );
    const auto bad_size =
        static_cast<std::uint32_t>(
            net::kV2MaxPlaintext + 1U
        );
    oversized[0] = static_cast<Byte>(bad_size);
    oversized[1] = static_cast<Byte>(bad_size >> 8U);
    oversized[2] = static_cast<Byte>(bad_size >> 16U);
    oversized[3] = static_cast<Byte>(bad_size >> 24U);

    assert(!receiver->decrypt(
        oversized,
        error
    ));
    assert(error ==
        net::V2TransportError::payload_too_large);
}

} // namespace

int main()
{
    roundtrip_and_tamper();
    network_domain_separation();
    malformed_packets_are_bounded();
    return 0;
}
