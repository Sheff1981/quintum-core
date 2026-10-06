#include "net/socks5.hpp"

#include "net/address.hpp"

#include <charconv>
#include <limits>
#include <system_error>
#include <utility>

namespace quintum::net {

std::optional<Socks5Proxy> parse_socks5_proxy(
    std::string_view text)
{
    const auto separator =
        text.rfind(':');

    if (separator == std::string_view::npos ||
        separator == 0U ||
        separator + 1U >= text.size() ||
        text.find(':') != separator) {
        return std::nullopt;
    }

    const auto host =
        text.substr(0U, separator);
    const auto port_text =
        text.substr(separator + 1U);

    std::uint32_t parsed_port{0U};

    const auto result =
        std::from_chars(
            port_text.data(),
            port_text.data() + port_text.size(),
            parsed_port
        );

    if (result.ec != std::errc{} ||
        result.ptr !=
            port_text.data() +
                port_text.size() ||
        parsed_port == 0U ||
        parsed_port >
            std::numeric_limits<
                std::uint16_t>::max()) {
        return std::nullopt;
    }

    Socks5Proxy proxy{
        .host = std::string{host},
        .port =
            static_cast<std::uint16_t>(
                parsed_port),
    };

    return proxy.valid()
        ? std::optional<Socks5Proxy>{
              std::move(proxy)}
        : std::nullopt;
}

Bytes socks5_no_auth_greeting()
{
    return Bytes{0x05U, 0x01U, 0x00U};
}

bool socks5_no_auth_selected(
    std::span<const Byte> response) noexcept
{
    return response.size() == 2U &&
           response[0] == 0x05U &&
           response[1] == 0x00U;
}

std::optional<Bytes> socks5_connect_request(
    std::string_view host,
    std::uint16_t port)
{
    if (host.empty() || port == 0U) {
        return std::nullopt;
    }

    Bytes out;
    out.reserve(4U + host.size() + 3U);
    out.push_back(0x05U);
    out.push_back(0x01U);
    out.push_back(0x00U);

    if (const auto ipv4 = parse_ipv4(host)) {
        out.push_back(0x01U);
        out.push_back(static_cast<Byte>(
            (*ipv4 >> 24U) & 0xffU));
        out.push_back(static_cast<Byte>(
            (*ipv4 >> 16U) & 0xffU));
        out.push_back(static_cast<Byte>(
            (*ipv4 >> 8U) & 0xffU));
        out.push_back(static_cast<Byte>(
            *ipv4 & 0xffU));
    } else {
        if (host.size() >
            static_cast<std::size_t>(
                std::numeric_limits<Byte>::max())) {
            return std::nullopt;
        }

        out.push_back(0x03U);
        out.push_back(
            static_cast<Byte>(host.size()));

        for (const char raw : host) {
            const auto ch =
                static_cast<unsigned char>(raw);

            if (ch == 0U) {
                return std::nullopt;
            }

            out.push_back(static_cast<Byte>(ch));
        }
    }

    out.push_back(
        static_cast<Byte>((port >> 8U) & 0xffU));
    out.push_back(
        static_cast<Byte>(port & 0xffU));

    return out;
}

Socks5Error socks5_validate_reply_prefix(
    std::span<const Byte> prefix) noexcept
{
    if (prefix.size() != 4U ||
        prefix[0] != 0x05U ||
        prefix[2] != 0x00U) {
        return Socks5Error::malformed_reply;
    }

    if (prefix[1] != 0x00U) {
        return Socks5Error::rejected;
    }

    if (prefix[3] != 0x01U &&
        prefix[3] != 0x03U &&
        prefix[3] != 0x04U) {
        return Socks5Error::malformed_reply;
    }

    return Socks5Error::none;
}

} // namespace quintum::net
