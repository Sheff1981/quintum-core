#pragma once

#include "core/serialize.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace quintum::net {

struct Socks5Proxy {
    std::string host{"127.0.0.1"};
    std::uint16_t port{9050U};

    [[nodiscard]] bool valid() const noexcept
    {
        return !host.empty() && port != 0U;
    }
};

enum class Socks5Error {
    none,
    invalid_proxy,
    invalid_target,
    unsupported_auth,
    rejected,
    malformed_reply,
};

[[nodiscard]] std::optional<Socks5Proxy> parse_socks5_proxy(
    std::string_view text
);

[[nodiscard]] Bytes socks5_no_auth_greeting();

[[nodiscard]] bool socks5_no_auth_selected(
    std::span<const Byte> response
) noexcept;

[[nodiscard]] std::optional<Bytes> socks5_connect_request(
    std::string_view host,
    std::uint16_t port
);

[[nodiscard]] Socks5Error socks5_validate_reply_prefix(
    std::span<const Byte> prefix
) noexcept;

} // namespace quintum::net
