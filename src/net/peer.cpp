#include "net/peer.hpp"
#ifdef __ANDROID__
#include <android/log.h>
#endif

#include "core/serialize.hpp"
#include "net/socks5.hpp"
#include "net/v2_transport.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <chrono>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace quintum::net {
namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidNativeSocket = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidNativeSocket = -1;
#endif

std::uintptr_t store_socket(NativeSocket socket) noexcept
{
    if (socket == kInvalidNativeSocket) {
        return std::numeric_limits<std::uintptr_t>::max();
    }
    return static_cast<std::uintptr_t>(socket);
}

NativeSocket native_socket(std::uintptr_t socket) noexcept
{
    if (socket ==
        std::numeric_limits<std::uintptr_t>::max()) {
        return kInvalidNativeSocket;
    }
    return static_cast<NativeSocket>(socket);
}

bool socket_runtime_ready() noexcept
{
#ifdef _WIN32
    struct Runtime {
        bool ok{false};

        Runtime() noexcept
        {
            WSADATA data{};
            ok = WSAStartup(
                MAKEWORD(2, 2),
                &data
            ) == 0;
        }

        ~Runtime()
        {
            if (ok) {
                WSACleanup();
            }
        }
    };

    static Runtime runtime;
    return runtime.ok;
#else
    return true;
#endif
}

void close_native(NativeSocket socket) noexcept
{
    if (socket == kInvalidNativeSocket) {
        return;
    }

#ifdef _WIN32
    closesocket(socket);
#else
    ::close(socket);
#endif
}

bool set_nonblocking(
    NativeSocket socket,
    bool enabled) noexcept
{
#ifdef _WIN32
    u_long mode = enabled ? 1UL : 0UL;
    return ioctlsocket(
        socket,
        FIONBIO,
        &mode
    ) == 0;
#else
    const int flags = fcntl(
        socket,
        F_GETFL,
        0
    );

    if (flags < 0) {
        return false;
    }

    const int updated = enabled
        ? (flags | O_NONBLOCK)
        : (flags & ~O_NONBLOCK);

    return fcntl(
        socket,
        F_SETFL,
        updated
    ) == 0;
#endif
}

int last_socket_error() noexcept
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool connect_in_progress(int error) noexcept
{
#ifdef _WIN32
    return error == WSAEWOULDBLOCK ||
           error == WSAEINPROGRESS ||
           error == WSAEINVAL;
#else
    return error == EINPROGRESS ||
           error == EWOULDBLOCK;
#endif
}

bool interrupted_socket_call() noexcept
{
#ifdef _WIN32
    return last_socket_error() == WSAEINTR;
#else
    return last_socket_error() == EINTR;
#endif
}

bool wait_socket(
    NativeSocket socket,
    bool write,
    std::uint32_t timeout_ms) noexcept
{
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    for (;;) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(socket, &set);
        const auto remaining = std::max<std::int64_t>(0,
            std::chrono::duration_cast<std::chrono::microseconds>(
                deadline - std::chrono::steady_clock::now()).count());
        timeval timeout{};
        timeout.tv_sec = static_cast<long>(remaining / 1'000'000);
        timeout.tv_usec = static_cast<long>(remaining % 1'000'000);
        const int result = select(
#ifdef _WIN32
            0,
#else
            socket + 1,
#endif
            write ? nullptr : &set,
            write ? &set : nullptr,
            nullptr, &timeout);
        if (result >= 0) return result > 0;
        if (!interrupted_socket_call() ||
            std::chrono::steady_clock::now() >= deadline) return false;
    }
}

bool set_io_timeout(
    NativeSocket socket,
    std::uint32_t timeout_ms) noexcept
{
#ifdef _WIN32
    const DWORD timeout =
        static_cast<DWORD>(timeout_ms);

    return setsockopt(
               socket,
               SOL_SOCKET,
               SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout),
               static_cast<int>(sizeof(timeout))
           ) == 0 &&
           setsockopt(
               socket,
               SOL_SOCKET,
               SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout),
               static_cast<int>(sizeof(timeout))
           ) == 0;
