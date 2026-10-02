#include "consensus/chainparams.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "net/runtime.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace {

using quintum::Byte;
using quintum::Hash256;

volatile std::sig_atomic_t g_stop_requested{0};

void handle_signal(int) noexcept
{
    g_stop_requested = 1;
}

std::uint64_t unix_time_now() noexcept
{
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    return seconds < 0
        ? 0U
        : static_cast<std::uint64_t>(seconds);
}

int hex_value(char ch) noexcept
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

std::optional<quintum::crypto::PublicKey>
parse_public_key(std::string_view hex)
{
    quintum::crypto::PublicKey key{};

    if (hex.size() != key.size() * 2U) {
        return std::nullopt;
    }

    for (std::size_t i = 0U; i < key.size(); ++i) {
        const int high = hex_value(hex[i * 2U]);
        const int low = hex_value(hex[i * 2U + 1U]);

        if (high < 0 || low < 0) {
            return std::nullopt;
        }

        key[i] = static_cast<Byte>(
            (static_cast<unsigned>(high) << 4U) |
            static_cast<unsigned>(low)
        );
    }

    if (!quintum::crypto::is_valid_public_key(key)) {
        return std::nullopt;
    }

    return key;
}

bool parse_u64(
    std::string_view text,
    std::uint64_t& value) noexcept
{
    const char* begin = text.data();
    const char* end = text.data() + text.size();

    const auto parsed =
        std::from_chars(begin, end, value);

    return parsed.ec == std::errc{} &&
           parsed.ptr == end;
}

bool parse_u16(
    std::string_view text,
    std::uint16_t& value) noexcept
{
    std::uint64_t parsed{0U};
    if (!parse_u64(text, parsed) ||
        parsed >
            std::numeric_limits<std::uint16_t>::max()) {
        return false;
    }

    value = static_cast<std::uint16_t>(parsed);
    return true;
}

std::string hash_hex(const Hash256& hash)
{
    constexpr std::array<char, 16> digits{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };

    std::string out;
    out.reserve(hash.size() * 2U);

    for (const Byte byte : hash) {
        out.push_back(digits[byte >> 4U]);
        out.push_back(digits[byte & 0x0fU]);
    }

    return out;
}

void print_usage()
{
    std::cout
        << "QUINTUM Core development node\n"
        << "Usage: quintumd [--regtest|--testnet|--mainnet]"
        << " [--datadir PATH]"
        << " [--listen-port N]"
        << " [--mine-blocks N --miner-pubkey HEX]"
        << " [--max-attempts N]\n"
        << "The node keeps running until Ctrl+C.\n";
}

} // namespace

