#pragma once

#include "net/protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

namespace quintum::net {

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

    [[nodiscard]] PeerError ping(std::uint64_t nonce);
    [[nodiscard]] PeerError service_once();

    void close() noexcept;

    // Internal adoption constructor used by the handshake layer.
    PeerSession(
        const consensus::ChainParams& params,
        std::uintptr_t socket,
        bool inbound,
        VersionMessage remote
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
};

struct PeerHandshakeResult {
    PeerError error{PeerError::none};
    WireError wire_error{WireError::none};
    std::optional<PeerSession> session{};

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