#else
    timeval timeout{};
    timeout.tv_sec =
        static_cast<time_t>(timeout_ms / 1'000U);
    timeout.tv_usec =
        static_cast<suseconds_t>(
            (timeout_ms % 1'000U) * 1'000U
        );

    return setsockopt(
               socket,
               SOL_SOCKET,
               SO_RCVTIMEO,
               &timeout,
               sizeof(timeout)
           ) == 0 &&
           setsockopt(
               socket,
               SOL_SOCKET,
               SO_SNDTIMEO,
               &timeout,
               sizeof(timeout)
           ) == 0;
#endif
}

bool send_all(
    NativeSocket socket,
    std::span<const Byte> bytes) noexcept
{
    std::size_t sent{0U};

    while (sent < bytes.size()) {
        const std::size_t remaining =
            bytes.size() - sent;
        const int chunk =
            static_cast<int>(std::min<std::size_t>(
                remaining,
                static_cast<std::size_t>(INT_MAX)
            ));

#ifdef _WIN32
        const int result = ::send(
            socket,
            reinterpret_cast<const char*>(
                bytes.data() + sent
            ),
            chunk,
            0
        );
#else
        const int result = static_cast<int>(::send(
            socket,
            bytes.data() + sent,
            static_cast<std::size_t>(chunk),
            MSG_NOSIGNAL
        ));
#endif

        if (result < 0 && interrupted_socket_call()) continue;
        if (result <= 0) {
#ifdef __ANDROID__
            const int socket_error = result < 0 ? last_socket_error() : 0;
            __android_log_print(ANDROID_LOG_WARN, "QUINTUM-SOCKET",
                "socket I/O failed result=%d errno=%d (%s)", result,
                socket_error, result == 0 ? "remote closed" : std::strerror(socket_error));
#endif
            return false;
        }

        sent += static_cast<std::size_t>(result);
    }

    return true;
}

bool receive_exact(
    NativeSocket socket,
    std::span<Byte> bytes) noexcept
{
    std::size_t received{0U};

    while (received < bytes.size()) {
        const std::size_t remaining =
            bytes.size() - received;
        const int chunk =
            static_cast<int>(std::min<std::size_t>(
                remaining,
                static_cast<std::size_t>(INT_MAX)
            ));

#ifdef _WIN32
        const int result = ::recv(
            socket,
            reinterpret_cast<char*>(
                bytes.data() + received
            ),
            chunk,
            0
        );
#else
        const int result = static_cast<int>(::recv(
            socket,
            bytes.data() + received,
            static_cast<std::size_t>(chunk),
            0
        ));
#endif

        if (result < 0 && interrupted_socket_call()) continue;
        if (result <= 0) {
#ifdef __ANDROID__
            const int socket_error = result < 0 ? last_socket_error() : 0;
            __android_log_print(ANDROID_LOG_WARN, "QUINTUM-SOCKET",
                "socket I/O failed result=%d errno=%d (%s)", result,
                socket_error, result == 0 ? "remote closed" : std::strerror(socket_error));
#endif
            return false;
        }

        received +=
            static_cast<std::size_t>(result);
    }

    return true;
}

PeerError send_plain_message(
    NativeSocket socket,
    const consensus::ChainParams& params,
    std::string_view command,
    std::span<const Byte> payload,
    WireError& wire_error)
{
    const auto encoded =
        encode_message(
            params,
            command,
            payload
        );

    if (!encoded.ok()) {
        wire_error = encoded.error;
        return PeerError::wire_error;
    }

    if (!send_all(socket, encoded.bytes)) {
        return PeerError::send_failed;
    }

    return PeerError::none;
}

PeerError receive_plain_message(
    NativeSocket socket,
    const consensus::ChainParams& params,
    WireMessage& message,
    WireError& wire_error)
{
    std::array<Byte, kMessageHeaderSize> header{};

    if (!receive_exact(socket, header)) {
        return PeerError::receive_failed;
    }

    std::size_t size_offset = 16U;
    const auto payload_size =
        read_little_endian<std::uint32_t>(
            header,
            size_offset
        );

    if (!payload_size) {
        wire_error = WireError::malformed_header;
        return PeerError::wire_error;
    }

    if (*payload_size > kMaxMessagePayload) {
        wire_error = WireError::payload_too_large;
        return PeerError::wire_error;
    }

    Bytes frame;
    frame.reserve(
        kMessageHeaderSize +
        static_cast<std::size_t>(*payload_size)
    );
    frame.insert(
        frame.end(),
        header.begin(),
        header.end()
    );

    const std::size_t old_size = frame.size();
    frame.resize(
        old_size +
        static_cast<std::size_t>(*payload_size)
    );

    if (*payload_size > 0U &&
        !receive_exact(
            socket,
            std::span<Byte>(
                frame.data() + old_size,
                static_cast<std::size_t>(*payload_size)
            ))) {
        return PeerError::receive_failed;
    }

    auto decoded =
        decode_message(params, frame);

    if (!decoded.ok()) {
        wire_error = decoded.error;
        return PeerError::wire_error;
    }

    message = std::move(decoded.message);
    return PeerError::none;
}


bool supports_v2_transport(
    const VersionMessage& local,
    const VersionMessage& remote) noexcept
{
    return (local.services &
                kServiceEncryptedTransport) != 0U &&
           (remote.services &
                kServiceEncryptedTransport) != 0U;
}

PeerError send_encrypted_message(
    NativeSocket socket,
    const consensus::ChainParams& params,
    V2Transport& transport,
    std::string_view command,
    std::span<const Byte> payload,
    WireError& wire_error)
{
    const auto encoded =
        encode_message(params, command, payload);

    if (!encoded.ok()) {
        wire_error = encoded.error;
        return PeerError::wire_error;
    }

    V2TransportError transport_error{};
    const auto packet =
        transport.encrypt(
            encoded.bytes,
            transport_error
        );

    if (transport_error !=
        V2TransportError::none) {
        return PeerError::encryption_failed;
    }

    if (!send_all(socket, packet)) {
        return PeerError::send_failed;
    }

    return PeerError::none;
}

PeerError receive_encrypted_message(
    NativeSocket socket,
    const consensus::ChainParams& params,
    V2Transport& transport,
    WireMessage& message,
    WireError& wire_error)
{
    std::array<Byte, kV2PacketLengthSize> prefix{};
    if (!receive_exact(socket, prefix)) {
        return PeerError::receive_failed;
    }

    const std::uint32_t plaintext_size =
        static_cast<std::uint32_t>(prefix[0]) |
        (static_cast<std::uint32_t>(prefix[1]) << 8U) |
        (static_cast<std::uint32_t>(prefix[2]) << 16U) |
        (static_cast<std::uint32_t>(prefix[3]) << 24U);

    if (plaintext_size > kV2MaxPlaintext) {
        wire_error = WireError::payload_too_large;
        return PeerError::wire_error;
    }

    Bytes packet;
    packet.resize(
        kV2PacketLengthSize +
        static_cast<std::size_t>(plaintext_size) +
        kV2PacketTagSize
    );
    std::copy(
        prefix.begin(),
        prefix.end(),
        packet.begin()
    );

    if (!receive_exact(
            socket,
            std::span<Byte>(
                packet.data() + kV2PacketLengthSize,
                packet.size() - kV2PacketLengthSize
            ))) {
        return PeerError::receive_failed;
    }

    V2TransportError transport_error{};
    const auto plaintext =
        transport.decrypt(
            packet,
            transport_error
        );

    if (!plaintext) {
        if (transport_error ==
            V2TransportError::payload_too_large) {
            wire_error =
                WireError::payload_too_large;
            return PeerError::wire_error;
        }
        return PeerError::encryption_failed;
    }

    auto decoded =
        decode_message(params, *plaintext);

    if (!decoded.ok() ||
        decoded.consumed != plaintext->size()) {
        wire_error = decoded.ok()
            ? WireError::malformed_payload
            : decoded.error;
        return PeerError::wire_error;
    }

    message = std::move(decoded.message);
    return PeerError::none;
}

std::unique_ptr<V2Transport> outbound_v2_upgrade(
    NativeSocket socket,
    const consensus::ChainParams& params,
    PeerError& error,
    WireError& wire_error)
{
    error = PeerError::none;

    auto local_key = create_v2_ephemeral();
    if (!local_key) {
        error = PeerError::encryption_failed;
        return nullptr;
    }

    error = send_plain_message(
        socket,
        params,
        "encinit",
        local_key->public_key,
        wire_error
    );
    if (error != PeerError::none) {
        return nullptr;
    }

    WireMessage ack;
    error = receive_plain_message(
        socket,
        params,
        ack,
        wire_error
    );
    if (error != PeerError::none) {
        return nullptr;
    }

    if (ack.command != "encack" ||
        ack.payload.size() !=
            crypto::EllSwiftPublicKey{}.size()) {
        error = PeerError::unexpected_message;
        return nullptr;
    }

    crypto::EllSwiftPublicKey responder_public{};
    std::copy(
        ack.payload.begin(),
        ack.payload.end(),
        responder_public.begin()
    );

    auto derived = derive_v2_transport(
        params,
        *local_key,
        local_key->public_key,
        responder_public,
        true
    );
    if (!derived) {
        error = PeerError::encryption_failed;
        return nullptr;
    }

    auto transport =
        std::make_unique<V2Transport>(
            std::move(*derived)
        );

    error = send_encrypted_message(
        socket,
        params,
        *transport,
        "encconf",
        {},
        wire_error
    );
    if (error != PeerError::none) {
        return nullptr;
    }

    WireMessage confirmation;
    error = receive_encrypted_message(
        socket,
        params,
        *transport,
        confirmation,
        wire_error
    );
    if (error != PeerError::none) {
        return nullptr;
    }

    if (confirmation.command != "encconf" ||
        !confirmation.payload.empty()) {
        error = PeerError::unexpected_message;
        return nullptr;
    }

    return transport;
}

std::unique_ptr<V2Transport> inbound_v2_upgrade(
    NativeSocket socket,
    const consensus::ChainParams& params,
    PeerError& error,
    WireError& wire_error)
{
    error = PeerError::none;

    WireMessage init;
    error = receive_plain_message(
        socket,
        params,
        init,
        wire_error
    );
    if (error != PeerError::none) {
        return nullptr;
    }

    if (init.command != "encinit" ||
        init.payload.size() !=
            crypto::EllSwiftPublicKey{}.size()) {
        error = PeerError::unexpected_message;
        return nullptr;
    }

    crypto::EllSwiftPublicKey initiator_public{};
    std::copy(
        init.payload.begin(),
        init.payload.end(),
        initiator_public.begin()
    );

    auto local_key = create_v2_ephemeral();
    if (!local_key) {
        error = PeerError::encryption_failed;
        return nullptr;
    }

    error = send_plain_message(
        socket,
        params,
        "encack",
        local_key->public_key,
        wire_error
    );
    if (error != PeerError::none) {
        return nullptr;
    }

    auto derived = derive_v2_transport(
        params,
        *local_key,
        initiator_public,
        local_key->public_key,
        false
    );
    if (!derived) {
        error = PeerError::encryption_failed;
        return nullptr;
    }

    auto transport =
        std::make_unique<V2Transport>(
            std::move(*derived)
        );

    WireMessage confirmation;
    error = receive_encrypted_message(
        socket,
        params,
        *transport,
        confirmation,
        wire_error
    );
    if (error != PeerError::none) {
        return nullptr;
    }

    if (confirmation.command != "encconf" ||
        !confirmation.payload.empty()) {
        error = PeerError::unexpected_message;
        return nullptr;
    }

    error = send_encrypted_message(
        socket,
        params,
        *transport,
        "encconf",
        {},
        wire_error
    );
    if (error != PeerError::none) {
        return nullptr;
    }

    return transport;
}

NativeSocket connect_tcp_socket(
    std::string_view host,
    std::uint16_t port,
    std::uint32_t timeout_ms,
    PeerError& error)
{
    error = PeerError::none;

    if (!socket_runtime_ready()) {
        error = PeerError::socket_runtime_failed;
        return kInvalidNativeSocket;
    }

    addrinfo hints{};
    // Outbound peers may be reachable over IPv4 or IPv6.
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addresses{nullptr};

    const std::string host_text{host};
    const std::string port_text =
        std::to_string(port);

    if (host.empty() || port == 0U ||
        getaddrinfo(
            host_text.c_str(),
            port_text.c_str(),
            &hints,
            &addresses) != 0 ||
        addresses == nullptr) {
        error = PeerError::resolve_failed;
        return kInvalidNativeSocket;
    }

    NativeSocket connected =
        kInvalidNativeSocket;

    for (addrinfo* current = addresses;
         current != nullptr;
         current = current->ai_next) {
        NativeSocket candidate = ::socket(
            current->ai_family,
            current->ai_socktype,
            current->ai_protocol
        );

        if (candidate == kInvalidNativeSocket) {
            continue;
        }

        if (!set_nonblocking(candidate, true)) {
            close_native(candidate);
            continue;
        }

    #ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "QUINTUM-SOCKET",
            "TCP connect endpoint=%s:%u family=%d timeout_ms=%u",
            host_text.c_str(), static_cast<unsigned>(port), current->ai_family,
            static_cast<unsigned>(timeout_ms));
#endif
        const int result = ::connect(
            candidate,
            current->ai_addr,
#ifdef _WIN32
            static_cast<int>(
                current->ai_addrlen)
#else
            current->ai_addrlen
#endif
        );

        bool success = result == 0;

        if (!success &&
            connect_in_progress(
                last_socket_error()) &&
            wait_socket(
                candidate,
                true,
                timeout_ms)) {
            int socket_error{0};
#ifdef _WIN32
            int error_size =
                static_cast<int>(
                    sizeof(socket_error));
            success = getsockopt(
                candidate,
                SOL_SOCKET,
                SO_ERROR,
                reinterpret_cast<char*>(
                    &socket_error),
                &error_size
            ) == 0 &&
            socket_error == 0;
#else
            socklen_t error_size =
                static_cast<socklen_t>(
                    sizeof(socket_error));
            success = getsockopt(
                candidate,
                SOL_SOCKET,
                SO_ERROR,
                &socket_error,
                &error_size
            ) == 0 &&
            socket_error == 0;
#endif
        }

        if (!success ||
            !set_nonblocking(
                candidate,
                false) ||
            !set_io_timeout(
                candidate,
                timeout_ms)) {
            close_native(candidate);
            continue;
        }

        connected = candidate;
        break;
    }

    freeaddrinfo(addresses);

    if (connected == kInvalidNativeSocket) {
        error = PeerError::connect_failed;
    }

    return connected;
}

