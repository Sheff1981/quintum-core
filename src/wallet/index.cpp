#include "wallet/wallet.hpp"

#include "consensus/monetary.hpp"
#include "core/serialize.hpp"
#include "crypto/sha256.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <limits>
#include <span>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace quintum::wallet {
namespace {

constexpr std::array<Byte, 8> kStateMagic{
    'Q', 'W', 'S', 'T', 'A', 'T', 'E', '1'
};
constexpr std::uint32_t kStateVersion{1U};
constexpr std::size_t kChecksumSize{32U};
constexpr std::uintmax_t kMaxStateFileSize{
    128U * 1024U * 1024U
};
constexpr std::size_t kMaxIndexedCoins{1'000'000U};
constexpr std::size_t kMaxHistoryRecords{1'000'000U};

bool flush_file(std::FILE* file) noexcept
{
    if (std::fflush(file) != 0) {
        return false;
    }

#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#else
    return ::fsync(fileno(file)) == 0;
#endif
}

bool replace_file(
    const std::filesystem::path& source,
    const std::filesystem::path& destination)
{
#ifdef _WIN32
    return MoveFileExW(
               source.c_str(),
               destination.c_str(),
               MOVEFILE_REPLACE_EXISTING |
                   MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (::rename(
            source.c_str(),
            destination.c_str()) != 0) {
        return false;
    }

    std::filesystem::path directory =
        destination.parent_path();

    if (directory.empty()) {
        directory = ".";
    }

    const int fd = ::open(
        directory.c_str(),
        O_RDONLY | O_DIRECTORY
    );

    if (fd >= 0) {
        (void)::fsync(fd);
        (void)::close(fd);
    }

    return true;
#endif
}

WalletStoreError write_state_atomic(
    const std::filesystem::path& destination,
    std::span<const Byte> bytes)
{
    std::error_code ec;

    const auto parent =
        destination.parent_path();

    if (!parent.empty()) {
        std::filesystem::create_directories(
            parent,
            ec
        );

        if (ec) {
            return WalletStoreError::io_error;
        }
    }

    auto temporary = destination;
    temporary += ".tmp";

#ifdef _WIN32
    std::FILE* file{nullptr};

    if (_wfopen_s(
            &file,
            temporary.c_str(),
            L"wb") != 0) {
        file = nullptr;
    }
#else
    std::FILE* file =
        std::fopen(
            temporary.c_str(),
            "wb"
        );
#endif

    if (file == nullptr) {
        return WalletStoreError::io_error;
    }

#ifndef _WIN32
    if (::chmod(
            temporary.c_str(),
            S_IRUSR | S_IWUSR) != 0) {
        (void)std::fclose(file);
        std::filesystem::remove(
            temporary,
            ec
        );
        return WalletStoreError::io_error;
    }
#endif

    const std::size_t written =
        bytes.empty()
            ? 0U
            : std::fwrite(
                  bytes.data(),
                  1U,
                  bytes.size(),
                  file
              );

    const bool durable =
        written == bytes.size() &&
        flush_file(file);

    const bool closed =
        std::fclose(file) == 0;

    if (!durable ||
        !closed ||
        !replace_file(
            temporary,
            destination)) {
        std::filesystem::remove(
            temporary,
            ec
        );
        return WalletStoreError::io_error;
    }

    return WalletStoreError::none;
}

std::optional<Bytes> read_state_file(
    const std::filesystem::path& path)
{
    std::ifstream input(
        path,
        std::ios::binary
    );

    if (!input) {
        return std::nullopt;
    }

    input.seekg(0, std::ios::end);
    const auto end = input.tellg();

    if (end < 0) {
        return std::nullopt;
    }

    const auto size =
        static_cast<std::uintmax_t>(end);

    if (size > kMaxStateFileSize ||
        size >
            static_cast<std::uintmax_t>(
                std::numeric_limits<
                    std::size_t>::max())) {
        return std::nullopt;
    }

    Bytes bytes(
        static_cast<std::size_t>(size)
    );

    input.seekg(0, std::ios::beg);

    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(
                bytes.data()),
            static_cast<std::streamsize>(
                bytes.size())
        );
    }

    if (!input) {
        return std::nullopt;
    }

