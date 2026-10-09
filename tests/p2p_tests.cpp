#include "consensus/chainparams.hpp"
#include "net/peer.hpp"
#include "net/protocol.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <thread>
#include <utility>

namespace {

quintum::net::VersionMessage version(
    std::uint64_t nonce,
    std::uint32_t height)
{
    return quintum::net::VersionMessage{
        .protocol_version =
            quintum::net::kProtocolVersion,
        .services = 1U,
        .timestamp = 1'790'970'000ULL,
        .nonce = nonce,
        .start_height = height,
    };
}

quintum::net::VersionMessage encrypted_version(
    std::uint64_t nonce,
    std::uint32_t height)
{
    auto value = version(nonce, height);
    value.services |=
        quintum::net::kServiceEncryptedTransport;
    return value;
}

void test_wire_protocol()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();

    const auto payload =
        serialize_version(
            version(0x1122334455667788ULL, 42U)
        );

    const auto encoded =
        encode_message(
            params,
            "version",
            payload
        );

    assert(encoded.ok());
    assert(encoded.bytes.size() ==
           kMessageHeaderSize + payload.size());

    const auto decoded =
        decode_message(
            params,
            encoded.bytes
        );

    assert(decoded.ok());
    assert(decoded.message.command == "version");
    assert(decoded.consumed == encoded.bytes.size());

    const auto parsed =
        parse_version(decoded.message.payload);

    assert(parsed.has_value());
    assert(parsed->protocol_version ==
           kProtocolVersion);
    assert(parsed->nonce ==
           0x1122334455667788ULL);
    assert(parsed->start_height == 42U);
    assert(parsed->listen_port == 0U);

    auto v2 =
        version(
            0x8877665544332211ULL,
            43U
        );
    v2.protocol_version =
        kPeerAddressProtocolVersion;
    v2.listen_port = 39444U;

    const auto v2_payload =
        serialize_version(v2);

    assert(v2_payload.size() == 34U);

    const auto parsed_v2 =
        parse_version(v2_payload);

    assert(parsed_v2.has_value());
    assert(parsed_v2->protocol_version ==
           kPeerAddressProtocolVersion);
    assert(parsed_v2->listen_port == 39444U);
    assert(parsed_v2->start_height == 43U);

    auto malformed_v2 = v2_payload;
    malformed_v2.resize(32U);
    assert(!parse_version(malformed_v2));

    auto corrupted = encoded.bytes;
    corrupted.back() ^= 0x01U;

    const auto checksum_failure =
        decode_message(params, corrupted);

    assert(checksum_failure.error ==
           WireError::checksum_mismatch);

    const auto wrong_network =
        decode_message(
            consensus::testnet_params(),
            encoded.bytes
        );

    assert(wrong_network.error ==
           WireError::bad_magic);

    const auto invalid_command =
        encode_message(
            params,
            "bad-command",
            {}
        );

    assert(invalid_command.error ==
           WireError::invalid_command);

    auto oversized = encoded.bytes;
    const std::uint32_t oversized_size =
        kMaxMessagePayload + 1U;

    for (std::size_t i = 0U;
         i < sizeof(oversized_size);
         ++i) {
        oversized[16U + i] =
            static_cast<Byte>(
                oversized_size >> (8U * i)
            );
    }

    const auto oversized_result =
        decode_message(params, oversized);

    assert(oversized_result.error ==
           WireError::payload_too_large);
}

void test_two_peer_handshake_and_ping()
{
    using namespace quintum::net;

    const auto& params =
        quintum::consensus::regtest_params();

    PeerListener listener{params};
    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);
    assert(listener.active());
    assert(listener.local_port() != 0U);

    PeerError server_handshake{
        PeerError::accept_failed
    };
    PeerError server_service{
        PeerError::receive_failed
    };
    std::uint32_t server_saw_height{0U};
    bool server_saw_inbound{false};
    std::size_t server_after_prune{99U};

    std::thread server([&] {
        auto accepted =
            listener.accept_and_handshake(
                version(0xaaa1U, 7U),
                5'000U
            );

        server_handshake = accepted.error;

        if (!accepted.ok()) {
            return;
        }

        server_saw_height =
            accepted.session->remote_version()
                .start_height;
        server_saw_inbound =
            accepted.session->inbound();

        ConnectionManager manager;
        assert(manager.add(
            std::move(*accepted.session)));
        assert(manager.size() == 1U);

        server_service =
            manager.peer(0U)->service_once();

        manager.peer(0U)->close();
        manager.prune_closed();
        server_after_prune = manager.size();
    });

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            version(0xbbb2U, 5U),
            5'000U
        );

    assert(connected.ok());
    assert(!connected.session->inbound());
    assert(connected.session->remote_version()
               .start_height == 7U);

    ConnectionManager manager;
    assert(manager.add(
        std::move(*connected.session)));
    assert(manager.size() == 1U);

    assert(manager.peer(0U)->ping(
               0x123456789abcdef0ULL) ==
           PeerError::none);

    manager.peer(0U)->close();
    manager.prune_closed();
    assert(manager.size() == 0U);

    server.join();

    assert(server_handshake == PeerError::none);
    assert(server_service == PeerError::none);
    assert(server_saw_height == 5U);
    assert(server_saw_inbound);
    assert(server_after_prune == 0U);
}