PeerError negotiate_socks5(
    NativeSocket socket,
    std::string_view target_host,
    std::uint16_t target_port)
{
    const auto greeting =
        socks5_no_auth_greeting();

    if (!send_all(socket, greeting)) {
        return PeerError::send_failed;
    }

    std::array<Byte, 2> selection{};
    if (!receive_exact(socket, selection)) {
        return PeerError::receive_failed;
    }

    if (!socks5_no_auth_selected(selection)) {
        return PeerError::proxy_negotiation_failed;
    }

    const auto request =
        socks5_connect_request(
            target_host,
            target_port
        );

    if (!request) {
        return PeerError::proxy_negotiation_failed;
    }

    if (!send_all(socket, *request)) {
        return PeerError::send_failed;
    }

    std::array<Byte, 4> prefix{};
    if (!receive_exact(socket, prefix)) {
        return PeerError::receive_failed;
    }

    const auto reply =
        socks5_validate_reply_prefix(prefix);

    if (reply == Socks5Error::rejected) {
        return PeerError::proxy_rejected;
    }

    if (reply != Socks5Error::none) {
        return PeerError::proxy_negotiation_failed;
    }

    std::size_t remaining{0U};

    if (prefix[3] == 0x01U) {
        remaining = 4U + 2U;
    } else if (prefix[3] == 0x04U) {
        remaining = 16U + 2U;
    } else {
        std::array<Byte, 1> length{};
        if (!receive_exact(socket, length) ||
            length[0] == 0U) {
            return PeerError::proxy_negotiation_failed;
        }
        remaining =
            static_cast<std::size_t>(length[0]) +
            2U;
    }

    Bytes discard(remaining);
    if (!receive_exact(socket, discard)) {
        return PeerError::receive_failed;
    }

    return PeerError::none;
}