    return bytes;
}

bool read_bytes(
    std::span<const Byte> input,
    std::size_t& offset,
    std::span<Byte> output) noexcept
{
    if (offset > input.size() ||
        output.size() >
            input.size() - offset) {
        return false;
    }

    std::copy_n(
        input.begin() +
            static_cast<std::ptrdiff_t>(
                offset),
        static_cast<std::ptrdiff_t>(
            output.size()),
        output.begin()
    );

    offset += output.size();
    return true;
}

bool checked_add(
    Amount& total,
    Amount value) noexcept
{
    if (!consensus::money_range(value) ||
        value >
            consensus::kMaxMoney - total) {
        return false;
    }

    total += value;
    return true;
}

void append_hash(
    Bytes& output,
    const Hash256& hash)
{
    output.insert(
        output.end(),
        hash.begin(),
        hash.end()
    );
}

} // namespace

void Wallet::reset_index_state() noexcept
{
    index_valid_ = false;
    indexed_height_ = 0U;
    indexed_tip_ = {};
    confirmed_history_.clear();
    history_.clear();
    confirmed_coins_.clear();
    available_coins_.clear();
    pending_coins_.clear();
    balance_ = {};
}

Hash256 Wallet::wallet_index_id() const
{
    if (keys_.empty()) {
        return {};
    }

    return crypto::double_sha256(
        std::span<const Byte>{
            keys_.front().public_key.data(),
            keys_.front().public_key.size()
        }
    );
}

