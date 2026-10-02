#include "net/peer.hpp"

#include "core/serialize.hpp"

#include <algorithm>
#include <array>
#include <climits>
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

bool wait_socket(
    NativeSocket socket,
    bool write,
    std::uint32_t timeout_ms) noexcept
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(socket, &set);

    timeval timeout{};
    timeout.tv_sec =
        static_cast<long>(timeout_ms / 1'000U);
    timeout.tv_usec =
        static_cast<long>(
            (timeout_ms % 1'000U) * 1'000U
        );

#ifdef _WIN32
    const int result = select(
        0,
        write ? nullptr : &set,
        write ? &set : nullptr,
        nullptr,
        &timeout
    );
#else
    const int result = select(
        socket + 1,
        write ? nullptr : &set,
        write ? &set : nullptr,
        nullptr,
        &timeout
    );
#endif

    return result > 0;
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
            0
        ));
#endif

        if (result <= 0) {
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

        if (result <= 0) {
            return false;
        }

        received +=
            static_cast<std::size_t>(result);
    }

    return true;
}

PeerError send_message(
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

PeerError receive_message(
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

PeerHandshakeResult outbound_handshake(
    const consensus::ChainParams& params,
    NativeSocket socket,
    const VersionMessage& local)
{
    PeerHandshakeResult out;
    WireError wire_error{WireError::none};

    const auto version_payload =
        serialize_version(local);

    auto error = send_message(
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

    WireMessage remote_message;
    error = receive_message(
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
        kProtocolVersion) {
        out.error = PeerError::unsupported_protocol;
        return out;
    }

    if (remote->nonce == local.nonce) {
        out.error = PeerError::self_connection;
        return out;
    }

    error = send_message(
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

    WireMessage verack;
    error = receive_message(
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

    out.session.emplace(
        params,
        store_socket(socket),
        false,
        *remote
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
    auto error = receive_message(
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
        kProtocolVersion) {
        out.error = PeerError::unsupported_protocol;
        return out;
    }

    if (remote->nonce == local.nonce) {
        out.error = PeerError::self_connection;
        return out;
    }

    const auto local_payload =
        serialize_version(local);

    error = send_message(
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
    error = receive_message(
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

    error = send_message(
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

    out.session.emplace(
        params,
        store_socket(socket),
        true,
        *remote
    );
    return out;
}

} // namespace

PeerSession::PeerSession(
    const consensus::ChainParams& params,
    std::uintptr_t socket,
    bool inbound,
    VersionMessage remote) noexcept
    : params_(params),
      socket_(socket),
      inbound_(inbound),
      remote_(remote)
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
      remote_(other.remote_)
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

PeerError PeerSession::ping(
    std::uint64_t nonce)
{
    if (!valid()) {
        return PeerError::receive_failed;
    }

    WireError wire_error{WireError::none};
    const auto payload = serialize_nonce(nonce);

    auto error = send_message(
        native_socket(socket_),
        params_,
        "ping",
        payload,
        wire_error
    );

    if (error != PeerError::none) {
        return error;
    }

    WireMessage message;
    error = receive_message(
        native_socket(socket_),
        params_,
        message,
        wire_error
    );

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

    WireError wire_error{WireError::none};
    WireMessage message;

    const auto error = receive_message(
        native_socket(socket_),
        params_,
        message,
        wire_error
    );

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

    return send_message(
        native_socket(socket_),
        params_,
        "pong",
        payload,
        wire_error
    );
}

void PeerSession::close() noexcept
{
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

    int reuse{1};

#ifdef _WIN32
    (void)setsockopt(
        socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuse),
        static_cast<int>(sizeof(reuse))
    );
#else
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
            timeout_ms)) {
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
            timeout_ms)) {
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

    if (!socket_runtime_ready()) {
        out.error =
            PeerError::socket_runtime_failed;
        return out;
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* addresses{nullptr};

    const std::string host_text{host};
    const std::string port_text =
        std::to_string(port);

    if (getaddrinfo(
            host_text.c_str(),
            port_text.c_str(),
            &hints,
            &addresses) != 0 ||
        addresses == nullptr) {
        out.error = PeerError::resolve_failed;
        return out;
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
        out.error = PeerError::connect_failed;
        return out;
    }

    out = outbound_handshake(
        params,
        connected,
        local
    );

    if (!out.ok()) {
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
