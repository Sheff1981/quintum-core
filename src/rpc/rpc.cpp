#include "rpc/rpc.hpp"

#include "consensus/pow.hpp"
#include "consensus/tx_auth.hpp"
#include "net/relay.hpp"
#include "net/sync.hpp"
#include "primitives/block.hpp"
#include "primitives/transaction.hpp"
#include "wallet/address.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace quintum::rpc {
namespace {

using Json = nlohmann::json;

struct RpcFailure final : std::runtime_error {
    int code;

    RpcFailure(int error_code, std::string message)
        : std::runtime_error(std::move(message)),
          code(error_code)
    {
    }
};

std::uint64_t unix_time_now() noexcept
{
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now()
                .time_since_epoch()
        ).count();

    return seconds < 0
        ? 0U
        : static_cast<std::uint64_t>(seconds);
}

std::string hex_encode(
    std::span<const Byte> bytes)
{
    constexpr std::array<char, 16> digits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    std::string out;
    out.reserve(bytes.size() * 2U);

    for (const Byte byte : bytes) {
        out.push_back(digits[byte >> 4U]);
        out.push_back(digits[byte & 0x0fU]);
    }

    return out;
}

std::string hash_hex(const Hash256& hash)
{
    return hex_encode(hash);
}

std::optional<Byte> hex_nibble(char ch) noexcept
{
    if (ch >= '0' && ch <= '9') {
        return static_cast<Byte>(ch - '0');
    }

    if (ch >= 'a' && ch <= 'f') {
        return static_cast<Byte>(
            10 + (ch - 'a')
        );
    }

    if (ch >= 'A' && ch <= 'F') {
        return static_cast<Byte>(
            10 + (ch - 'A')
        );
    }

    return std::nullopt;
}

std::optional<Bytes> hex_decode(
    std::string_view text,
    std::size_t max_bytes)
{
    if ((text.size() & 1U) != 0U ||
        text.size() / 2U > max_bytes) {
        return std::nullopt;
    }

    Bytes out;
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
            static_cast<Byte>(
                (*high << 4U) | *low
            )
        );
    }

    return out;
}

std::optional<Hash256> parse_hash(
    const Json& value)
{
    if (!value.is_string()) {
        return std::nullopt;
    }

    const std::string text =
        value.get<std::string>();

    const auto bytes =
        hex_decode(text, 32U);

    if (!bytes ||
        bytes->size() != 32U) {
        return std::nullopt;
    }

    Hash256 out{};
    std::copy(
        bytes->begin(),
        bytes->end(),
        out.begin()
    );
    return out;
}

std::string bits_hex(std::uint32_t bits)
{
    std::ostringstream stream;
    stream << std::hex
           << std::setfill('0')
           << std::setw(8)
           << bits;
    return stream.str();
}

Json make_error(
    const Json& id,
    int code,
    std::string_view message)
{
    return Json{
        {"jsonrpc", "2.0"},
        {"error", Json{
            {"code", code},
            {"message", message},
        }},
        {"id", id},
    };
}

Json make_result(
    const Json& id,
    Json result)
{
    return Json{
        {"jsonrpc", "2.0"},
        {"result", std::move(result)},
        {"id", id},
    };
}

const Json& normalized_params(
    const Json& request)
{
    static const Json empty =
        Json::array();

    if (!request.contains("params")) {
        return empty;
    }

    const Json& params =
        request.at("params");

    if (!params.is_array() &&
        !params.is_object()) {
        throw RpcFailure{
            -32602,
            "Invalid params"
        };
    }

    return params;
}

const Json& positional(
    const Json& params,
    std::size_t index)
{
    if (!params.is_array() ||
        index >= params.size()) {
        throw RpcFailure{
            -32602,
            "Invalid params"
        };
    }

    return params.at(index);
}

std::optional<std::string>
optional_payout_address(
    const Json& params)
{
    const Json* options{nullptr};

    if (params.is_object()) {
        options = &params;
    } else if (params.is_array() &&
               !params.empty()) {
        if (!params.front().is_object()) {
            throw RpcFailure{
                -32602,
                "getblocktemplate expects an options object"
            };
        }

        options = &params.front();
    }

    if (options == nullptr ||
        !options->contains(
            "payout_address")) {
        return std::nullopt;
    }

    const Json& value =
        options->at("payout_address");

    if (!value.is_string()) {
        throw RpcFailure{
            -32602,
            "payout_address must be a string"
        };
    }

    return value.get<std::string>();
}