PeerHandshakeResult outbound_handshake(
    const consensus::ChainParams& params,
    NativeSocket socket,
    const VersionMessage& local)
{
    PeerHandshakeResult out;
    WireError wire_error{WireError::none};

    const auto version_payload =
        serialize_version(local);

    auto error = send_plain_message(
        socket,
        params,
        "version",
        version_payload,
        wire_error
    );

    if (error != PeerError::none) {
        out.error = error;
        out.wire_error = wire_error;
        return out;
    }

#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "QUINTUM-HANDSHAKE", "version sent; waiting for remote version");
#endif
    WireMessage remote_message;
    error = receive_plain_message(
        socket,
        params,
        remote_message,
        wire_error
    );

    if (error != PeerError::none) {
        out.error = error;
        out.wire_error = wire_error;
        return out;
    }

#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "QUINTUM-HANDSHAKE", "remote command=%s", remote_message.command.c_str());
#endif
    if (remote_message.command != "version") {
        out.error = PeerError::unexpected_message;
        return out;
    }

    const auto remote =
        parse_version(remote_message.payload);

    if (!remote) {
        out.error = PeerError::malformed_version;
        return out;
    }

#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "QUINTUM-HANDSHAKE", "remote protocol=%u height=%u", remote->protocol_version, remote->start_height);
#endif
    if (remote->protocol_version !=
        local.protocol_version) {
        out.error = PeerError::unsupported_protocol;
        return out;
    }

    if (remote->nonce == local.nonce) {
        out.error = PeerError::self_connection;
        return out;
    }

    error = send_plain_message(
        socket,
        params,
        "verack",
        {},
        wire_error
    );

    if (error != PeerError::none) {
        out.error = error;
        out.wire_error = wire_error;
        return out;
    }

#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "QUINTUM-HANDSHAKE", "verack sent; waiting for remote verack");
#endif
    WireMessage verack;
    error = receive_plain_message(
        socket,
        params,
        verack,
        wire_error
    );

    if (error != PeerError::none) {
        out.error = error;
        out.wire_error = wire_error;
        return out;
    }

    if (verack.command != "verack" ||
        !verack.payload.empty()) {
        out.error = PeerError::unexpected_message;
        return out;
    }

    std::unique_ptr<V2Transport> transport;
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "QUINTUM-HANDSHAKE",
        "version/verack completed; encrypted_upgrade=%d",
        supports_v2_transport(local, *remote) ? 1 : 0);