void Wallet::load_index_state() noexcept
{
    reset_index_state();

    try {
        std::error_code ec;

        if (!std::filesystem::exists(
                state_path_,
                ec) ||
            ec) {
            return;
        }

        auto bytes =
            read_state_file(state_path_);

        if (!bytes ||
            bytes->size() <
                kStateMagic.size() +
                sizeof(std::uint32_t) +
                1U +
                params_.message_start.size() +
                Hash256{}.size() +
                sizeof(std::uint32_t) +
                Hash256{}.size() +
                1U +
                1U +
                kChecksumSize) {
            return;
        }

        const std::span<const Byte> all{
            bytes->data(),
            bytes->size()
        };

        const std::span<const Byte> body{
            bytes->data(),
            bytes->size() - kChecksumSize
        };

        const auto checksum =
            crypto::double_sha256(body);

        if (!std::equal(
                checksum.begin(),
                checksum.end(),
                bytes->end() -
                    static_cast<
                        std::ptrdiff_t>(
                            kChecksumSize))) {
            return;
        }

        std::size_t offset{0U};

        if (!std::equal(
                kStateMagic.begin(),
                kStateMagic.end(),
                body.begin())) {
            return;
        }

        offset += kStateMagic.size();

        const auto version =
            read_little_endian<
                std::uint32_t>(
                    body,
                    offset
                );

        if (!version ||
            *version != kStateVersion ||
            offset >= body.size()) {
            return;
        }

        const Byte network =
            body[offset++];

        if (network !=
            static_cast<Byte>(
                params_.network)) {
            return;
        }

        if (offset +
                params_.message_start.size() >
            body.size() ||
            !std::equal(
                params_.message_start.begin(),
                params_.message_start.end(),
                body.begin() +
                    static_cast<
                        std::ptrdiff_t>(
                            offset))) {
            return;
        }

        offset +=
            params_.message_start.size();

        Hash256 stored_wallet_id{};

        if (!read_bytes(
                body,
                offset,
                stored_wallet_id) ||
            stored_wallet_id !=
                wallet_index_id()) {
            return;
        }

        const auto height =
            read_little_endian<
                std::uint32_t>(
                    body,
                    offset
                );

        Hash256 tip{};

        if (!height ||
            !read_bytes(
                body,
                offset,
                tip)) {
            return;
        }

        const auto coin_count =
            read_compact_size(
                body,
                offset
            );

        if (!coin_count ||
            *coin_count >
                kMaxIndexedCoins) {
            return;
        }

        std::map<
            OutPoint,
            WalletCoin,
            OutPointLess> coins;

        for (std::uint64_t i = 0U;
             i < *coin_count;
             ++i) {
            Hash256 txid{};

            if (!read_bytes(
                    body,
                    offset,
                    txid)) {
                return;
            }

            const auto output_index =
                read_little_endian<
                    std::uint32_t>(
                        body,
                        offset
                    );

            const auto value =
                read_little_endian<
                    Amount>(
                        body,
                        offset
                    );

            const auto script_size =
                read_compact_size(
                    body,
                    offset
                );

            if (!output_index ||
                !value ||
                !script_size ||
                !consensus::money_range(
                    *value) ||
                *script_size >
                    params_.limits
                        .max_script_bytes ||
                *script_size >
                    body.size() - offset) {
                return;
            }

            Bytes script(
                static_cast<std::size_t>(
                    *script_size)
            );

            if (!read_bytes(
                    body,
                    offset,
                    script)) {
                return;
            }

            const auto coin_height =
                read_little_endian<
                    std::uint32_t>(
                        body,
                        offset
                    );

            if (!coin_height ||
                *coin_height > *height ||
                offset >= body.size()) {
                return;
            }

            const Byte coinbase =
                body[offset++];

            if (coinbase > 1U) {
                return;
            }

            crypto::PublicKey public_key{};

            if (!read_bytes(
                    body,
                    offset,
                    public_key) ||
                !crypto::is_valid_public_key(
                    public_key) ||
                !owns_public_key(public_key)) {
                return;
            }

            const auto parsed =
                consensus::
                    parse_p2pk_locking_script(
                        script
                    );

            if (!parsed ||
                *parsed != public_key) {
                return;
            }

            const OutPoint outpoint{
                .txid = txid,
                .index = *output_index,
            };

            const auto [it, inserted] =
                coins.emplace(
                    outpoint,
                    WalletCoin{
                        .outpoint = outpoint,
                        .coin = Coin{
                            .output = TxOutput{
                                .value = *value,
                                .locking_script =
                                    std::move(script),
                            },
                            .height = *coin_height,
                            .coinbase =
                                coinbase != 0U,
                        },
                        .public_key = public_key,
                    }
                );

            (void)it;

            if (!inserted) {
                return;
            }
        }

        const auto history_count =
            read_compact_size(
                body,
                offset
            );

        if (!history_count ||
            *history_count >
                kMaxHistoryRecords) {
            return;
        }

        std::vector<WalletTransactionRecord>
            records;
        std::vector<WalletTransactionRecord>
            confirmed_records;

        records.reserve(
            static_cast<std::size_t>(
                *history_count)
        );
        confirmed_records.reserve(
            static_cast<std::size_t>(
                *history_count)
        );

        for (std::uint64_t i = 0U;
             i < *history_count;
             ++i) {
            WalletTransactionRecord record;

            if (!read_bytes(
                    body,
                    offset,
                    record.txid) ||
                offset >= body.size()) {
                return;
            }

            const Byte status =
                body[offset++];

            if (status >
                static_cast<Byte>(
                    WalletTransactionStatus::
                        inactive)) {
                return;
            }

            record.status =
                static_cast<
                    WalletTransactionStatus>(
                        status
                    );

            // The node's mempool is intentionally memory-only.
            // After restart a previously unconfirmed transaction
            // remains in history, but is inactive until observed
            // again in the current mempool.
            if (record.status ==
                WalletTransactionStatus::
                    unconfirmed) {
                record.status =
                    WalletTransactionStatus::
                        inactive;
            }

            const auto received =
                read_little_endian<Amount>(
                    body,
                    offset
                );
            const auto spent =
                read_little_endian<Amount>(
                    body,
                    offset
                );

            if (!received ||
                !spent ||
                !consensus::money_range(
                    *received) ||
                !consensus::money_range(
                    *spent) ||
                offset >= body.size()) {
                return;
            }

            record.received = *received;
            record.spent = *spent;

            const Byte has_fee =
                body[offset++];

            if (has_fee > 1U) {
                return;
            }

            if (has_fee != 0U) {
                const auto fee =
                    read_little_endian<Amount>(
                        body,
                        offset
                    );

                if (!fee ||
                    !consensus::money_range(
                        *fee)) {
                    return;
                }

                record.fee = *fee;
            }

            if (offset >= body.size()) {
                return;
            }

            const Byte coinbase =
                body[offset++];

            if (coinbase > 1U) {
                return;
            }

            record.coinbase =
                coinbase != 0U;

            if (record.status ==
                WalletTransactionStatus::
                    confirmed) {
                const auto block_height =
                    read_little_endian<
                        std::uint32_t>(
                            body,
                            offset
                        );

                Hash256 block_hash{};

                if (!block_height ||
                    *block_height > *height ||
                    !read_bytes(
                        body,
                        offset,
                        block_hash)) {
                    return;
                }

                record.block_height =
                    *block_height;
                record.block_hash =
                    block_hash;
            }

            const bool duplicate =
                std::any_of(
                    records.begin(),
                    records.end(),
                    [&](const auto& existing) {
                        return existing.txid ==
                               record.txid;
                    }
                );

            if (duplicate ||
                (record.received == 0U &&
                 record.spent == 0U)) {
                return;
            }

            if (record.status ==
                WalletTransactionStatus::
                    confirmed) {
                confirmed_records.push_back(
                    record
                );
            }

            records.push_back(
                std::move(record)
            );
        }

        if (offset != body.size()) {
            return;
        }

        indexed_height_ = *height;
        indexed_tip_ = tip;
        confirmed_coins_ =
            std::move(coins);
        confirmed_history_ =
            std::move(confirmed_records);
        history_ =
            std::move(records);
        index_valid_ = true;
    } catch (...) {
        reset_index_state();
    }
}