void test_encrypted_peer_handshake_and_ping()
{
    using namespace quintum::net;

    const auto& params =
        quintum::consensus::regtest_params();

    PeerListener listener{params};
    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    PeerError server_error{
        PeerError::accept_failed
    };
    std::optional<quintum::Hash256>
        server_session_id;

    std::thread server([&] {
        auto accepted =
            listener.accept_and_handshake(
                encrypted_version(
                    0xc001U,
                    11U
                ),
                5'000U
            );

        server_error = accepted.error;
        if (!accepted.ok()) {
            return;
        }

        assert(accepted.session->encrypted());
        server_session_id =
            accepted.session->session_id();
        server_error =
            accepted.session->service_once();
        accepted.session->close();
    });

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            encrypted_version(
                0xc002U,
                12U
            ),
            5'000U
        );

    assert(connected.ok());
    assert(connected.session->encrypted());
    const auto client_session_id =
        connected.session->session_id();
    assert(client_session_id.has_value());

    assert(connected.session->ping(
               0xfeedfacecafebeefULL) ==
           PeerError::none);

    connected.session->close();
    server.join();

    assert(server_error == PeerError::none);
    assert(server_session_id.has_value());
    assert(*server_session_id ==
           *client_session_id);
}

void test_wrong_network_rejected()
{
    using namespace quintum::net;

    const auto& regtest =
        quintum::consensus::regtest_params();

    PeerListener listener{regtest};
    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    PeerHandshakeResult server_result;

    std::thread server([&] {
        server_result =
            listener.accept_and_handshake(
                version(0x1001U, 0U),
                5'000U
            );
    });

    const auto client =
        connect_and_handshake(
            quintum::consensus::testnet_params(),
            "127.0.0.1",
            listener.local_port(),
            version(0x2002U, 0U),
            5'000U
        );

    server.join();

    assert(!server_result.ok());
    assert(server_result.error ==
           PeerError::wire_error);
    assert(server_result.wire_error ==
           WireError::bad_magic);
    assert(!client.ok());
}

void test_self_connection_rejected()
{
    using namespace quintum::net;

    const auto& params =
        quintum::consensus::regtest_params();

    PeerListener listener{params};
    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    PeerHandshakeResult server_result;

    std::thread server([&] {
        server_result =
            listener.accept_and_handshake(
                version(0x5555U, 1U),
                5'000U
            );
    });

    const auto client =
        connect_and_handshake(
            params,
            "127.0.0.1",
            listener.local_port(),
            version(0x5555U, 1U),
            5'000U
        );

    server.join();

    assert(server_result.error ==
           PeerError::self_connection);
    assert(client.error ==
               PeerError::self_connection ||
           client.error ==
               PeerError::receive_failed);
}

void test_disconnect_and_reconnect()
{
    using namespace quintum::net;

    const auto& params =
        quintum::consensus::regtest_params();

    PeerListener listener{params};
    assert(listener.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    std::array<PeerError, 2> server_errors{
        PeerError::accept_failed,
        PeerError::accept_failed
    };

    std::thread server([&] {
        for (std::size_t i = 0U;
             i < server_errors.size();
             ++i) {
            auto accepted =
                listener.accept_and_handshake(
                    version(
                        0x9000U +
                            static_cast<std::uint64_t>(i),
                        10U +
                            static_cast<std::uint32_t>(i)
                    ),
                    5'000U
                );

            if (!accepted.ok()) {
                server_errors[i] = accepted.error;
                return;
            }

            server_errors[i] =
                accepted.session->service_once();

            accepted.session->close();
        }
    });

    for (std::size_t i = 0U;
         i < server_errors.size();
         ++i) {
        auto connected =
            connect_and_handshake(
                params,
                "127.0.0.1",
                listener.local_port(),
                version(
                    0xa000U +
                        static_cast<std::uint64_t>(i),
                    20U +
                        static_cast<std::uint32_t>(i)
                ),
                5'000U
            );

        assert(connected.ok());
        assert(connected.session->ping(
                   0xb000U +
                       static_cast<std::uint64_t>(i)) ==
               PeerError::none);
        connected.session->close();
    }

    server.join();

    assert(server_errors[0] == PeerError::none);
    assert(server_errors[1] == PeerError::none);
}


void test_adversarial_wire_frame_corpus()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const Bytes payload =
        serialize_version(
            version(0x5050U, 50U)
        );
    const auto encoded =
        encode_message(
            params,
            "version",
            payload
        );

    assert(encoded.ok());

    // Every strict prefix must be classified as incomplete,
    // never accepted as a message.
    for (std::size_t size = 0U;
         size < encoded.bytes.size();
         ++size) {
        const auto decoded =
            decode_message(
                params,
                std::span<const Byte>{
                    encoded.bytes.data(),
                    size
                }
            );
        assert(!decoded.ok());
    }

    // A complete first frame may be followed by another frame;
    // the decoder must consume exactly the first frame.
    Bytes concatenated = encoded.bytes;
    concatenated.insert(
        concatenated.end(),
        encoded.bytes.begin(),
        encoded.bytes.end()
    );
    const auto first =
        decode_message(
            params,
            concatenated
        );
    assert(first.ok());
    assert(first.consumed ==
           encoded.bytes.size());

    // Corrupt each magic byte independently.
    for (std::size_t i = 0U; i < 4U; ++i) {
        Bytes bad_magic = encoded.bytes;
        bad_magic[i] ^= 0x80U;
        const auto decoded =
            decode_message(
                params,
                bad_magic
            );
        assert(decoded.error ==
               WireError::bad_magic);
    }
}

} // namespace

#include "interrupted_receive_test.inc"

int main()
{
#ifndef _WIN32
    test_interrupted_peer_receive();
    test_interrupted_peer_receive(true);
#endif
    test_wire_protocol();
    test_two_peer_handshake_and_ping();
    test_encrypted_peer_handshake_and_ping();
    test_wrong_network_rejected();
    test_self_connection_rejected();
    test_disconnect_and_reconnect();
    test_adversarial_wire_frame_corpus();
    return 0;
}