std::string block_reject_reason(
    const NodeSubmitResult& submitted)
{
    if (submitted.error ==
        NodeSubmitError::not_started) {
        return "node-not-started";
    }

    if (submitted.error ==
        NodeSubmitError::storage_failed) {
        return "storage-failed";
    }

    switch (submitted.connect.chain.error) {
    case ChainConnectError::none:
        return "accepted";
    case ChainConnectError::duplicate_block:
        return "duplicate";
    case ChainConnectError::invalid_proof_of_work:
        return "high-hash";
    case ChainConnectError::unknown_parent:
    case ChainConnectError::bad_previous_block:
        return "prev-blk-not-found";
    case ChainConnectError::unexpected_difficulty:
        return "bad-diffbits";
    case ChainConnectError::timestamp_too_old:
        return "time-too-old";
    case ChainConnectError::timestamp_too_far_future:
        return "time-too-new";
    case ChainConnectError::invalid_coinbase_reward:
        return "bad-cb-amount";
    case ChainConnectError::transaction_failed:
        return "bad-tx";
    default:
        return "rejected";
    }
}

Json dispatch_method(
    const consensus::ChainParams& params,
    net::NetworkRuntime& runtime,
    std::string_view method,
    const Json& method_params)
{
    const auto status =
        runtime.status();

    if (method == "getblockcount") {
        return status.height.value_or(0U);
    }

    if (method == "getbestblockhash") {
        if (!status.tip) {
            throw RpcFailure{
                -5,
                "Block chain is empty"
            };
        }

        return hash_hex(*status.tip);
    }

    if (method == "getblockhash") {
        const Json& height_json =
            positional(method_params, 0U);

        if (!height_json.is_number_unsigned() &&
            !height_json.is_number_integer()) {
            throw RpcFailure{
                -32602,
                "Height must be an integer"
            };
        }

        const auto height =
            height_json.get<std::uint64_t>();

        if (height >
            std::numeric_limits<
                std::uint32_t>::max()) {
            throw RpcFailure{
                -8,
                "Block height out of range"
            };
        }

        const auto hash =
            runtime.active_hash(
                static_cast<std::uint32_t>(
                    height)
            );

        if (!hash) {
            throw RpcFailure{
                -8,
                "Block height out of range"
            };
        }

        return hash_hex(*hash);
    }

    if (method == "getblock") {
        const auto hash =
            parse_hash(
                positional(method_params, 0U)
            );

        if (!hash) {
            throw RpcFailure{
                -8,
                "Invalid block hash"
            };
        }

        int verbosity{1};

        if (method_params.is_array() &&
            method_params.size() > 1U) {
            if (!method_params[1U].
                    is_number_integer()) {
                throw RpcFailure{
                    -32602,
                    "Verbosity must be 0 or 1"
                };
            }

            verbosity =
                method_params[1U].get<int>();
        }

        if (verbosity != 0 &&
            verbosity != 1) {
            throw RpcFailure{
                -32602,
                "Verbosity must be 0 or 1"
            };
        }

        const auto block =
            runtime.block(*hash);

        if (!block) {
            throw RpcFailure{
                -5,
                "Block not found"
            };
        }

        if (verbosity == 0) {
            return hex_encode(
                net::serialize_block_payload(
                    *block
                )
            );
        }

        Json transactions =
            Json::array();

        for (const auto& tx :
             block->transactions) {
            transactions.push_back(
                hash_hex(
                    transaction_id(tx)
                )
            );
        }

        const auto active_height =
            runtime.active_height(*hash);

        std::int64_t confirmations{-1};

        if (active_height &&
            status.height &&
            *status.height >=
                *active_height) {
            confirmations =
                static_cast<std::int64_t>(
                    *status.height -
                    *active_height + 1U
                );
        }

        const auto size =
            serialized_block_size(*block);

        Json result{
            {"hash", hash_hex(*hash)},
            {"confirmations", confirmations},
            {"version", block->header.version},
            {"merkleroot",
             hash_hex(
                 block->header.merkle_root)},
            {"time", block->header.timestamp},
            {"bits",
             bits_hex(block->header.bits)},
            {"nonce", block->header.nonce},
            {"tx", std::move(transactions)},
        };

        if (size) {
            result["size"] = *size;
        }

        if (active_height) {
            result["height"] =
                *active_height;
        }

        if (std::any_of(
                block->header.previous_block.begin(),
                block->header.previous_block.end(),
                [](Byte byte) {
                    return byte != 0U;
                })) {
            result["previousblockhash"] =
                hash_hex(
                    block->header.
                        previous_block
                );
        }

        return result;
    }

    if (method == "getblockchaininfo") {
        Json result{
            {"chain", params.name},
            {"blocks",
             status.height.value_or(0U)},
            {"headers",
             status.peer_best_height.
                 value_or(
                     status.height.
                         value_or(0U))},
            {"verificationprogress",
             status.sync_progress},
            {"initialblockdownload",
             status.synchronizing},
            {"chainwork",
             hash_hex(
                 runtime.
                     cumulative_work())},
        };

        if (status.tip) {
            result["bestblockhash"] =
                hash_hex(*status.tip);
        }

        return result;
    }

    if (method == "getnetworkinfo") {
        return Json{
            {"protocolversion",
             params.p2p_protocol_version},
            {"networkactive",
             status.running},
            {"connections", status.peers},
            {"connections_out",
             status.outbound_peers},
            {"knownaddresses",
             status.known_addresses},
            {"localport",
             status.listen_port},
            {"network", params.name},
        };
    }

    if (method == "getmempoolinfo") {
        return Json{
            {"loaded", status.running},
            {"size",
             status.mempool_transactions},
            {"bytes",
             runtime.mempool_bytes()},
            {"maxmempool",
             kMaxMempoolBytes},
            {"minrelaytxfee_atomic_per_kb",
             status.
                 min_relay_fee_rate_per_kb},
        };
    }

    if (method == "getrawmempool") {
        Json result = Json::array();

        for (const auto& txid :
             runtime.
                 mempool_transaction_ids()) {
            result.push_back(
                hash_hex(txid)
            );
        }

        return result;
    }

    if (method == "getmininginfo") {
        return Json{
            {"blocks",
             status.height.value_or(0U)},
            {"currentblocktx",
             status.mempool_transactions},
            {"chain", params.name},
            {"powalgorithm",
             params.pow.pow_algorithm ==
                     consensus::PowAlgorithm::
                         randomx_v2
                 ? "randomx-v2"
                 : "sha256d"},
            {"targetspacing",
             params.pow.
                 target_spacing_seconds},
        };
    }

    if (method == "getblocktemplate") {
        auto payout =
            optional_payout_address(
                method_params
            );

        if (!payout) {
            if (status.receive_address.empty()) {
                throw RpcFailure{
                    -8,
                    "payout_address is required when the wallet is disabled"
                };
            }

            payout =
                status.receive_address;
        }

        const auto decoded =
            wallet::decode_address(
                params.network,
                *payout
            );

        if (!decoded.ok()) {
            throw RpcFailure{
                -8,
                "Invalid payout_address"
            };
        }

        const Bytes payout_script =
            consensus::
                make_p2pk_locking_script(
                    decoded.public_key
                );

        const auto built =
            runtime.mining_template(
                payout_script,
                unix_time_now()
            );

        if (!built.ok()) {
            throw RpcFailure{
                -32603,
                "Unable to build block template"
            };
        }

        const Block& block =
            built.block_template.
                value.block;

        const auto compact =
            consensus::
                decode_compact_target(
                    block.header.bits
                );

        if (!compact.valid()) {
            throw RpcFailure{
                -32603,
                "Invalid template target"
            };
        }

        Json transactions =
            Json::array();

        for (std::size_t i = 1U;
             i < block.transactions.size();
             ++i) {
            const auto& tx =
                block.transactions[i];

            transactions.push_back(
                Json{
                    {"txid",
                     hash_hex(
                         transaction_id(
                             tx))},
                    {"data",
                     hex_encode(
                         serialize_transaction(
                             tx))},
                }
            );
        }

        const auto& coinbase =
            block.transactions.front();

        Amount coinbase_value{0U};

        for (const auto& output :
             coinbase.outputs) {
            if (output.value >
                std::numeric_limits<
                    Amount>::max() -
                    coinbase_value) {
                throw RpcFailure{
                    -32603,
                    "Coinbase value overflow"
                };
            }

            coinbase_value +=
                output.value;
        }

        Json result{
            {"version",
             block.header.version},
            {"previousblockhash",
             hash_hex(
                 block.header.
                     previous_block)},
            {"transactions",
             std::move(transactions)},
            {"coinbasetxn",
             Json{
                 {"txid",
                  hash_hex(
                      transaction_id(
                          coinbase))},
                 {"data",
                  hex_encode(
                      serialize_transaction(
                          coinbase))},
             }},
            {"coinbasevalue",
             coinbase_value},
            {"height",
             built.block_template.
                 value.height},
            {"curtime",
             block.header.timestamp},
            {"bits",
             bits_hex(
                 block.header.bits)},
            {"target",
             hash_hex(compact.target)},
            {"noncerange",
             "0000000000000000ffffffffffffffff"},
            {"mutable",
             Json::array({"nonce"})},
            {"powalgorithm",
             params.pow.pow_algorithm ==
                     consensus::PowAlgorithm::
                         randomx_v2
                 ? "randomx-v2"
                 : "sha256d"},
            {"blockhex",
             hex_encode(
                 net::
                     serialize_block_payload(
                         block))},
            {"headerhex",
             hex_encode(
                 serialize_block_header(
                     block.header))},
        };

        if (params.pow.pow_algorithm ==
            consensus::PowAlgorithm::
                randomx_v2) {
            if (!built.randomx_seed) {
                throw RpcFailure{
                    -32603,
                    "RandomX seed unavailable"
                };
            }

            result["randomxseed"] =
                hash_hex(
                    *built.randomx_seed
                );
        }

        return result;
    }

    if (method == "submitblock") {
        const Json& raw =
            positional(method_params, 0U);

        if (!raw.is_string()) {
            throw RpcFailure{
                -32602,
                "submitblock expects block hex"
            };
        }

        const auto bytes =
            hex_decode(
                raw.get<std::string>(),
                static_cast<std::size_t>(
                    params.limits.
                        max_block_serialized_bytes)
            );

        if (!bytes) {
            throw RpcFailure{
                -22,
                "Block decode failed"
            };
        }

        const auto block =
            net::parse_block_payload(
                *bytes,
                params.limits
            );

        if (!block) {
            throw RpcFailure{
                -22,
                "Block decode failed"
            };
        }

        const auto submitted =
            runtime.submit_block(*block);

        if (!submitted.ok()) {
            return block_reject_reason(
                submitted
            );
        }

        return nullptr;
    }

    if (method == "sendrawtransaction") {
        const Json& raw =
            positional(method_params, 0U);

        if (!raw.is_string()) {
            throw RpcFailure{
                -32602,
                "sendrawtransaction expects transaction hex"
            };
        }

        const auto bytes =
            hex_decode(
                raw.get<std::string>(),
                static_cast<std::size_t>(
                    params.limits.
                        max_block_serialized_bytes)
            );

        if (!bytes) {
            throw RpcFailure{
                -22,
                "Transaction decode failed"
            };
        }

        const auto tx =
            net::parse_transaction_payload(
                *bytes,
                params.limits
            );

        if (!tx) {
            throw RpcFailure{
                -22,
                "Transaction decode failed"
            };
        }

        const auto accepted =
            runtime.
                submit_transaction(*tx);

        if (!accepted.ok()) {
            throw RpcFailure{
                -26,
                "Transaction rejected"
            };
        }

        return hash_hex(
            accepted.mempool.txid
        );
    }

    throw RpcFailure{
        -32601,
        "Method not found"
    };
}