WalletStoreError Wallet::save_index_state() const
{
    if (!index_valid_ ||
        keys_.empty() ||
        confirmed_coins_.size() >
            kMaxIndexedCoins ||
        history_.size() >
            kMaxHistoryRecords) {
        return WalletStoreError::corrupt;
    }

    try {
        Bytes body;

        body.insert(
            body.end(),
            kStateMagic.begin(),
            kStateMagic.end()
        );

        append_little_endian(
            body,
            kStateVersion
        );

        body.push_back(
            static_cast<Byte>(
                params_.network)
        );

        body.insert(
            body.end(),
            params_.message_start.begin(),
            params_.message_start.end()
        );

        const auto id =
            wallet_index_id();

        append_hash(body, id);

        append_little_endian(
            body,
            indexed_height_
        );

        append_hash(
            body,
            indexed_tip_
        );

        append_compact_size(
            body,
            static_cast<std::uint64_t>(
                confirmed_coins_.size())
        );

        for (const auto& [outpoint, wallet_coin] :
             confirmed_coins_) {
            append_hash(
                body,
                outpoint.txid
            );

            append_little_endian(
                body,
                outpoint.index
            );

            append_little_endian(
                body,
                wallet_coin.coin.output.value
            );

            append_compact_size(
                body,
                static_cast<std::uint64_t>(
                    wallet_coin.coin.output
                        .locking_script.size())
            );

            body.insert(
                body.end(),
                wallet_coin.coin.output
                    .locking_script.begin(),
                wallet_coin.coin.output
                    .locking_script.end()
            );

            append_little_endian(
                body,
                wallet_coin.coin.height
            );

            body.push_back(
                wallet_coin.coin.coinbase
                    ? 1U
                    : 0U
            );

            body.insert(
                body.end(),
                wallet_coin.public_key.begin(),
                wallet_coin.public_key.end()
            );
        }

        append_compact_size(
            body,
            static_cast<std::uint64_t>(
                history_.size())
        );

        for (const auto& record :
             history_) {
            if (!consensus::money_range(
                    record.received) ||
                !consensus::money_range(
                    record.spent) ||
                (record.fee &&
                 !consensus::money_range(
                     *record.fee)) ||
                (record.received == 0U &&
                 record.spent == 0U)) {
                return WalletStoreError::corrupt;
            }

            append_hash(
                body,
                record.txid
            );

            body.push_back(
                static_cast<Byte>(
                    record.status)
            );

            append_little_endian(
                body,
                record.received
            );

            append_little_endian(
                body,
                record.spent
            );

            body.push_back(
                record.fee ? 1U : 0U
            );

            if (record.fee) {
                append_little_endian(
                    body,
                    *record.fee
                );
            }

            body.push_back(
                record.coinbase
                    ? 1U
                    : 0U
            );

            if (record.status ==
                WalletTransactionStatus::
                    confirmed) {
                if (!record.block_height ||
                    !record.block_hash ||
                    *record.block_height >
                        indexed_height_) {
                    return WalletStoreError::corrupt;
                }

                append_little_endian(
                    body,
                    *record.block_height
                );

                append_hash(
                    body,
                    *record.block_hash
                );
            } else if (record.block_height ||
                       record.block_hash) {
                return WalletStoreError::corrupt;
            }
        }

        const auto checksum =
            crypto::double_sha256(body);

        body.insert(
            body.end(),
            checksum.begin(),
            checksum.end()
        );

        return write_state_atomic(
            state_path_,
            body
        );
    } catch (...) {
        return WalletStoreError::io_error;
    }
}

