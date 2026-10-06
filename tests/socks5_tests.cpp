#include "net/socks5.hpp"

#include <array>
#include <cassert>
#include <string>

namespace {

void test_proxy_parser()
{
    using namespace quintum::net;

    const auto proxy =
        parse_socks5_proxy(
            "127.0.0.1:9050"
        );

    assert(proxy.has_value());
    assert(proxy->host == "127.0.0.1");
    assert(proxy->port == 9050U);

    assert(!parse_socks5_proxy(
        "127.0.0.1"));
    assert(!parse_socks5_proxy(
        ":9050"));
    assert(!parse_socks5_proxy(
        "127.0.0.1:0"));
    assert(!parse_socks5_proxy(
        "127.0.0.1:65536"));
    assert(!parse_socks5_proxy(
        "127.0.0.1:notaport"));
}

void test_no_auth_greeting()
{
    using namespace quintum::net;

    const auto greeting =
        socks5_no_auth_greeting();

    assert(greeting.size() == 3U);
    assert(greeting[0] == 0x05U);
    assert(greeting[1] == 0x01U);
    assert(greeting[2] == 0x00U);

    const std::array<quintum::Byte, 2>
        accepted{0x05U, 0x00U};
    const std::array<quintum::Byte, 2>
        rejected{0x05U, 0xffU};

    assert(socks5_no_auth_selected(accepted));
    assert(!socks5_no_auth_selected(rejected));
}

void test_connect_request_ipv4()
{
    using namespace quintum::net;

    const auto request =
        socks5_connect_request(
            "203.0.113.7",
            39444U
        );

    assert(request.has_value());
    assert(request->size() == 10U);
    assert((*request)[0] == 0x05U);
    assert((*request)[1] == 0x01U);
    assert((*request)[2] == 0x00U);
    assert((*request)[3] == 0x01U);
    assert((*request)[4] == 203U);
    assert((*request)[5] == 0U);
    assert((*request)[6] == 113U);
    assert((*request)[7] == 7U);
    assert((*request)[8] ==
           static_cast<quintum::Byte>(
               39444U >> 8U));
    assert((*request)[9] ==
           static_cast<quintum::Byte>(
               39444U & 0xffU));
}

void test_connect_request_remote_dns()
{
    using namespace quintum::net;

    const std::string onion =
        "abcdefghijklmnopqrstuvwxyz234567"
        "abcdefghijklmnopqrstuvwxyz234567"
        ".onion";

    const auto request =
        socks5_connect_request(
            onion,
            39444U
        );

    assert(request.has_value());
    assert((*request)[3] == 0x03U);
    assert((*request)[4] == onion.size());

    const std::string encoded(
        request->begin() + 5,
        request->begin() +
            static_cast<std::ptrdiff_t>(
                5U + onion.size())
    );

    assert(encoded == onion);
}

void test_reply_prefix()
{
    using namespace quintum::net;

    const std::array<quintum::Byte, 4>
        success{0x05U, 0x00U, 0x00U, 0x03U};
    const std::array<quintum::Byte, 4>
        denied{0x05U, 0x02U, 0x00U, 0x01U};
    const std::array<quintum::Byte, 4>
        malformed{0x04U, 0x00U, 0x00U, 0x01U};

    assert(socks5_validate_reply_prefix(success) ==
           Socks5Error::none);
    assert(socks5_validate_reply_prefix(denied) ==
           Socks5Error::rejected);
    assert(socks5_validate_reply_prefix(malformed) ==
           Socks5Error::malformed_reply);
}

} // namespace

int main()
{
    test_proxy_parser();
    test_no_auth_greeting();
    test_connect_request_ipv4();
    test_connect_request_remote_dns();
    test_reply_prefix();
    return 0;
}