#endif
    if (supports_v2_transport(local, *remote)) {
        transport = outbound_v2_upgrade(
            socket,
            params,
            error,
            wire_error
        );
        if (!transport) {
            out.error = error;
            out.wire_error = wire_error;
            return out;
        }
    }

#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "QUINTUM-HANDSHAKE",
        "handshake completed protocol=%u remote_height=%u encrypted=%d",
        remote->protocol_version, remote->start_height, transport ? 1 : 0);
#endif
    out.session.emplace(
        params,
        store_socket(socket),
        false,
        *remote,
        std::move(transport)
    );
    return out;
}

PeerHandshakeResult inbound_handshake(
    const consensus::ChainParams& params,
    NativeSocket socket,
    const VersionMessage& local)
{
    PeerHandshakeResult out;
    WireError wire_error{WireError::none};

    WireMessage remote_message;
    auto error = receive_plain_message(
        socket,
        params,
        remote_message,
        wire_error
    );

    if (error != PeerError::none) {
        out.error = error;
        out.wire_error = wire_error;
        return out;
    }

    if (remote_message.command != "version") {
        out.error = PeerError::unexpected_message;
        return out;
    }

    const auto remote =
        parse_version(remote_message.payload);

    if (!remote) {
        out.error = PeerError::malformed_version;
        return out;
    }

    if (remote->protocol_version !=
        local.protocol_version) {
        out.error = PeerError::unsupported_protocol;
        return out;
    }

    if (remote->nonce == local.nonce) {
        out.error = PeerError::self_connection;
        return out;
    }

    const auto local_payload =
        serialize_version(local);

    error = send_plain_message(
        socket,
        params,
        "version",
        local_payload,
        wire_error
    );

    if (error != PeerError::none) {
        out.error = error;
        out.wire_error = wire_error;
        return out;
    }

    WireMessage verack;
    error = receive_plain_message(
        socket,
        params,
        verack,
        wire_error
    );

    if (error != PeerError::none) {
        out.error = error;
        out.wire_error = wire_error;
        return out;
    }

    if (verack.command != "verack" ||
        !verack.payload.empty()) {
        out.error = PeerError::unexpected_message;
        return out;
    }

    error = send_plain_message(
        socket,
        params,
        "verack",
        {},
        wire_error
    );

    if (error != PeerError::none) {
        out.error = error;
        out.wire_error = wire_error;
        return out;
    }

    std::unique_ptr<V2Transport> transport;
    if (supports_v2_transport(local, *remote)) {
        transport = inbound_v2_upgrade(
            socket,
            params,
            error,
            wire_error
        );
        if (!transport) {
            out.error = error;
            out.wire_error = wire_error;
            return out;
        }
    }

    out.session.emplace(
        params,
        store_socket(socket),
        true,
        *remote,
        std::move(transport)
    );
    return out;
}

} // namespace

