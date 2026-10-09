#pragma once

#include "net/address.hpp"
#include "net/protocol.hpp"
#include "net/socks5.hpp"
#include "net/v2_transport.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace quintum::net {

struct PeerHandshakeResult;

enum class PeerError {
    none,
    socket_runtime_failed,
    resolve_failed,
    socket_create_failed,
    bind_failed,
    listen_failed,
    accept_failed,
    connect_failed,
    timeout,
    send_failed,
    receive_failed,
    wire_error,
    malformed_version,
    unsupported_protocol,
    self_connection,
    unexpected_message,
    malformed_ping,
    encryption_failed,
    proxy_negotiation_failed,
    proxy_rejected,
};

class PeerSession {
public:
    PeerSession() noexcept = default;
    ~PeerSession();

    PeerSession(const PeerSession&) = delete;
    PeerSession& operator=(const PeerSession&) = delete;

    PeerSession(PeerSession&& other) noexcept;
    PeerSession& operator=(PeerSession&& other) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool inbound() const noexcept;
    [[nodiscard]] const VersionMessage& remote_version() const noexcept;
    [[nodiscard]] bool encrypted() const noexcept;
    [[nodiscard]] std::optional<Hash256> session_id() const noexcept;

    [[nodiscard]] bool wait_readable(
        std::uint32_t timeout_ms
    ) const noexcept;

    [[nodiscard]] PeerError send_command(
        std::string_view command,
        std::span<const Byte> payload
    );
    [[nodiscard]] PeerError receive_command(
        WireMessage& message
    );

    [[nodiscard]] PeerError ping(std::uint64_t nonce);
    [[nodiscard]] PeerError service_once();

    [[nodiscard]] PeerError request_addresses(
        bool allow_local,
        std::vector<PeerAddress>& addresses
    );

    [[nodiscard]] PeerError service_discovery_once(
        std::span<const PeerAddress> advertised,
        bool allow_local,
        std::vector<PeerAddress>* learned = nullptr
    );

    void close() noexcept;

    // Internal adoption constructor used by the handshake layer.
    PeerSession(
        const consensus::ChainParams& params,
        std::uintptr_t socket,
        bool inbound,
        VersionMessage remote,
        std::unique_ptr<V2Transport> transport = nullptr
    ) noexcept;

private:
    friend class PeerListener;
    friend struct PeerHandshakeResult;
    friend PeerHandshakeResult connect_and_handshake(
        const consensus::ChainParams&,
        std::string_view,
        std::uint16_t,
        const VersionMessage&,
        std::uint32_t
    );

    static constexpr std::uintptr_t kInvalidSocket =
        std::numeric_limits<std::uintptr_t>::max();

    consensus::ChainParams params_{};
    std::uintptr_t socket_{kInvalidSocket};
    bool inbound_{false};
    VersionMessage remote_{};
    std::unique_ptr<V2Transport> transport_{};
};

struct PeerHandshakeResult {
    PeerError error{PeerError::none};
    WireError wire_error{WireError::none};
    // Last handshake phase; static labels only, no payload or credential data.
    std::string_view phase{"tcp-connect"};
    std::optional<PeerSession> session{};
    std::optional<std::uint32_t> observed_ipv4{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == PeerError::none &&
               session.has_value();
    }
};

class PeerListener {
public:
    explicit PeerListener(
        const consensus::ChainParams& params
    ) noexcept;
    ~PeerListener();

    PeerListener(const PeerListener&) = delete;
    PeerListener& operator=(const PeerListener&) = delete;

    PeerListener(PeerListener&& other) noexcept;
    PeerListener& operator=(PeerListener&& other) noexcept;

    [[nodiscard]] PeerError listen(
        std::string_view bind_address,
        std::uint16_t port
    );

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] std::uint16_t local_port() const noexcept;

    [[nodiscard]] PeerHandshakeResult accept_and_handshake(
        const VersionMessage& local,
        std::uint32_t timeout_ms
    );

    [[nodiscard]] PeerHandshakeResult accept_and_handshake(
        const VersionMessage& local,
        std::uint32_t accept_timeout_ms,
        std::uint32_t io_timeout_ms
    );

    void close() noexcept;

private:
    static constexpr std::uintptr_t kInvalidSocket =
        std::numeric_limits<std::uintptr_t>::max();

    consensus::ChainParams params_{};
    std::uintptr_t socket_{kInvalidSocket};
    std::uint16_t local_port_{0U};
};

[[nodiscard]] PeerHandshakeResult connect_and_handshake(
    const consensus::ChainParams& params,
    std::string_view host,
    std::uint16_t port,
    const VersionMessage& local,
    std::uint32_t timeout_ms
);

[[nodiscard]] PeerHandshakeResult connect_and_handshake(
    const consensus::ChainParams& params,
    std::string_view host,
    std::uint16_t port,
    const VersionMessage& local,
    std::uint32_t timeout_ms,
    const Socks5Proxy& proxy
);

class ConnectionManager {
public:
    [[nodiscard]] bool add(PeerSession session);
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] PeerSession* peer(std::size_t index) noexcept;
    [[nodiscard]] const PeerSession* peer(
        std::size_t index
    ) const noexcept;

    void prune_closed();
    void disconnect_all() noexcept;

private:
    std::vector<PeerSession> peers_{};
};

} // namespace quintum::net
