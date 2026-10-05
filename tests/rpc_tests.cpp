#include "consensus/pow.hpp"
#include "crypto/secp256k1.hpp"
#include "net/runtime.hpp"
#include "net/sync.hpp"
#include "rpc/rpc.hpp"
#include "rpc/server.hpp"
#include "wallet/address.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>

namespace {

using Json = nlohmann::json;

std::filesystem::path unique_dir(
    std::string_view suffix)
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    return std::filesystem::temp_directory_path() /
        ("quintum-rpc-" +
         std::string(suffix) + "-" +
         std::to_string(stamp));
}

std::string hex_encode(
    std::span<const quintum::Byte> bytes)
{
    constexpr std::array<char, 16> digits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    std::string out;
    out.reserve(bytes.size() * 2U);

    for (const quintum::Byte byte : bytes) {
        out.push_back(digits[byte >> 4U]);
        out.push_back(digits[byte & 0x0fU]);
    }

    return out;
}

std::optional<quintum::Byte>
hex_nibble(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return static_cast<quintum::Byte>(
            ch - '0'
        );
    }

    if (ch >= 'a' && ch <= 'f') {
        return static_cast<quintum::Byte>(
            10 + (ch - 'a')
        );
    }

    if (ch >= 'A' && ch <= 'F') {
        return static_cast<quintum::Byte>(
            10 + (ch - 'A')
        );
    }

    return std::nullopt;
}

std::optional<quintum::Bytes>
hex_decode(std::string_view text)
{
    if ((text.size() & 1U) != 0U) {
        return std::nullopt;
    }

    quintum::Bytes out;
    out.reserve(text.size() / 2U);

    for (std::size_t i = 0U;
         i < text.size();
         i += 2U) {
        const auto high =
            hex_nibble(text[i]);
        const auto low =
            hex_nibble(text[i + 1U]);

        if (!high || !low) {
            return std::nullopt;
        }

        out.push_back(
            static_cast<quintum::Byte>(
                (*high << 4U) | *low
            )
        );
    }

    return out;
}

std::string payout_address(
    const quintum::consensus::ChainParams& params)
{
    quintum::crypto::PrivateKey private_key{};
    private_key.back() = 41U;

    const auto public_key =
        quintum::crypto::derive_public_key(
            private_key
        );

    assert(public_key.has_value());

    return quintum::wallet::encode_address(
        params.network,
        *public_key
    );
}

Json call(
    quintum::rpc::RpcDispatcher& dispatcher,
    std::string_view method,
    Json params = Json::array(),
    std::uint64_t id = 1U)
{
    const Json request{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", method},
        {"params", std::move(params)},
    };

    const std::string response =
        dispatcher.handle(
            request.dump()
        );

    assert(!response.empty());
    return Json::parse(response);
}