PeerSession::PeerSession(
    const consensus::ChainParams& params,
    std::uintptr_t socket,
    bool inbound,
    VersionMessage remote,
    std::unique_ptr<V2Transport> transport) noexcept
    : params_(params),
      socket_(socket),
      inbound_(inbound),
      remote_(remote),
      transport_(std::move(transport))
{
}

PeerSession::~PeerSession()
{
    close();
}

PeerSession::PeerSession(
    PeerSession&& other) noexcept
    : params_(other.params_),
      socket_(other.socket_),
      inbound_(other.inbound_),
      remote_(other.remote_),
      transport_(std::move(other.transport_))
{
    other.socket_ = kInvalidSocket;
}

PeerSession& PeerSession::operator=(
    PeerSession&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    close();

    params_ = other.params_;
    socket_ = other.socket_;
    inbound_ = other.inbound_;
    remote_ = other.remote_;
    transport_ = std::move(other.transport_);

    other.socket_ = kInvalidSocket;
    return *this;
}

bool PeerSession::valid() const noexcept
{
    return socket_ != kInvalidSocket;
}

bool PeerSession::inbound() const noexcept
{
    return inbound_;
}

const VersionMessage&
PeerSession::remote_version() const noexcept
{
    return remote_;
}

bool PeerSession::encrypted() const noexcept
{
    return transport_ != nullptr;
}

std::optional<Hash256>
PeerSession::session_id() const noexcept
{
    if (!transport_) {
        return std::nullopt;
    }
    return transport_->session_id();
}

bool PeerSession::wait_readable(
    std::uint32_t timeout_ms) const noexcept
{
    if (!valid()) {
        return false;
    }

    return wait_socket(
        native_socket(socket_),
        false,
        timeout_ms
    );
}

PeerError PeerSession::send_command(
    std::string_view command,
    std::span<const Byte> payload)
{
    if (!valid()) {
        return PeerError::send_failed;
    }

    WireError wire_error{WireError::none};
    if (transport_) {
        return send_encrypted_message(
            native_socket(socket_),
            params_,
            *transport_,
            command,
            payload,
            wire_error
        );
    }

    return send_plain_message(
        native_socket(socket_),
        params_,
        command,
        payload,
        wire_error
    );
}

PeerError PeerSession::receive_command(
    WireMessage& message)
{
    if (!valid()) {
        return PeerError::receive_failed;
    }

    WireError wire_error{WireError::none};
    if (transport_) {
        return receive_encrypted_message(
            native_socket(socket_),
            params_,
            *transport_,
            message,
            wire_error
        );
    }

    return receive_plain_message(
        native_socket(socket_),
        params_,
        message,
        wire_error
    );
}

PeerError PeerSession::ping(
    std::uint64_t nonce)
{
    if (!valid()) {
        return PeerError::receive_failed;
    }

    const auto payload = serialize_nonce(nonce);

    auto error = send_command("ping", payload);

    if (error != PeerError::none) {
        return error;
    }

    WireMessage message;
    error = receive_command(message);

    if (error != PeerError::none) {
        return error;
    }

    if (message.command != "pong") {
        return PeerError::unexpected_message;
    }

    const auto returned =
        parse_nonce(message.payload);

    if (!returned || *returned != nonce) {
        return PeerError::malformed_ping;
    }

    return PeerError::none;
}

PeerError PeerSession::service_once()
{
    if (!valid()) {
        return PeerError::receive_failed;
    }

    WireMessage message;

    const auto error = receive_command(message);

    if (error != PeerError::none) {
        return error;
    }

    if (message.command != "ping") {
        return PeerError::unexpected_message;
    }

    const auto nonce =
        parse_nonce(message.payload);

    if (!nonce) {
        return PeerError::malformed_ping;
    }

    const auto payload =
        serialize_nonce(*nonce);

    return send_command("pong", payload);
}

PeerError PeerSession::request_addresses(
    bool allow_local,
    std::vector<PeerAddress>& addresses)
{
    addresses.clear();

    if (!valid()) {
        return PeerError::receive_failed;
    }

    const bool use_v2 =
        (remote_.services &
            kServiceAddrV2) != 0U;

    auto error = send_command(
        use_v2 ? "getaddrv2" : "getaddr",
        {}
    );

    if (error != PeerError::none) {
        return error;
    }

    for (std::size_t handled = 0U;
         handled < 8U;
         ++handled) {
        WireMessage message;
        error = receive_command(message);

        if (error != PeerError::none) {
            return error;
        }

        if (message.command == "ping") {
            const auto nonce =
                parse_nonce(message.payload);

            if (!nonce) {
                return PeerError::malformed_ping;
            }

            const auto pong =
                serialize_nonce(*nonce);

            error = send_command(
                "pong",
                pong
            );

            if (error != PeerError::none) {
                return error;
            }

            continue;
        }

        const bool got_v2 =
            message.command == "addrv2";
        const bool got_v1 =
            message.command == "addr";

        if ((!use_v2 && !got_v1) ||
            (use_v2 && !got_v2)) {
            return PeerError::unexpected_message;
        }

        const auto parsed = got_v2
            ? parse_addresses_v2(
                  message.payload,
                  allow_local)
            : parse_addresses(
                  message.payload,
                  allow_local);

        if (!parsed) {
            return PeerError::unexpected_message;
        }

        addresses = *parsed;
        return PeerError::none;
    }

    return PeerError::unexpected_message;
}