bool Wallet::apply_confirmed_block_to_index(
    const Block& block,
    const Hash256& block_hash,
    std::uint32_t height,
    std::vector<crypto::PublicKey>& discovered_keys)
{
    for (const auto& tx :
         block.transactions) {
        Amount spent{0U};
        std::size_t owned_inputs{0U};

        if (!tx.is_coinbase()) {
            for (const auto& input :
                 tx.inputs) {
                const auto it =
                    confirmed_coins_.find(
                        input.previous_output
                    );

                if (it == confirmed_coins_.end()) {
                    continue;
                }

                if (!checked_add(
                        spent,
                        it->second.coin.output.value)) {
                    return false;
                }

                ++owned_inputs;
                confirmed_coins_.erase(it);
            }
        }

        const Hash256 txid =
            transaction_id(tx);

        Amount received{0U};

        for (std::size_t index = 0U;
             index < tx.outputs.size();
             ++index) {
            if (index >
                static_cast<std::size_t>(
                    std::numeric_limits<
                        std::uint32_t>::max())) {
                return false;
            }

            const auto public_key =
                consensus::
                    parse_p2pk_locking_script(
                        tx.outputs[index]
                            .locking_script
                    );

            if (!public_key ||
                !owns_public_key(
                    *public_key)) {
                continue;
            }

            if (!checked_add(
                    received,
                    tx.outputs[index].value)) {
                return false;
            }

            if (std::find(
                    discovered_keys.begin(),
                    discovered_keys.end(),
                    *public_key) ==
                discovered_keys.end()) {
                discovered_keys.push_back(
                    *public_key
                );
            }

            const OutPoint outpoint{
                .txid = txid,
                .index =
                    static_cast<
                        std::uint32_t>(
                            index),
            };

            const auto [it, inserted] =
                confirmed_coins_.emplace(
                    outpoint,
                    WalletCoin{
                        .outpoint = outpoint,
                        .coin = Coin{
                            .output =
                                tx.outputs[index],
                            .height = height,
                            .coinbase =
                                tx.is_coinbase(),
                        },
                        .public_key =
                            *public_key,
                    }
                );

            (void)it;

            if (!inserted) {
                return false;
            }
        }

        if (spent == 0U &&
            received == 0U) {
            continue;
        }

        WalletTransactionRecord record;
        record.txid = txid;
        record.status =
            WalletTransactionStatus::confirmed;
        record.received = received;
        record.spent = spent;
        record.coinbase =
            tx.is_coinbase();
        record.block_height = height;
        record.block_hash = block_hash;

        if (spent > 0U &&
            owned_inputs ==
                tx.inputs.size()) {
            Amount outputs{0U};

            for (const auto& output :
                 tx.outputs) {
                if (!checked_add(
                        outputs,
                        output.value)) {
                    return false;
                }
            }

            if (spent < outputs) {
                return false;
            }

            record.fee =
                spent - outputs;
        }

        const auto existing =
            std::find_if(
                confirmed_history_.begin(),
                confirmed_history_.end(),
                [&](const auto& item) {
                    return item.txid == txid;
                }
            );

        if (existing == confirmed_history_.end()) {
            confirmed_history_.push_back(
                std::move(record)
            );
        } else {
            *existing = std::move(record);
        }
    }

    return true;
}

} // namespace quintum::wallet
