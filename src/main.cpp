#include "consensus/chainparams.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "crypto/random.hpp"
#include "net/runtime.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

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

void wipe_string(std::string& value) noexcept
{
    if (!value.empty()) {
        quintum::crypto::secure_erase(
            std::span<quintum::Byte>{
                reinterpret_cast<quintum::Byte*>(
                    value.data()),
                value.size()
            }
        );
    }

    value.clear();
}

std::optional<std::string> read_passphrase_file(
    const std::filesystem::path& path)
{
    std::ifstream input(
        path,
        std::ios::binary
    );

    if (!input) {
        return std::nullopt;
    }

    std::string value{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };

    if (!input.good() && !input.eof()) {
        wipe_string(value);
        return std::nullopt;
    }

    constexpr std::size_t kMaxPassphraseBytes{
        4'096U
    };

    if (value.size() >
        kMaxPassphraseBytes) {
        wipe_string(value);
        return std::nullopt;
    }

    while (!value.empty() &&
           (value.back() == '\n' ||
            value.back() == '\r')) {
        value.pop_back();
    }

    if (value.empty()) {
        return std::nullopt;
    }

    return value;
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
        << " [--addnode HOST[:PORT]]"
        << " [--run-seconds N]"
        << " [--network-only]"
        << " [--new-address]"
        << " [--send-to ADDRESS --amount ATOMIC [--fee ATOMIC]]"
        << " [--backup-wallet PATH]"
        << " [--wallet-passphrase-file PATH]"
        << " [--encrypt-wallet]"
        << " [--mine-blocks N [--miner-pubkey HEX]]"
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
    std::vector<std::string> addnodes{};
    std::optional<std::uint64_t> run_seconds{};
    bool network_only{false};
    std::optional<crypto::PublicKey> miner_public_key;
    bool new_address_requested{false};
    std::optional<std::string> send_to;
    std::optional<Amount> send_amount;
    Amount send_fee{0U};
    bool fee_was_set{false};
    std::optional<std::filesystem::path> wallet_backup;
    std::optional<std::filesystem::path>
        wallet_passphrase_file;
    bool encrypt_wallet_requested{false};

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

        if (arg == "--addnode") {
            if (i + 1 >= argc) {
                std::cerr
                    << "Missing value for --addnode\n";
                return 2;
            }

            addnodes.emplace_back(argv[++i]);
            continue;
        }

        if (arg == "--run-seconds") {
            std::uint64_t value{0U};

            if (i + 1 >= argc ||
                !parse_u64(argv[++i], value) ||
                value == 0U) {
                std::cerr
                    << "Invalid --run-seconds value\n";
                return 2;
            }

            run_seconds = value;
            continue;
        }

        if (arg == "--network-only") {
            network_only = true;
            continue;
        }

        if (arg == "--new-address") {
            new_address_requested = true;
            continue;
        }

        if (arg == "--send-to") {
            if (i + 1 >= argc) {
                std::cerr
                    << "Missing value for --send-to\n";
                return 2;
            }

            send_to =
                std::string{argv[++i]};
            continue;
        }

        if (arg == "--amount") {
            std::uint64_t value{0U};

            if (i + 1 >= argc ||
                !parse_u64(
                    argv[++i],
                    value)) {
                std::cerr
                    << "Invalid --amount value\n";
                return 2;
            }

            send_amount =
                static_cast<Amount>(value);
            continue;
        }

        if (arg == "--fee") {
            std::uint64_t value{0U};

            if (i + 1 >= argc ||
                !parse_u64(
                    argv[++i],
                    value)) {
                std::cerr
                    << "Invalid --fee value\n";
                return 2;
            }

            send_fee =
                static_cast<Amount>(value);
            fee_was_set = true;
            continue;
        }

        if (arg == "--backup-wallet") {
            if (i + 1 >= argc) {
                std::cerr
                    << "Missing value for --backup-wallet\n";
                return 2;
            }

            wallet_backup =
                std::filesystem::path{
                    argv[++i]};
            continue;
        }

        if (arg == "--wallet-passphrase-file") {
            if (i + 1 >= argc) {
                std::cerr
                    << "Missing value for --wallet-passphrase-file\n";
                return 2;
            }

            wallet_passphrase_file =
                std::filesystem::path{
                    argv[++i]
                };
            continue;
        }

        if (arg == "--encrypt-wallet") {
            encrypt_wallet_requested = true;
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

    if (send_to.has_value() !=
        send_amount.has_value()) {
        std::cerr
            << "--send-to and --amount must be used together\n";
        return 2;
    }

    if (fee_was_set && !send_to) {
        std::cerr
            << "--fee requires --send-to and --amount\n";
        return 2;
    }

    if (encrypt_wallet_requested &&
        !wallet_passphrase_file) {
        std::cerr
            << "--encrypt-wallet requires --wallet-passphrase-file\n";
        return 2;
    }

    if (network_only &&
        (wallet_passphrase_file ||
         new_address_requested ||
         send_to ||
         wallet_backup ||
         encrypt_wallet_requested ||
         (mine_blocks > 0U &&
          !miner_public_key))) {
        std::cerr
            << "--network-only cannot use wallet operations; "
            << "mining requires --miner-pubkey\n";
        return 2;
    }

    std::string wallet_passphrase;

    if (wallet_passphrase_file) {
        auto loaded =
            read_passphrase_file(
                *wallet_passphrase_file
            );

        if (!loaded) {
            std::cerr
                << "Unable to read a non-empty wallet passphrase file\n";
            return 2;
        }

        wallet_passphrase =
            std::move(*loaded);
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
    network_config.enable_wallet =
        !network_only;
    network_config.wallet_passphrase =
        wallet_passphrase;

    const std::uint64_t addnode_seen =
        unix_time_now();

    for (const auto& endpoint : addnodes) {
        std::string_view host{endpoint};
        std::uint16_t port =
            static_cast<std::uint16_t>(
                params.p2p_port
            );

        const auto colon = host.rfind(':');

        if (colon != std::string_view::npos) {
            if (host.find(':') != colon) {
                wipe_string(wallet_passphrase);
                std::cerr
                    << "--addnode currently supports IPv4/DNS only: "
                    << endpoint << '\n';
                return 2;
            }

            const std::string_view port_text =
                host.substr(colon + 1U);
            host = host.substr(0U, colon);

            if (host.empty() ||
                port_text.empty() ||
                !parse_u16(port_text, port) ||
                port == 0U) {
                wipe_string(wallet_passphrase);
                std::cerr
                    << "Invalid --addnode endpoint: "
                    << endpoint << '\n';
                return 2;
            }
        }

        if (host.empty()) {
            wipe_string(wallet_passphrase);
            std::cerr
                << "Invalid --addnode endpoint\n";
            return 2;
        }

        const auto resolved =
            net::resolve_ipv4_host(host);

        if (resolved.empty()) {
            wipe_string(wallet_passphrase);
            std::cerr
                << "Unable to resolve --addnode host: "
                << host << '\n';
            return 2;
        }

        for (const auto ipv4 : resolved) {
            network_config.bootstrap_peers.push_back(
                net::PeerAddress{
                    .ipv4 = ipv4,
                    .port = port,
                    .services = 1U,
                    .last_seen = addnode_seen,
                }
            );
        }
    }

    const auto start_result =
        runtime.start(
            std::move(network_config)
        );

    if (start_result.ok() &&
        encrypt_wallet_requested) {
        const auto encryption_error =
            runtime.encrypt_wallet(
                wallet_passphrase
            );

        if (encryption_error !=
            wallet::WalletStoreError::none) {
            wipe_string(wallet_passphrase);
            std::cerr
                << "Wallet encryption failed: "
                << static_cast<int>(
                       encryption_error)
                << '\n';
            runtime.stop();
            return 1;
        }

        std::cout
            << "Wallet encryption enabled\n";
    }

    wipe_string(wallet_passphrase);

    if (!start_result.ok()) {
        std::cerr
            << "Node startup failed: "
            << static_cast<int>(
                   start_result.error)
            << " node="
            << static_cast<int>(
                   start_result.node.error)
            << " wallet="
            << static_cast<int>(
                   start_result.wallet.error)
            << " wallet_store="
            << static_cast<int>(
                   start_result.wallet.store_error)
            << " wallet_sync="
            << static_cast<int>(
                   start_result.wallet_sync)
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

    if (start_result.wallet.backup_recommended) {
        std::cout
            << "Wallet backup recommended: newly created keypool\n";
    }

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

    if (initial_status.wallet_enabled) {
        if (!initial_status.receive_address.empty()) {
            std::cout
                << "Receive address: "
                << initial_status.receive_address
                << '\n';
        }

        std::cout
            << "Wallet confirmed: "
            << initial_status.wallet_balance.confirmed
            << " atomic\n"
            << "Wallet available: "
            << initial_status.wallet_balance.available
            << " atomic\n"
            << "Wallet pending: "
            << initial_status.wallet_balance.pending
            << " atomic\n"
            << "Wallet immature: "
            << initial_status.wallet_balance.immature
            << " atomic\n";
    } else {
        std::cout
            << "Wallet: disabled (network-only mode)\n";
    }

    std::cout
        << "Min relay fee rate: "
        << initial_status.min_relay_fee_rate_per_kb
        << " atomic/1000 bytes\n"
        << "Recommended fee rate: "
        << initial_status.recommended_fee_rate_per_kb
        << " atomic/1000 bytes\n";

    if (new_address_requested) {
        const auto generated =
            runtime.new_receive_address();

        if (!generated.ok()) {
            std::cerr
                << "New address failed: "
                << static_cast<int>(
                       generated.error)
                << " store="
                << static_cast<int>(
                       generated.store_error)
                << '\n';
            runtime.stop();
            return 1;
        }

        std::cout
            << "New receive address: "
            << generated.address
            << '\n';

        if (generated.backup_recommended) {
            std::cout
                << "Wallet backup recommended: keypool refilled\n";
        }
    }

    if (send_to && send_amount) {
        const auto sent =
            fee_was_set
                ? runtime.send_to_address(
                      *send_to,
                      *send_amount,
                      send_fee
                  )
                : runtime.send_to_address_auto_fee(
                      *send_to,
                      *send_amount
                  );

        if (!sent.ok()) {
            std::cerr
                << "Wallet send failed: "
                << static_cast<int>(
                       sent.error)
                << " create="
                << static_cast<int>(
                       sent.wallet.error)
                << " address="
                << static_cast<int>(
                       sent.wallet.address_error)
                << " node="
                << static_cast<int>(
                       sent.node.error)
                << '\n';
            runtime.stop();
            return 1;
        }

        std::cout
            << "Transaction submitted: "
            << hash_hex(
                   sent.node.mempool.txid)
            << " fee="
            << sent.node.mempool.fee
            << " atomic rate="
            << sent.wallet.fee_rate_per_kb
            << " atomic/1000 bytes size="
            << sent.wallet.serialized_size
            << " bytes"
            << (fee_was_set ? " manual" : " auto")
            << '\n';

        if (sent.wallet.backup_recommended) {
            std::cout
                << "Wallet backup recommended: change keypool refilled\n";
        }
    }

    if (mine_blocks > 0U) {
        crypto::PublicKey payout_key{};

        if (miner_public_key) {
            payout_key =
                *miner_public_key;
        } else {
            const auto status =
                runtime.status();

            const auto decoded =
                wallet::decode_address(
                    params.network,
                    status.receive_address
                );

            if (!decoded.ok()) {
                std::cerr
                    << "Wallet mining address unavailable\n";
                runtime.stop();
                return 1;
            }

            payout_key =
                decoded.public_key;
        }

        const Bytes payout_script =
            consensus::make_p2pk_locking_script(
                payout_key
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

    if (wallet_backup) {
        const auto backup_error =
            runtime.backup_wallet(
                *wallet_backup,
                false
            );

        if (backup_error !=
            wallet::WalletStoreError::none) {
            std::cerr
                << "Wallet backup failed: "
                << static_cast<int>(
                       backup_error)
                << '\n';
            runtime.stop();
            return 1;
        }

        std::cout
            << "Wallet backup: "
            << wallet_backup->string()
            << '\n';
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    std::cout
        << "P2P runtime active. Press Ctrl+C to stop.\n";

    const auto run_started =
        std::chrono::steady_clock::now();

    while (g_stop_requested == 0 &&
           runtime.running()) {
        if (run_seconds) {
            const auto elapsed_seconds =
                std::chrono::duration_cast<
                    std::chrono::seconds>(
                    std::chrono::steady_clock::now() -
                    run_started
                ).count();

            if (elapsed_seconds >= 0 &&
                static_cast<std::uint64_t>(
                    elapsed_seconds
                ) >= *run_seconds) {
                break;
            }
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(250)
        );
    }

    const bool unexpected_stop =
        g_stop_requested == 0 &&
        !runtime.running();

    const auto final_status =
        runtime.status();

    runtime.stop();

    if (unexpected_stop) {
        std::cerr
            << "P2P runtime stopped unexpectedly\n";
        return 1;
    }

    std::cout
        << "Final peers: "
        << final_status.peers
        << " (outbound "
        << final_status.outbound_peers
        << ")\n";

    if (final_status.height) {
        std::cout
            << "Stopped at height "
            << *final_status.height
            << '\n';
    }

    return 0;
}