PeerError PeerSession::service_discovery_once(
    std::span<const PeerAddress> advertised,
    bool allow_local,
    std::vector<PeerAddress>* learned)
{
    if (!valid()) {
        return PeerError::receive_failed;
    }

    WireMessage message;

    auto error = receive_command(message);

    if (error != PeerError::none) {
        return error;
    }

    if (message.command == "ping") {
        const auto nonce =
            parse_nonce(message.payload);

        if (!nonce) {
            return PeerError::malformed_ping;
        }

        const auto pong =
            serialize_nonce(*nonce);

        return send_command(
            "pong",
            pong
        );
    }

    if (message.command == "getaddr" ||
        message.command == "getaddrv2") {
        if (!message.payload.empty()) {
            return PeerError::unexpected_message;
        }

        if (message.command == "getaddrv2" &&
            (remote_.services &
                kServiceAddrV2) == 0U) {
            return PeerError::unexpected_message;
        }

        const bool use_v2 =
            message.command == "getaddrv2";

        const auto payload = use_v2
            ? serialize_addresses_v2(
                  advertised)
            : serialize_addresses(
                  advertised);

        return send_command(
            use_v2 ? "addrv2" : "addr",
            payload
        );
    }

    if (message.command == "addr" ||
        message.command == "addrv2") {
        if (message.command == "addrv2" &&
            (remote_.services &
                kServiceAddrV2) == 0U) {
            return PeerError::unexpected_message;
        }

        const auto parsed =
            message.command == "addrv2"
                ? parse_addresses_v2(
                      message.payload,
                      allow_local)
                : parse_addresses(
                      message.payload,
                      allow_local);

        if (!parsed) {
            return PeerError::unexpected_message;
        }

        if (learned != nullptr) {
            learned->insert(
                learned->end(),
                parsed->begin(),
                parsed->end()
            );
        }

        return PeerError::none;
    }

    return PeerError::unexpected_message;
}

void PeerSession::close() noexcept
{
    transport_.reset();

    if (!valid()) {
        return;
    }

    close_native(native_socket(socket_));
    socket_ = kInvalidSocket;
}

PeerListener::PeerListener(
    const consensus::ChainParams& params) noexcept
    : params_(params)
{
}

PeerListener::~PeerListener()
{
    close();
}

PeerListener::PeerListener(
    PeerListener&& other) noexcept
    : params_(other.params_),
      socket_(other.socket_),
      local_port_(other.local_port_)
{
    other.socket_ = kInvalidSocket;
    other.local_port_ = 0U;
}

PeerListener& PeerListener::operator=(
    PeerListener&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    close();

    params_ = other.params_;
    socket_ = other.socket_;
    local_port_ = other.local_port_;

    other.socket_ = kInvalidSocket;
    other.local_port_ = 0U;
    return *this;
}

PeerError PeerListener::listen(
    std::string_view bind_address,
    std::uint16_t port)
{
    close();

    if (!socket_runtime_ready()) {
        return PeerError::socket_runtime_failed;
    }

    const NativeSocket socket =
        ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (socket == kInvalidNativeSocket) {
        return PeerError::socket_create_failed;
    }

#ifdef _WIN32
    // Windows SO_REUSEADDR permits multiple listeners to bind the same
    // address/port, which can make inbound ownership ambiguous. A wallet
    // node must own its listening endpoint exclusively.
    int exclusive{1};
    (void)setsockopt(
        socket,
        SOL_SOCKET,
        SO_EXCLUSIVEADDRUSE,
        reinterpret_cast<const char*>(&exclusive),
        static_cast<int>(sizeof(exclusive))
    );
#else
    int reuse{1};
    (void)setsockopt(
        socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
    );
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    const std::string bind_text{bind_address};

    if (inet_pton(
            AF_INET,
            bind_text.c_str(),
            &address.sin_addr) != 1) {
        close_native(socket);
        return PeerError::bind_failed;
    }

    if (::bind(
            socket,
            reinterpret_cast<const sockaddr*>(
                &address
            ),
            static_cast<int>(sizeof(address))) != 0) {
        close_native(socket);
        return PeerError::bind_failed;
    }

    if (::listen(socket, 16) != 0) {
        close_native(socket);
        return PeerError::listen_failed;
    }

    sockaddr_in bound{};
#ifdef _WIN32
    int bound_size =
        static_cast<int>(sizeof(bound));
#else
    socklen_t bound_size =
        static_cast<socklen_t>(sizeof(bound));
#endif

    if (getsockname(
            socket,
            reinterpret_cast<sockaddr*>(&bound),
            &bound_size) != 0) {
        close_native(socket);
        return PeerError::bind_failed;
    }

    socket_ = store_socket(socket);
    local_port_ = ntohs(bound.sin_port);
    return PeerError::none;
}