std::optional<Json> handle_one(
    const consensus::ChainParams& params,
    net::NetworkRuntime& runtime,
    const Json& request)
{
    Json id = nullptr;
    bool notification{false};

    if (!request.is_object()) {
        return make_error(
            id,
            -32600,
            "Invalid Request"
        );
    }

    notification =
        !request.contains("id");

    if (!notification) {
        id = request.at("id");

        if (!id.is_null() &&
            !id.is_string() &&
            !id.is_number_integer() &&
            !id.is_number_unsigned()) {
            return make_error(
                nullptr,
                -32600,
                "Invalid Request"
            );
        }
    }

    if (!request.contains("jsonrpc") ||
        request.at("jsonrpc") != "2.0" ||
        !request.contains("method") ||
        !request.at("method").is_string()) {
        if (notification) {
            return std::nullopt;
        }

        return make_error(
            id,
            -32600,
            "Invalid Request"
        );
    }

    try {
        const auto result =
            dispatch_method(
                params,
                runtime,
                request.at("method").
                    get<std::string>(),
                normalized_params(request)
            );

        if (notification) {
            return std::nullopt;
        }

        return make_result(
            id,
            result
        );
    } catch (const RpcFailure& error) {
        if (notification) {
            return std::nullopt;
        }

        return make_error(
            id,
            error.code,
            error.what()
        );
    } catch (const Json::exception&) {
        if (notification) {
            return std::nullopt;
        }

        return make_error(
            id,
            -32602,
            "Invalid params"
        );
    } catch (...) {
        if (notification) {
            return std::nullopt;
        }

        return make_error(
            id,
            -32603,
            "Internal error"
        );
    }
}

} // namespace

RpcDispatcher::RpcDispatcher(
    const consensus::ChainParams& params,
    net::NetworkRuntime& runtime) noexcept
    : params_(params),
      runtime_(runtime)
{
}

std::string RpcDispatcher::handle(
    std::string_view request_body)
{
    Json request;

    try {
        request =
            Json::parse(request_body);
    } catch (const Json::parse_error&) {
        return make_error(
            nullptr,
            -32700,
            "Parse error"
        ).dump();
    }

    if (!request.is_array()) {
        const auto response =
            handle_one(
                params_,
                runtime_,
                request
            );

        return response
            ? response->dump()
            : std::string{};
    }

    if (request.empty()) {
        return make_error(
            nullptr,
            -32600,
            "Invalid Request"
        ).dump();
    }

    Json responses =
        Json::array();

    for (const auto& item : request) {
        const auto response =
            handle_one(
                params_,
                runtime_,
                item
            );

        if (response) {
            responses.push_back(
                *response
            );
        }
    }

    return responses.empty()
        ? std::string{}
        : responses.dump();
}

} // namespace quintum::rpc