int main(int argc, char* argv[])
{
    using namespace quintum;

    consensus::Network network =
        consensus::Network::regtest;

    std::filesystem::path data_root =
        std::filesystem::current_path() /
        "quintum-data";

    std::uint64_t mine_blocks{0U};
    std::uint64_t max_attempts{5'000'000U};
    std::optional<std::uint16_t> listen_port;
    std::optional<crypto::PublicKey> miner_public_key;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};

        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        }

        if (arg == "--regtest") {
            network = consensus::Network::regtest;
            continue;
        }

        if (arg == "--testnet") {
            network = consensus::Network::testnet;
            continue;
        }

        if (arg == "--mainnet") {
            network = consensus::Network::mainnet;
            continue;
        }

        if (arg == "--datadir") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --datadir\n";
                return 2;
            }
            data_root = argv[++i];
            continue;
        }

        if (arg == "--listen-port") {
            std::uint16_t parsed_port{0U};

            if (i + 1 >= argc ||
                !parse_u16(
                    argv[++i],
                    parsed_port)) {
                std::cerr
                    << "Invalid --listen-port value\n";
                return 2;
            }

            listen_port = parsed_port;
            continue;
        }

        if (arg == "--mine-blocks") {
            if (i + 1 >= argc ||
                !parse_u64(argv[++i], mine_blocks)) {
                std::cerr << "Invalid --mine-blocks value\n";
                return 2;
            }
            continue;
        }

        if (arg == "--max-attempts") {
            if (i + 1 >= argc ||
                !parse_u64(argv[++i], max_attempts)) {
                std::cerr << "Invalid --max-attempts value\n";
                return 2;
            }
            continue;
        }

        if (arg == "--miner-pubkey") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --miner-pubkey\n";
                return 2;
            }

            miner_public_key =
                parse_public_key(argv[++i]);

            if (!miner_public_key) {
                std::cerr
                    << "Invalid compressed secp256k1 public key\n";
                return 2;
            }
            continue;
        }

        std::cerr << "Unknown argument: " << arg << '\n';
        print_usage();
        return 2;
    }

    if (mine_blocks > 0U && !miner_public_key) {
        std::cerr
            << "--miner-pubkey is required when mining\n";
        return 2;
    }

    const auto& params =
        consensus::chain_params(network);

    const std::filesystem::path network_directory =
        data_root / std::string(params.name);

    net::NetworkRuntime runtime{
        params,
        network_directory
    };

    net::NetworkRuntimeConfig network_config;
    network_config.listen_port = listen_port;

    const auto start_result =
        runtime.start(network_config);

    if (!start_result.ok()) {
        std::cerr
            << "Node startup failed: "
            << static_cast<int>(
                   start_result.error)
            << " node="
            << static_cast<int>(
                   start_result.node.error)
            << " addr="
            << static_cast<int>(
                   start_result.address_store)
            << " peer="
            << static_cast<int>(
                   start_result.peer_error)
            << '\n';
        return 1;
    }

    const auto initial_status =
        runtime.status();

    std::cout
        << "QUINTUM Core 0.0.1-dev\n"
        << "Network: " << params.name << '\n'
        << "Data directory: "
        << network_directory.string() << '\n'
        << "P2P listen port: "
        << initial_status.listen_port << '\n'
        << "Known peers: "
        << initial_status.known_addresses << '\n';

    if (initial_status.height) {
        std::cout
            << "Height: "
            << *initial_status.height
            << '\n';
    }

    if (initial_status.tip) {
        std::cout
            << "Tip: "
            << hash_hex(*initial_status.tip)
            << '\n';
    }

    if (mine_blocks > 0U) {
        const Bytes payout_script =
            consensus::make_p2pk_locking_script(
                *miner_public_key
            );

        const std::uint64_t base_time =
            unix_time_now();

        for (std::uint64_t i = 0U;
             i < mine_blocks;
             ++i) {
            if (i >
                std::numeric_limits<
                    std::uint64_t>::max() -
                    base_time) {
                std::cerr
                    << "Mining timestamp overflow\n";
                runtime.stop();
                return 1;
            }

            const auto mined =
                runtime.mine_mempool_block_at(
                    payout_script,
                    base_time + i,
                    max_attempts
                );

            if (!mined.ok()) {
                std::cerr
                    << "Mining failed: "
                    << static_cast<int>(
                           mined.error)
                    << " template="
                    << static_cast<int>(
                           mined.template_error)
                    << " chain="
                    << static_cast<int>(
                           mined.connect.chain.error)
                    << " storage="
                    << static_cast<int>(
                           mined.connect.storage_error)
                    << '\n';
                runtime.stop();
                return 1;
            }

            std::cout
                << "Mined height="
                << mined.height
                << " nonce="
                << mined.mining.nonce
                << " attempts="
                << mined.mining.attempts
                << " hash="
                << hash_hex(
                       block_hash(
                           mined.block.header))
                << '\n';
        }
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    std::cout
        << "P2P runtime active. Press Ctrl+C to stop.\n";

    while (g_stop_requested == 0) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(250)
        );
    }

    runtime.stop();

    const auto final_status =
        runtime.status();

    if (final_status.height) {
        std::cout
            << "Stopped at height "
            << *final_status.height
            << '\n';
    }

    return 0;
    return 0;
}