bool PeerListener::active() const noexcept
{
    return socket_ != kInvalidSocket;
}

std::uint16_t
PeerListener::local_port() const noexcept
{
    return local_port_;
}

PeerHandshakeResult
PeerListener::accept_and_handshake(
    const VersionMessage& local,
    std::uint32_t timeout_ms)
{
    return accept_and_handshake(
        local,
        timeout_ms,
        timeout_ms
    );
}

PeerHandshakeResult
PeerListener::accept_and_handshake(
    const VersionMessage& local,
    std::uint32_t accept_timeout_ms,
    std::uint32_t io_timeout_ms)
{
    PeerHandshakeResult out;

    if (!active()) {
        out.error = PeerError::accept_failed;
        return out;
    }

    const NativeSocket listener =
        native_socket(socket_);

    if (!wait_socket(
            listener,
            false,
            accept_timeout_ms)) {
        out.error = PeerError::timeout;
        return out;
    }

    sockaddr_in remote_address{};
#ifdef _WIN32
    int remote_size =
        static_cast<int>(sizeof(remote_address));
#else
    socklen_t remote_size =
        static_cast<socklen_t>(
            sizeof(remote_address)
        );
#endif

    const NativeSocket accepted = ::accept(
        listener,
        reinterpret_cast<sockaddr*>(
            &remote_address
        ),
        &remote_size
    );

    if (accepted == kInvalidNativeSocket) {
        out.error = PeerError::accept_failed;
        return out;
    }

    if (!set_io_timeout(
            accepted,
            io_timeout_ms)) {
        close_native(accepted);
        out.error = PeerError::accept_failed;
        return out;
    }

    out = inbound_handshake(
        params_,
        accepted,
        local
    );

    if (!out.ok()) {
        close_native(accepted);
    } else {
        out.observed_ipv4 =
            ntohl(
                remote_address.sin_addr.s_addr
            );
    }

    return out;
}

void PeerListener::close() noexcept
{
    if (!active()) {
        return;
    }

    close_native(native_socket(socket_));
    socket_ = kInvalidSocket;
    local_port_ = 0U;
}

PeerHandshakeResult connect_and_handshake(
    const consensus::ChainParams& params,
    std::string_view host,
    std::uint16_t port,
    const VersionMessage& local,
    std::uint32_t timeout_ms)
{
    PeerHandshakeResult out;
    PeerError dial_error{PeerError::none};

    NativeSocket connected =
        connect_tcp_socket(
            host,
            port,
            timeout_ms,
            dial_error
        );

    if (connected == kInvalidNativeSocket) {
        out.error = dial_error;
        return out;
    }

    out = outbound_handshake(
        params,
        connected,
        local
    );

    if (!out.ok()) {
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_WARN, "QUINTUM-HANDSHAKE",
            "handshake failed peer_error=%d wire_error=%d",
            static_cast<int>(out.error), static_cast<int>(out.wire_error));
#endif
        close_native(connected);
    }

    return out;
}

PeerHandshakeResult connect_and_handshake(
    const consensus::ChainParams& params,
    std::string_view host,
    std::uint16_t port,
    const VersionMessage& local,
    std::uint32_t timeout_ms,
    const Socks5Proxy& proxy)
{
    PeerHandshakeResult out;

    if (!proxy.valid()) {
        out.error =
            PeerError::proxy_negotiation_failed;
        return out;
    }

    PeerError dial_error{PeerError::none};
    NativeSocket connected =
        connect_tcp_socket(
            proxy.host,
            proxy.port,
            timeout_ms,
            dial_error
        );

    if (connected == kInvalidNativeSocket) {
        out.error = dial_error;
        return out;
    }

    const auto proxy_error =
        negotiate_socks5(
            connected,
            host,
            port
        );

    if (proxy_error != PeerError::none) {
        close_native(connected);
        out.error = proxy_error;
        return out;
    }

    out = outbound_handshake(
        params,
        connected,
        local
    );

    if (!out.ok()) {
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_WARN, "QUINTUM-HANDSHAKE",
            "handshake failed peer_error=%d wire_error=%d",
            static_cast<int>(out.error), static_cast<int>(out.wire_error));
#endif
        close_native(connected);
    }

    return out;
}

bool ConnectionManager::add(
    PeerSession session)
{
    if (!session.valid()) {
        return false;
    }

    peers_.push_back(std::move(session));
    return true;
}

std::size_t
ConnectionManager::size() const noexcept
{
    return peers_.size();
}

PeerSession* ConnectionManager::peer(
    std::size_t index) noexcept
{
    if (index >= peers_.size()) {
        return nullptr;
    }
    return &peers_[index];
}

const PeerSession* ConnectionManager::peer(
    std::size_t index) const noexcept
{
    if (index >= peers_.size()) {
        return nullptr;
    }
    return &peers_[index];
}

void ConnectionManager::prune_closed()
{
    std::erase_if(
        peers_,
        [](const PeerSession& peer) {
            return !peer.valid();
        }
    );
}

void ConnectionManager::disconnect_all() noexcept
{
    for (auto& peer : peers_) {
        peer.close();
    }
    peers_.clear();
}

} // namespace quintum::net