void test_dispatcher_and_mining_roundtrip()
{
    using namespace quintum;

    const auto& params =
        consensus::regtest_params();
    const auto directory =
        unique_dir("dispatcher");

    net::NetworkRuntime runtime{
        params,
        directory
    };

    net::NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.target_outbound = 0U;
    config.allow_local_peers = true;
    config.wallet_enabled = false;

    assert(runtime.start(config).ok());

    rpc::RpcDispatcher dispatcher{
        params,
        runtime
    };

    const Json initial =
        call(
            dispatcher,
            "getblockcount"
        );

    assert(initial["result"] == 0U);
    assert(!initial.contains("error"));

    const Json chain_info =
        call(
            dispatcher,
            "getblockchaininfo"
        );

    assert(chain_info["result"]["chain"] ==
           "regtest");
    assert(chain_info["result"]["blocks"] ==
           0U);

    const Json mempool =
        call(
            dispatcher,
            "getmempoolinfo"
        );

    assert(mempool["result"]["size"] ==
           0U);

    const Json template_response =
        call(
            dispatcher,
            "getblocktemplate",
            Json::array({
                Json{
                    {"payout_address",
                     payout_address(params)}
                }
            })
        );

    assert(template_response["result"]["height"] ==
           1U);
    assert(template_response["result"]
                           ["powalgorithm"] ==
           "sha256d");

    const std::string block_hex =
        template_response["result"]
                         ["blockhex"].
            get<std::string>();

    const auto raw =
        hex_decode(block_hex);

    assert(raw.has_value());

    auto block =
        net::parse_block_payload(
            *raw,
            params.limits
        );

    assert(block.has_value());

    const auto mined =
        consensus::mine_header(
            block->header,
            4'096U
        );

    assert(mined.found());

    const Json submitted =
        call(
            dispatcher,
            "submitblock",
            Json::array({
                hex_encode(
                    net::serialize_block_payload(
                        *block
                    )
                )
            }),
            2U
        );

    assert(submitted["result"].is_null());
    assert(runtime.status().height ==
           std::optional<std::uint32_t>{1U});

    const Json duplicate =
        call(
            dispatcher,
            "submitblock",
            Json::array({
                hex_encode(
                    net::serialize_block_payload(
                        *block
                    )
                )
            }),
            3U
        );

    assert(duplicate["result"] ==
           "duplicate");

    const Json height_one =
        call(
            dispatcher,
            "getblockhash",
            Json::array({1U}),
            4U
        );

    const Json fetched =
        call(
            dispatcher,
            "getblock",
            Json::array({
                height_one["result"],
                1
            }),
            5U
        );

    assert(fetched["result"]["height"] ==
           1U);
    assert(fetched["result"]
                  ["confirmations"] ==
           1);

    const Json bad_method =
        call(
            dispatcher,
            "not_a_method",
            Json::array(),
            6U
        );

    assert(bad_method["error"]["code"] ==
           -32601);

    const Json bad_json =
        Json::parse(
            dispatcher.handle("{")
        );

    assert(bad_json["error"]["code"] ==
           -32700);

    runtime.stop();

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

void test_randomx_template_metadata()
{
    using namespace quintum;

    const auto& params =
        consensus::randomx_testnet_params();
    const auto directory =
        unique_dir("randomx-template");

    net::NetworkRuntime runtime{
        params,
        directory
    };

    net::NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.target_outbound = 0U;
    config.allow_local_peers = true;
    config.wallet_enabled = false;

    assert(runtime.start(config).ok());

    rpc::RpcDispatcher dispatcher{
        params,
        runtime
    };

    const Json response =
        call(
            dispatcher,
            "getblocktemplate",
            Json::array({
                Json{
                    {"payout_address",
                     payout_address(params)}
                }
            })
        );

    assert(response["result"]["height"] ==
           1U);
    assert(response["result"]
                   ["powalgorithm"] ==
           "randomx-v2");
    assert(response["result"].
               contains("randomxseed"));
    assert(response["result"]
                   ["randomxseed"].
               get<std::string>().
               size() == 64U);

    runtime.stop();

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

void test_cookie_authenticated_http_server()
{
    using namespace quintum;

    const auto& params =
        consensus::regtest_params();
    const auto directory =
        unique_dir("server");

    net::NetworkRuntime runtime{
        params,
        directory
    };

    net::NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.target_outbound = 0U;
    config.allow_local_peers = true;
    config.wallet_enabled = false;

    assert(runtime.start(config).ok());

    rpc::RpcServer server{
        params,
        runtime,
        directory
    };

    const auto started =
        server.start(0U);

    assert(started.ok());
    assert(started.port != 0U);
    assert(std::filesystem::exists(
        started.cookie_path
    ));

    const Json request{
        {"jsonrpc", "2.0"},
        {"id", 9U},
        {"method", "getblockcount"},
        {"params", Json::array()},
    };

    httplib::Client unauthorized{
        "127.0.0.1",
        static_cast<int>(started.port)
    };

    const auto unauthorized_result =
        unauthorized.Post(
            "/",
            request.dump(),
            "application/json"
        );

    assert(unauthorized_result);
    assert(unauthorized_result->status ==
           401);

    std::string cookie;

    {
        std::ifstream cookie_file{
            started.cookie_path,
            std::ios::binary
        };

        assert(cookie_file.is_open());

        std::getline(
            cookie_file,
            cookie
        );
        assert(cookie_file.good() ||
               cookie_file.eof());
    }

    constexpr std::string_view prefix{
        "__cookie__:"
    };

    assert(cookie.starts_with(prefix));

    httplib::Client authorized{
        "127.0.0.1",
        static_cast<int>(started.port)
    };

    authorized.set_basic_auth(
        "__cookie__",
        cookie.substr(prefix.size())
    );

    const auto authorized_result =
        authorized.Post(
            "/",
            request.dump(),
            "application/json"
        );

    assert(authorized_result);
    assert(authorized_result->status ==
           200);

    const Json response =
        Json::parse(
            authorized_result->body
        );

    assert(response["result"] == 0U);

    server.stop();

    assert(!std::filesystem::exists(
        started.cookie_path
    ));

    runtime.stop();

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

} // namespace

int main()
{
    test_dispatcher_and_mining_roundtrip();
    test_randomx_template_metadata();
    test_cookie_authenticated_http_server();
    return 0;
}
