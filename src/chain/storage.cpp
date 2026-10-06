#include "chain/storage.hpp"
#include "consensus/pow.hpp"

#include "core/serialize.hpp"
#include "crypto/sha256.hpp"
#include "primitives/block.hpp"
#include "primitives/transaction.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace quintum {
namespace {

constexpr std::array<Byte, 8> kStateMagic{
    'Q', 'T', 'M', 'S', 'T', 'A', 'T', 'E'
};
constexpr std::array<Byte, 4> kBlockMagic{
    'Q', 'B', 'L', 'K'
};
constexpr std::uint32_t kStorageVersionV1 = 1U;
constexpr std::uint32_t kStorageVersionV2 = 2U;
constexpr std::uint32_t kStorageVersion = 3U;
constexpr std::size_t kChecksumSize = 32U;
constexpr std::uint64_t kMaxCollectionEntries = 100'000'000ULL;

struct IndexMeta {
    Hash256 hash{};
    BlockHeader header{};
    Hash256 parent{};
    std::uint32_t height{0U};
    Hash256 chain_work{};
    bool failed{false};
    bool body_available{true};
};

struct ActiveMeta {
    Hash256 hash{};
    std::uint32_t height{0U};
    Hash256 chain_work{};
    BlockUndo undo{};
};

struct DiskState {
    std::uint32_t version{kStorageVersionV1};
    std::uint64_t block_generation{0U};
    std::vector<IndexMeta> index{};
    std::vector<ActiveMeta> active{};
    std::map<OutPoint, Coin, OutPointLess> utxos{};
};

struct BlockScanResult {
    StorageError error{StorageError::none};
    std::vector<Block> blocks{};
    std::uintmax_t committed_size{0U};
};

class Reader {
public:
    explicit Reader(std::span<const Byte> data)
        : data_(data)
    {
    }

    template <typename T>
    [[nodiscard]] std::optional<T> little()
    {
        return read_little_endian<T>(data_, offset_);
    }

    [[nodiscard]] std::optional<std::uint64_t> compact()
    {
        return read_compact_size(data_, offset_);
    }

    [[nodiscard]] bool byte(Byte& value)
    {
        if (offset_ >= data_.size()) {
            return false;
        }
        value = data_[offset_++];
        return true;
    }

    [[nodiscard]] bool hash(Hash256& value)
    {
        return raw(value.data(), value.size());
    }

    [[nodiscard]] bool raw(Byte* destination, std::size_t size)
    {
        if (offset_ > data_.size() ||
            size > data_.size() - offset_) {
            return false;
        }

        std::copy_n(
            data_.begin() + static_cast<std::ptrdiff_t>(offset_),
            static_cast<std::ptrdiff_t>(size),
            destination
        );
        offset_ += size;
        return true;
    }

    [[nodiscard]] bool bytes(Bytes& value, std::size_t size)
    {
        if (offset_ > data_.size() ||
            size > data_.size() - offset_) {
            return false;
        }

        value.assign(
            data_.begin() + static_cast<std::ptrdiff_t>(offset_),
            data_.begin() + static_cast<std::ptrdiff_t>(offset_ + size)
        );
        offset_ += size;
        return true;
    }

    [[nodiscard]] std::size_t remaining() const noexcept
    {
        return data_.size() - offset_;
    }

private:
    std::span<const Byte> data_;
    std::size_t offset_{0U};
};

void append_hash(Bytes& out, const Hash256& hash)
{
    out.insert(out.end(), hash.begin(), hash.end());
}

void append_literal(Bytes& out, std::span<const Byte> value)
{
    out.insert(out.end(), value.begin(), value.end());
}

bool count_is_reasonable(
    std::uint64_t count,
    std::size_t remaining,
    std::size_t minimum_bytes_per_item = 1U) noexcept
{
    if (count > kMaxCollectionEntries ||
        count > static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max())) {
        return false;
    }

    if (minimum_bytes_per_item == 0U) {
        return true;
    }

    const auto maximum_from_input =
        remaining / minimum_bytes_per_item + 1U;

    return count <= static_cast<std::uint64_t>(maximum_from_input);
}

Bytes serialize_block_bytes(const Block& block)
{
    Bytes out = serialize_block_header(block.header);
    append_compact_size(
        out,
        static_cast<std::uint64_t>(block.transactions.size())
    );

    for (const auto& tx : block.transactions) {
        const auto bytes = serialize_transaction(tx);
        out.insert(out.end(), bytes.begin(), bytes.end());
    }

    return out;
}

std::optional<Transaction> parse_transaction(
    Reader& reader,
    const consensus::ResourceLimits& limits)
{
    Transaction tx;

    const auto version = reader.little<std::uint32_t>();
    if (!version) {
        return std::nullopt;
    }
    tx.version = *version;

    const auto input_count = reader.compact();
    if (!input_count ||
        !count_is_reasonable(*input_count, reader.remaining(), 41U)) {
        return std::nullopt;
    }

    tx.inputs.reserve(static_cast<std::size_t>(*input_count));

    for (std::uint64_t i = 0U; i < *input_count; ++i) {
        TxInput input;

        if (!reader.hash(input.previous_output.txid)) {
            return std::nullopt;
        }

        const auto index = reader.little<std::uint32_t>();
        if (!index) {
            return std::nullopt;
        }
        input.previous_output.index = *index;

        const auto script_size = reader.compact();
        if (!script_size ||
            *script_size > limits.max_script_bytes ||
            *script_size >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::size_t>::max()) ||
            !reader.bytes(
                input.unlocking_script,
                static_cast<std::size_t>(*script_size))) {
            return std::nullopt;
        }

        const auto sequence = reader.little<std::uint32_t>();
        if (!sequence) {
            return std::nullopt;
        }
        input.sequence = *sequence;
        tx.inputs.push_back(std::move(input));
    }

    const auto output_count = reader.compact();
    if (!output_count ||
        !count_is_reasonable(*output_count, reader.remaining(), 9U)) {
        return std::nullopt;
    }

    tx.outputs.reserve(static_cast<std::size_t>(*output_count));

    for (std::uint64_t i = 0U; i < *output_count; ++i) {
        TxOutput output;

        const auto value = reader.little<Amount>();
        if (!value) {
            return std::nullopt;
        }
        output.value = *value;

        const auto script_size = reader.compact();
        if (!script_size ||
            *script_size > limits.max_script_bytes ||
            *script_size >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::size_t>::max()) ||
            !reader.bytes(
                output.locking_script,
                static_cast<std::size_t>(*script_size))) {
            return std::nullopt;
        }

        tx.outputs.push_back(std::move(output));
    }

    const auto lock_time = reader.little<std::uint32_t>();
    if (!lock_time) {
        return std::nullopt;
    }
    tx.lock_time = *lock_time;

    return tx;
}

std::optional<Block> parse_block_bytes(
    std::span<const Byte> data,
    const consensus::ResourceLimits& limits)
{
    if (data.size() >
        static_cast<std::size_t>(limits.max_block_serialized_bytes)) {
        return std::nullopt;
    }

    Reader reader{data};
    Block block;

    const auto version = reader.little<std::uint32_t>();
    if (!version ||
        !reader.hash(block.header.previous_block) ||
        !reader.hash(block.header.merkle_root)) {
        return std::nullopt;
    }
    block.header.version = *version;

    const auto timestamp = reader.little<std::uint64_t>();
    const auto bits = reader.little<std::uint32_t>();
    const auto nonce = reader.little<std::uint64_t>();

    if (!timestamp || !bits || !nonce) {
        return std::nullopt;
    }

    block.header.timestamp = *timestamp;
    block.header.bits = *bits;
    block.header.nonce = *nonce;

    const auto transaction_count = reader.compact();
    if (!transaction_count ||
        *transaction_count > limits.max_block_transactions ||
        !count_is_reasonable(
            *transaction_count,
            reader.remaining(),
            10U)) {
        return std::nullopt;
    }

    block.transactions.reserve(
        static_cast<std::size_t>(*transaction_count));

    for (std::uint64_t i = 0U;
         i < *transaction_count;
         ++i) {
        auto tx = parse_transaction(reader, limits);
        if (!tx) {
            return std::nullopt;
        }
        block.transactions.push_back(std::move(*tx));
    }

    if (reader.remaining() != 0U) {
        return std::nullopt;
    }

    const auto expected_size = serialized_block_size(block);
    if (!expected_size || *expected_size != data.size()) {
        return std::nullopt;
    }

    return block;
}

void append_outpoint(Bytes& out, const OutPoint& outpoint)
{
    append_hash(out, outpoint.txid);
    append_little_endian(out, outpoint.index);
}

bool read_outpoint(Reader& reader, OutPoint& outpoint)
{
    if (!reader.hash(outpoint.txid)) {
        return false;
    }

    const auto index = reader.little<std::uint32_t>();
    if (!index) {
        return false;
    }

    outpoint.index = *index;
    return true;
}

void append_coin(Bytes& out, const Coin& coin)
{
    append_little_endian(out, coin.output.value);
    append_little_endian(
        out,
        static_cast<std::uint64_t>(
            coin.output.locking_script.size())
    );
    append_literal(out, coin.output.locking_script);
    append_little_endian(out, coin.height);
    out.push_back(coin.coinbase ? 1U : 0U);
}

bool read_coin(
    Reader& reader,
    Coin& coin,
    const consensus::ResourceLimits& limits)
{
    const auto value = reader.little<Amount>();
    const auto script_size = reader.little<std::uint64_t>();

    if (!value || !script_size ||
        *script_size > limits.max_script_bytes ||
        *script_size >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::size_t>::max())) {
        return false;
    }

    coin.output.value = *value;

    if (!reader.bytes(
            coin.output.locking_script,
            static_cast<std::size_t>(*script_size))) {
        return false;
    }

    const auto height = reader.little<std::uint32_t>();
    Byte coinbase{0U};

    if (!height ||
        !reader.byte(coinbase) ||
        coinbase > 1U) {
        return false;
    }

    coin.height = *height;
    coin.coinbase = coinbase != 0U;
    return true;
}

void append_undo(Bytes& out, const BlockUndo& undo)
{
    append_little_endian(
        out,
        static_cast<std::uint64_t>(undo.transactions.size())
    );

    for (const auto& tx_undo : undo.transactions) {
        append_little_endian(
            out,
            static_cast<std::uint64_t>(tx_undo.spent.size())
        );

        for (const auto& [outpoint, coin] : tx_undo.spent) {
            append_outpoint(out, outpoint);
            append_coin(out, coin);
        }

        append_little_endian(
            out,
            static_cast<std::uint64_t>(tx_undo.created.size())
        );

        for (const auto& outpoint : tx_undo.created) {
            append_outpoint(out, outpoint);
        }
    }
}

bool read_undo(
    Reader& reader,
    BlockUndo& undo,
    const consensus::ResourceLimits& limits)
{
    const auto transaction_count =
        reader.little<std::uint64_t>();

    if (!transaction_count ||
        *transaction_count > limits.max_block_transactions ||
        !count_is_reasonable(
            *transaction_count,
            reader.remaining())) {
        return false;
    }

    undo.transactions.reserve(
        static_cast<std::size_t>(*transaction_count));

    for (std::uint64_t i = 0U;
         i < *transaction_count;
         ++i) {
        UtxoUndo tx_undo;

        const auto spent_count =
            reader.little<std::uint64_t>();

        if (!spent_count ||
            !count_is_reasonable(
                *spent_count,
                reader.remaining(),
                49U)) {
            return false;
        }

        tx_undo.spent.reserve(
            static_cast<std::size_t>(*spent_count));

        for (std::uint64_t j = 0U;
             j < *spent_count;
             ++j) {
            OutPoint outpoint;
            Coin coin;

            if (!read_outpoint(reader, outpoint) ||
                !read_coin(reader, coin, limits)) {
                return false;
            }

            tx_undo.spent.emplace_back(
                std::move(outpoint),
                std::move(coin)
            );
        }

        const auto created_count =
            reader.little<std::uint64_t>();

        if (!created_count ||
            !count_is_reasonable(
                *created_count,
                reader.remaining(),
                36U)) {
            return false;
        }

        tx_undo.created.reserve(
            static_cast<std::size_t>(*created_count));

        for (std::uint64_t j = 0U;
             j < *created_count;
             ++j) {
            OutPoint outpoint;
            if (!read_outpoint(reader, outpoint)) {
                return false;
            }
            tx_undo.created.push_back(std::move(outpoint));
        }

        undo.transactions.push_back(std::move(tx_undo));
    }

    return true;
}

bool equal_coin(const Coin& lhs, const Coin& rhs)
{
    return lhs.output.value == rhs.output.value &&
           lhs.output.locking_script ==
               rhs.output.locking_script &&
           lhs.height == rhs.height &&
           lhs.coinbase == rhs.coinbase;
}

bool equal_undo(
    const BlockUndo& lhs,
    const BlockUndo& rhs)
{
    if (lhs.transactions.size() !=
        rhs.transactions.size()) {
        return false;
    }

    for (std::size_t i = 0U;
         i < lhs.transactions.size();
         ++i) {
        const auto& a = lhs.transactions[i];
        const auto& b = rhs.transactions[i];

        if (a.created != b.created ||
            a.spent.size() != b.spent.size()) {
            return false;
        }

        for (std::size_t j = 0U;
             j < a.spent.size();
             ++j) {
            if (!(a.spent[j].first ==
                  b.spent[j].first) ||
                !equal_coin(
                    a.spent[j].second,
                    b.spent[j].second)) {
                return false;
            }
        }
    }

    return true;
}

bool equal_utxos(
    const std::map<OutPoint, Coin, OutPointLess>& lhs,
    const std::map<OutPoint, Coin, OutPointLess>& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    auto a = lhs.begin();
    auto b = rhs.begin();

    while (a != lhs.end()) {
        if (!(a->first == b->first) ||
            !equal_coin(a->second, b->second)) {
            return false;
        }
        ++a;
        ++b;
    }

    return true;
}

StorageError read_file_bytes(
    const std::filesystem::path& path,
    Bytes& out)
{
    std::error_code ec;
    const bool exists =
        std::filesystem::exists(path, ec);

    if (ec) {
        return StorageError::io_error;
    }
    if (!exists) {
        return StorageError::not_found;
    }

    const auto file_size =
        std::filesystem::file_size(path, ec);

    if (ec ||
        file_size >
            static_cast<std::uintmax_t>(
                std::numeric_limits<std::size_t>::max()) ||
        file_size >
            static_cast<std::uintmax_t>(
                std::numeric_limits<std::streamsize>::max())) {
        return StorageError::io_error;
    }

    out.resize(static_cast<std::size_t>(file_size));

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return StorageError::io_error;
    }

    if (!out.empty()) {
        input.read(
            reinterpret_cast<char*>(out.data()),
            static_cast<std::streamsize>(out.size())
        );
    }

    if (!input) {
        return StorageError::io_error;
    }

    return StorageError::none;
}

#ifdef _WIN32
std::FILE* open_binary_file(
    const std::filesystem::path& path,
    const wchar_t* mode)
{
    std::FILE* file{nullptr};
    if (_wfopen_s(&file, path.c_str(), mode) != 0) {
        return nullptr;
    }
    return file;
}
#else
std::FILE* open_binary_file(
    const std::filesystem::path& path,
    const char* mode)
{
    return std::fopen(path.c_str(), mode);
}
#endif

bool sync_file(std::FILE* file)
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

bool write_all(
    std::FILE* file,
    std::span<const Byte> data)
{
    if (data.empty()) {
        return true;
    }

    return std::fwrite(
               data.data(),
               1U,
               data.size(),
               file) == data.size();
}

StorageError write_file_synced(
    const std::filesystem::path& path,
    std::span<const Byte> data)
{
#ifdef _WIN32
    std::FILE* file =
        open_binary_file(path, L"wb");
#else
    std::FILE* file =
        open_binary_file(path, "wb");
#endif

    if (file == nullptr) {
        return StorageError::io_error;
    }

    const bool ok =
        write_all(file, data) &&
        sync_file(file);

    const int close_result = std::fclose(file);

    return ok && close_result == 0
        ? StorageError::none
        : StorageError::io_error;
}

StorageError append_file_synced(
    const std::filesystem::path& path,
    std::span<const Byte> data)
{
#ifdef _WIN32
    std::FILE* file =
        open_binary_file(path, L"ab");
#else
    std::FILE* file =
        open_binary_file(path, "ab");
#endif

    if (file == nullptr) {
        return StorageError::io_error;
    }

    const bool ok =
        write_all(file, data) &&
        sync_file(file);

    const int close_result = std::fclose(file);

    return ok && close_result == 0
        ? StorageError::none
        : StorageError::io_error;
}

bool atomic_replace(
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

    const auto parent = destination.parent_path();
    const int directory_fd = ::open(
        parent.empty() ? "." : parent.c_str(),
        O_RDONLY | O_DIRECTORY
    );

    if (directory_fd >= 0) {
        (void)::fsync(directory_fd);
        (void)::close(directory_fd);
    }

    return true;
#endif
}

StorageError parse_state_file(
    const std::filesystem::path& path,
    const consensus::ChainParams& params,
    DiskState& state)
{
    Bytes bytes;
    const auto read_error =
        read_file_bytes(path, bytes);

    if (read_error != StorageError::none) {
        return read_error;
    }

    constexpr std::size_t minimum =
        kStateMagic.size() +
        sizeof(std::uint32_t) +
        1U +
        4U +
        1U +
        32U +
        sizeof(std::uint64_t) +
        sizeof(std::uint64_t) +
        sizeof(std::uint64_t) +
        kChecksumSize;

    if (bytes.size() < minimum) {
        return StorageError::truncated;
    }

    const std::size_t payload_size =
        bytes.size() - kChecksumSize;

    const auto expected_checksum =
        crypto::double_sha256(
            std::span<const Byte>{
                bytes.data(),
                payload_size
            });

    Hash256 stored_checksum{};
    std::copy_n(
        bytes.begin() +
            static_cast<std::ptrdiff_t>(payload_size),
        static_cast<std::ptrdiff_t>(kChecksumSize),
        stored_checksum.begin()
    );

    if (expected_checksum != stored_checksum) {
        return StorageError::checksum_mismatch;
    }

    Reader reader{
        std::span<const Byte>{
            bytes.data(),
            payload_size
        }
    };

    std::array<Byte, 8> magic{};
    if (!reader.raw(magic.data(), magic.size()) ||
        magic != kStateMagic) {
        return StorageError::bad_format;
    }

    const auto version =
        reader.little<std::uint32_t>();

    if (!version) {
        return StorageError::truncated;
    }

    if (*version != kStorageVersionV1 &&
        *version != kStorageVersionV2 &&
        *version != kStorageVersion) {
        return StorageError::unsupported_version;
    }

    state.version = *version;

    Byte network{0U};
    std::array<Byte, 4> message_start{};
    Byte genesis_enforced{0U};
    Hash256 genesis_hash{};

    if (!reader.byte(network) ||
        !reader.raw(
            message_start.data(),
            message_start.size()) ||
        !reader.byte(genesis_enforced) ||
        genesis_enforced > 1U ||
        !reader.hash(genesis_hash)) {
        return StorageError::truncated;
    }

    if (network !=
            static_cast<Byte>(params.network) ||
        message_start != params.message_start ||
        (genesis_enforced != 0U) !=
            params.genesis.enforce ||
        genesis_hash != params.genesis.hash) {
        return StorageError::wrong_network;
    }

    if (*version >= kStorageVersion) {
        const auto generation =
            reader.little<std::uint64_t>();
        if (!generation) {
            return StorageError::truncated;
        }
        state.block_generation = *generation;
    }

    const auto block_count =
        reader.little<std::uint64_t>();

    if (!block_count ||
        !count_is_reasonable(
            *block_count,
            reader.remaining(),
            101U)) {
        return StorageError::bad_format;
    }

    state.index.reserve(
        static_cast<std::size_t>(*block_count));

    for (std::uint64_t i = 0U;
         i < *block_count;
         ++i) {
        IndexMeta meta;
        Byte failed{0U};

        if (!reader.hash(meta.hash) ||
            !reader.hash(meta.parent)) {
            return StorageError::truncated;
        }

        const auto height =
            reader.little<std::uint32_t>();

        if (!height ||
            !reader.hash(meta.chain_work) ||
            !reader.byte(failed) ||
            failed > 1U) {
            return StorageError::truncated;
        }

        meta.height = *height;
        meta.failed = failed != 0U;

        if (*version >= kStorageVersionV2) {
            const auto header_version =
                reader.little<std::uint32_t>();
            if (!header_version ||
                !reader.hash(meta.header.previous_block) ||
                !reader.hash(meta.header.merkle_root)) {
                return StorageError::truncated;
            }
            meta.header.version = *header_version;

            const auto timestamp =
                reader.little<std::uint64_t>();
            const auto bits =
                reader.little<std::uint32_t>();
            const auto nonce =
                reader.little<std::uint64_t>();

            if (!timestamp || !bits || !nonce) {
                return StorageError::truncated;
            }

            meta.header.timestamp = *timestamp;
            meta.header.bits = *bits;
            meta.header.nonce = *nonce;
        }

        if (*version >= kStorageVersion) {
            Byte body_available{0U};
            if (!reader.byte(body_available) ||
                body_available > 1U) {
                return StorageError::truncated;
            }
            meta.body_available =
                body_available != 0U;
        }

        state.index.push_back(std::move(meta));
    }

    const auto active_count =
        reader.little<std::uint64_t>();

    if (!active_count ||
        *active_count > *block_count ||
        !count_is_reasonable(
            *active_count,
            reader.remaining(),
            76U)) {
        return StorageError::bad_format;
    }

    state.active.reserve(
        static_cast<std::size_t>(*active_count));

    for (std::uint64_t i = 0U;
         i < *active_count;
         ++i) {
        ActiveMeta meta;

        if (!reader.hash(meta.hash)) {
            return StorageError::truncated;
        }

        const auto height =
            reader.little<std::uint32_t>();

        if (!height ||
            !reader.hash(meta.chain_work) ||
            !read_undo(
                reader,
                meta.undo,
                params.limits)) {
            return StorageError::bad_format;
        }

        meta.height = *height;
        state.active.push_back(std::move(meta));
    }

    const auto utxo_count =
        reader.little<std::uint64_t>();

    if (!utxo_count ||
        !count_is_reasonable(
            *utxo_count,
            reader.remaining(),
            49U)) {
        return StorageError::bad_format;
    }

    for (std::uint64_t i = 0U;
         i < *utxo_count;
         ++i) {
        OutPoint outpoint;
        Coin coin;

        if (!read_outpoint(reader, outpoint) ||
            !read_coin(
                reader,
                coin,
                params.limits)) {
            return StorageError::bad_format;
        }

        if (!state.utxos.emplace(
                std::move(outpoint),
                std::move(coin)).second) {
            return StorageError::bad_format;
        }
    }

    if (reader.remaining() != 0U) {
        return StorageError::bad_format;
    }

    return StorageError::none;
}

bool read_stream_exact(
    std::ifstream& input,
    Byte* destination,
    std::size_t size)
{
    if (size >
        static_cast<std::size_t>(
            std::numeric_limits<std::streamsize>::max())) {
        return false;
    }

    if (size == 0U) {
        return true;
    }

    input.read(
        reinterpret_cast<char*>(destination),
        static_cast<std::streamsize>(size)
    );

    return static_cast<bool>(input);
}

BlockScanResult scan_block_file(
    const std::filesystem::path& path,
    std::size_t count,
    const consensus::ChainParams& params)
{
    BlockScanResult out;
    out.blocks.reserve(count);

    std::error_code ec;
    const bool exists =
        std::filesystem::exists(path, ec);

    if (ec) {
        out.error = StorageError::io_error;
        return out;
    }

    if (!exists) {
        if (count == 0U) {
            return out;
        }
        out.error = StorageError::truncated;
        return out;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        out.error = StorageError::io_error;
        return out;
    }

    for (std::size_t i = 0U;
         i < count;
         ++i) {
        std::array<Byte, 12> header{};

        if (!read_stream_exact(
                input,
                header.data(),
                header.size())) {
            out.error = StorageError::truncated;
            return out;
        }

        if (!std::equal(
                kBlockMagic.begin(),
                kBlockMagic.end(),
                header.begin())) {
            out.error = StorageError::bad_format;
            return out;
        }

        std::size_t offset = kBlockMagic.size();
        const auto payload_size =
            read_little_endian<std::uint64_t>(
                header,
                offset);

        if (!payload_size ||
            *payload_size >
                params.limits.max_block_serialized_bytes ||
            *payload_size >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::size_t>::max())) {
            out.error = StorageError::bad_format;
            return out;
        }

        Bytes payload(
            static_cast<std::size_t>(*payload_size)
        );

        if (!read_stream_exact(
                input,
                payload.data(),
                payload.size())) {
            out.error = StorageError::truncated;
            return out;
        }

        Hash256 stored_checksum{};
        if (!read_stream_exact(
                input,
                stored_checksum.data(),
                stored_checksum.size())) {
            out.error = StorageError::truncated;
            return out;
        }

        Bytes checksum_input{
            header.begin(),
            header.end()
        };
        checksum_input.insert(
            checksum_input.end(),
            payload.begin(),
            payload.end()
        );

        if (crypto::double_sha256(
                checksum_input) !=
            stored_checksum) {
            out.error = StorageError::checksum_mismatch;
            return out;
        }

        auto block =
            parse_block_bytes(
                payload,
                params.limits);

        if (!block) {
            out.error = StorageError::bad_format;
            return out;
        }

        out.blocks.push_back(std::move(*block));

        const std::uintmax_t record_size =
            static_cast<std::uintmax_t>(
                header.size()) +
            static_cast<std::uintmax_t>(
                payload.size()) +
            static_cast<std::uintmax_t>(
                kChecksumSize);

        if (record_size >
            std::numeric_limits<std::uintmax_t>::max() -
                out.committed_size) {
            out.error = StorageError::bad_format;
            return out;
        }

        out.committed_size += record_size;
    }

    return out;
}

Bytes make_block_record(const Block& block)
{
    const auto payload =
        serialize_block_bytes(block);

    Bytes record;
    record.reserve(
        kBlockMagic.size() +
        sizeof(std::uint64_t) +
        payload.size() +
        kChecksumSize
    );

    append_literal(record, kBlockMagic);
    append_little_endian(
        record,
        static_cast<std::uint64_t>(payload.size())
    );
    record.insert(
        record.end(),
        payload.begin(),
        payload.end()
    );

    const auto checksum =
        crypto::double_sha256(record);

    append_hash(record, checksum);
    return record;
}

StorageError write_block_records_synced(
    const std::filesystem::path& path,
    const std::vector<const Block*>& blocks)
{
    Bytes bytes;
    for (const auto* block : blocks) {
        if (block == nullptr) {
            return StorageError::state_mismatch;
        }
        const auto record = make_block_record(*block);
        bytes.insert(
            bytes.end(),
            record.begin(),
            record.end());
    }
    return write_file_synced(path, bytes);
}

} // namespace

ChainstateStore::ChainstateStore(
    std::filesystem::path directory,
    const consensus::ChainParams& params,
    PrunePolicy prune_policy)
    : directory_(std::move(directory)),
      params_(params),
      prune_policy_(prune_policy)
{
}

const std::filesystem::path&
ChainstateStore::directory() const noexcept
{
    return directory_;
}

std::filesystem::path ChainstateStore::blocks_path() const
{
    return directory_ / "blocks.dat";
}

namespace {
std::filesystem::path generation_blocks_path(
    const std::filesystem::path& directory,
    std::uint64_t generation)
{
    if (generation == 0U) {
        return directory / "blocks.dat";
    }
    return directory /
        ("blocks." + std::to_string(generation) + ".dat");
}
} // namespace

std::filesystem::path ChainstateStore::state_path() const
{
    return directory_ / "chainstate.dat";
}

const PrunePolicy& ChainstateStore::prune_policy() const noexcept
{
    return prune_policy_;
}

PruneStatus ChainstateStore::prune_status(
    const Chainstate& chain) const noexcept
{
    PruneStatus out{
        .enabled = prune_policy_.enabled,
        .keep_recent_blocks = prune_policy_.keep_recent_blocks,
    };

    if (!prune_policy_.enabled ||
        prune_policy_.keep_recent_blocks == 0U) {
        return out;
    }

    const auto height = chain.height();
    if (!height ||
        *height < prune_policy_.keep_recent_blocks) {
        return out;
    }

    out.prune_height =
        *height - prune_policy_.keep_recent_blocks;
    return out;
}

void ChainstateStore::apply_pruning(
    Chainstate& chain) const noexcept
{
    const auto status = prune_status(chain);
    if (!status.prune_height) {
        return;
    }

    for (auto& [hash, entry] :
         chain.block_index_) {
        (void)hash;
        if (entry.height <= *status.prune_height) {
            entry.block.reset();
        }
    }
}

StorageError ChainstateStore::commit(
    const Chainstate& chain) const
{
    if (chain.params_.network != params_.network ||
        chain.params_.message_start !=
            params_.message_start ||
        chain.params_.genesis.enforce !=
            params_.genesis.enforce ||
        chain.params_.genesis.hash !=
            params_.genesis.hash) {
        return StorageError::wrong_network;
    }

    std::error_code ec;
    std::filesystem::create_directories(
        directory_,
        ec);

    if (ec) {
        return StorageError::io_error;
    }

    DiskState previous_state;
    bool have_previous_state{false};
    {
        const auto previous_error =
            parse_state_file(
                state_path(),
                params_,
                previous_state);
        if (previous_error == StorageError::none) {
            have_previous_state = true;
        } else if (previous_error != StorageError::not_found) {
            return previous_error;
        }
    }

    std::uint64_t next_generation{0U};

    if (prune_policy_.enabled) {
        if (have_previous_state &&
            previous_state.block_generation ==
                std::numeric_limits<std::uint64_t>::max()) {
            return StorageError::bad_format;
        }
        next_generation = have_previous_state
            ? previous_state.block_generation + 1U
            : 1U;

        const auto status = prune_status(chain);
        std::vector<const Block*> retained;
        retained.reserve(chain.acceptance_order_.size());

        for (const auto& hash : chain.acceptance_order_) {
            const auto it = chain.block_index_.find(hash);
            if (it == chain.block_index_.end()) {
                return StorageError::state_mismatch;
            }

            const bool keep =
                !status.prune_height ||
                it->second.height > *status.prune_height;

            if (keep) {
                if (!it->second.block) {
                    return StorageError::state_mismatch;
                }
                retained.push_back(&*it->second.block);
            }
        }

        auto temporary_blocks =
            generation_blocks_path(
                directory_,
                next_generation);
        temporary_blocks += ".tmp";
        const auto write_error =
            write_block_records_synced(
                temporary_blocks,
                retained);
        if (write_error != StorageError::none) {
            return write_error;
        }

        const auto generation_path =
            generation_blocks_path(
                directory_,
                next_generation);
        if (!atomic_replace(
                temporary_blocks,
                generation_path)) {
            return StorageError::io_error;
        }
    }

    if (!prune_policy_.enabled) {
        std::size_t committed_count{0U};
    
        {
            DiskState old_state;
            const auto old_error =
                parse_state_file(
                    state_path(),
                    params_,
                    old_state);
    
            if (old_error == StorageError::none) {
                committed_count =
                    old_state.index.size();
            } else if (
                old_error != StorageError::not_found) {
                return old_error;
            }
        }
    
        if (committed_count >
            chain.acceptance_order_.size()) {
            return StorageError::state_mismatch;
        }
    
        auto scan =
            scan_block_file(
                blocks_path(),
                committed_count,
                params_);
    
        if (scan.error != StorageError::none) {
            return scan.error;
        }
    
        for (std::size_t i = 0U;
             i < committed_count;
             ++i) {
            const auto& expected_hash =
                chain.acceptance_order_[i];
    
            const auto index_it =
                chain.block_index_.find(expected_hash);
    
            if (index_it ==
                    chain.block_index_.end() ||
                block_hash(
                    scan.blocks[i].header) !=
                    expected_hash ||
                !index_it->second.block ||
                serialize_block_bytes(
                    scan.blocks[i]) !=
                    serialize_block_bytes(
                        *index_it->second.block)) {
                return StorageError::state_mismatch;
            }
        }
    
        {
            const auto path = blocks_path();
            const bool exists =
                std::filesystem::exists(path, ec);
    
            if (ec) {
                return StorageError::io_error;
            }
    
            if (exists) {
                const auto current_size =
                    std::filesystem::file_size(path, ec);
    
                if (ec ||
                    current_size <
                        scan.committed_size) {
                    return StorageError::truncated;
                }
    
                if (current_size !=
                    scan.committed_size) {
                    std::filesystem::resize_file(
                        path,
                        scan.committed_size,
                        ec);
    
                    if (ec) {
                        return StorageError::io_error;
                    }
                }
            }
        }
    
        for (std::size_t i = committed_count;
             i < chain.acceptance_order_.size();
             ++i) {
            const auto index_it =
                chain.block_index_.find(
                    chain.acceptance_order_[i]);
    
            if (index_it ==
                    chain.block_index_.end() ||
                !index_it->second.block) {
                return StorageError::state_mismatch;
            }
    
            const auto record =
                make_block_record(
                    *index_it->second.block);
    
            const auto append_error =
                append_file_synced(
                    blocks_path(),
                    record);
    
            if (append_error !=
                StorageError::none) {
                return append_error;
            }
        }
    
    }

    Bytes state;
    state.reserve(1024U);

    append_literal(state, kStateMagic);
    append_little_endian(
        state,
        kStorageVersion);
    state.push_back(
        static_cast<Byte>(params_.network));
    append_literal(
        state,
        params_.message_start);
    state.push_back(
        params_.genesis.enforce ? 1U : 0U);
    append_hash(
        state,
        params_.genesis.hash);
    // The snapshot names the exact block-store generation it was built
    // against. Generation 0 is the legacy blocks.dat layout.
    append_little_endian(
        state,
        next_generation);

    append_little_endian(
        state,
        static_cast<std::uint64_t>(
            chain.acceptance_order_.size())
    );

    for (const auto& hash :
         chain.acceptance_order_) {
        const auto it =
            chain.block_index_.find(hash);

        if (it == chain.block_index_.end()) {
            return StorageError::state_mismatch;
        }

        append_hash(state, it->second.hash);
        append_hash(state, it->second.parent);
        append_little_endian(
            state,
            it->second.height);
        append_hash(
            state,
            it->second.chain_work);
        state.push_back(
            it->second.failed ? 1U : 0U);
        const auto header_bytes =
            serialize_block_header(it->second.header);
        state.insert(
            state.end(),
            header_bytes.begin(),
            header_bytes.end());
        bool body_available =
            it->second.block.has_value();
        if (prune_policy_.enabled) {
            const auto status = prune_status(chain);
            body_available =
                body_available &&
                (!status.prune_height ||
                 it->second.height >
                     *status.prune_height);
        }
        state.push_back(
            body_available ? 1U : 0U);
    }

    append_little_endian(
        state,
        static_cast<std::uint64_t>(
            chain.chain_.size())
    );

    for (const auto& entry :
         chain.chain_) {
        append_hash(state, entry.hash);
        append_little_endian(
            state,
            entry.height);
        append_hash(
            state,
            entry.chain_work);
        append_undo(state, entry.undo);
    }

    append_little_endian(
        state,
        static_cast<std::uint64_t>(
            chain.utxos_.coins_.size())
    );

    for (const auto& [outpoint, coin] :
         chain.utxos_.coins_) {
        append_outpoint(state, outpoint);
        append_coin(state, coin);
    }

    const auto checksum =
        crypto::double_sha256(state);
    append_hash(state, checksum);

    auto temporary_path = state_path();
    temporary_path += ".tmp";

    const auto write_error =
        write_file_synced(
            temporary_path,
            state);

    if (write_error != StorageError::none) {
        return write_error;
    }

    if (!atomic_replace(
            temporary_path,
            state_path())) {
        return StorageError::io_error;
    }

    // The new snapshot is now authoritative. Only after this durable
    // switch is it safe to remove the generation referenced by the
    // previous snapshot. Cleanup failure is non-fatal: keeping an
    // obsolete generation wastes disk space but cannot corrupt state.
    if (prune_policy_.enabled &&
        have_previous_state &&
        previous_state.block_generation != next_generation) {
        const auto obsolete_path =
            generation_blocks_path(
                directory_,
                previous_state.block_generation);
        std::error_code cleanup_ec;
        (void)std::filesystem::remove(
            obsolete_path,
            cleanup_ec);
    }

    return StorageError::none;
}

StorageError ChainstateStore::load(
    Chainstate& chain) const
{
    DiskState disk;
    const auto state_error =
        parse_state_file(
            state_path(),
            params_,
            disk);

    if (state_error != StorageError::none) {
        return state_error;
    }

    if (disk.version >= kStorageVersion &&
        disk.block_generation != 0U) {
        const auto body_count =
            static_cast<std::size_t>(
                std::count_if(
                    disk.index.begin(),
                    disk.index.end(),
                    [](const IndexMeta& meta) {
                        return meta.body_available;
                    }));

        auto scan = scan_block_file(
            generation_blocks_path(
                directory_,
                disk.block_generation),
            body_count,
            params_);
        if (scan.error != StorageError::none ||
            scan.blocks.size() != body_count) {
            return scan.error != StorageError::none
                ? scan.error
                : StorageError::truncated;
        }

        Chainstate restored{params_};
        std::size_t body_index{0U};

        for (const auto& meta : disk.index) {
            // Pruned entries no longer have a block body to cross-check.
            // Their persisted header must therefore authenticate the
            // stored index hash on its own.
            if (block_hash(meta.header) != meta.hash) {
                return StorageError::state_mismatch;
            }

            std::optional<Block> body;
            if (meta.body_available) {
                if (body_index >= scan.blocks.size()) {
                    return StorageError::truncated;
                }
                const auto& candidate =
                    scan.blocks[body_index++];
                if (block_hash(candidate.header) !=
                        meta.hash ||
                    serialize_block_header(
                        candidate.header) !=
                    serialize_block_header(
                        meta.header)) {
                    return StorageError::state_mismatch;
                }
                body = candidate;
            }

            Hash256 expected_chain_work{};
            if (meta.height == 0U) {
                if (meta.parent != Hash256{}) {
                    return StorageError::state_mismatch;
                }
            } else {
                const auto parent_it =
                    restored.block_index_.find(meta.parent);
                if (parent_it == restored.block_index_.end() ||
                    parent_it->second.height + 1U != meta.height) {
                    return StorageError::state_mismatch;
                }
                expected_chain_work =
                    parent_it->second.chain_work;
            }

            const BlockIndexEntry* parent_ptr = nullptr;
            if (meta.height != 0U) {
                const auto parent_it =
                    restored.block_index_.find(meta.parent);
                if (parent_it == restored.block_index_.end()) {
                    return StorageError::state_mismatch;
                }
                parent_ptr = &parent_it->second;
            }

            Block header_candidate{};
            header_candidate.header = meta.header;
            const auto required_bits =
                restored.expected_bits(
                    header_candidate,
                    parent_ptr);
            if (!required_bits ||
                *required_bits != meta.header.bits) {
                return StorageError::state_mismatch;
            }

            const auto pow_error =
                params_.pow.pow_algorithm ==
                        consensus::PowAlgorithm::randomx_v2
                    ? [&]() {
                          const auto seed =
                              restored.randomx_seed_key_for(
                                  parent_ptr,
                                  meta.height);
                          return seed
                              ? consensus::check_randomx_proof_of_work(
                                    meta.header,
                                    params_.pow,
                                    *seed)
                              : consensus::PowCheckError::hashing_failed;
                      }()
                    : consensus::check_proof_of_work(
                          meta.header,
                          params_.pow);
            if (pow_error != consensus::PowCheckError::none) {
                return StorageError::state_mismatch;
            }

            const auto compact =
                consensus::decode_compact_target(
                    meta.header.bits);
            if (!compact.valid() ||
                consensus::encode_compact_target(
                    compact.target) != meta.header.bits ||
                !consensus::add_chain_work(
                    expected_chain_work,
                    consensus::work_for_target(
                        compact.target)) ||
                expected_chain_work != meta.chain_work) {
                return StorageError::state_mismatch;
            }

            const auto [inserted_it, inserted] =
                restored.block_index_.emplace(
                    meta.hash,
                    BlockIndexEntry{
                        .block = std::move(body),
                        .header = meta.header,
                        .hash = meta.hash,
                        .parent = meta.parent,
                        .height = meta.height,
                        .chain_work = meta.chain_work,
                        .failed = meta.failed,
                    });
            (void)inserted_it;
            if (!inserted) {
                return StorageError::state_mismatch;
            }

            restored.acceptance_order_.push_back(
                meta.hash);
        }

        if (body_index != scan.blocks.size()) {
            return StorageError::state_mismatch;
        }

        restored.chain_.reserve(disk.active.size());
        for (std::size_t active_index = 0U;
             active_index < disk.active.size();
             ++active_index) {
            const auto& stored =
                disk.active[active_index];
            const auto index_it =
                restored.block_index_.find(
                    stored.hash);
            if (index_it ==
                    restored.block_index_.end() ||
                index_it->second.failed ||
                index_it->second.height !=
                    stored.height ||
                index_it->second.chain_work !=
                    stored.chain_work ||
                stored.height != active_index ||
                (active_index == 0U
                     ? index_it->second.parent != Hash256{}
                     : index_it->second.parent !=
                           disk.active[
                               active_index - 1U].hash)) {
                return StorageError::state_mismatch;
            }
            restored.chain_.push_back(
                ChainEntry{
                    .hash = stored.hash,
                    .header =
                        index_it->second.header,
                    .height = stored.height,
                    .chain_work =
                        stored.chain_work,
                    .undo = stored.undo,
                });
        }

        restored.utxos_.coins_ = disk.utxos;
        chain = std::move(restored);
        return StorageError::none;
    }

    auto scan =
        scan_block_file(
            blocks_path(),
            disk.index.size(),
            params_);

    if (scan.error != StorageError::none) {
        return scan.error;
    }

    if (scan.blocks.size() !=
        disk.index.size()) {
        return StorageError::truncated;
    }

    Chainstate replay{params_};

    for (std::size_t i = 0U;
         i < scan.blocks.size();
         ++i) {
        const auto& block = scan.blocks[i];
        const auto hash =
            block_hash(block.header);

        if (hash != disk.index[i].hash) {
            return StorageError::state_mismatch;
        }

        // v1 snapshots did not persist headers. The verified block log is
        // the authoritative migration source. v2 must match exactly.
        const auto block_header_bytes =
            serialize_block_header(block.header);

        if (disk.version == kStorageVersionV1) {
            disk.index[i].header = block.header;
        } else if (serialize_block_header(
                       disk.index[i].header) !=
                   block_header_bytes) {
            return StorageError::state_mismatch;
        }

        const auto result =
            replay.connect_block(
                block,
                std::numeric_limits<
                    std::uint64_t>::max());

        if (!result.ok()) {
            return StorageError::
                consensus_replay_failed;
        }

        const auto it =
            replay.block_index_.find(hash);

        if (it ==
                replay.block_index_.end() ||
            it->second.parent !=
                disk.index[i].parent ||
            it->second.height !=
                disk.index[i].height ||
            it->second.chain_work !=
                disk.index[i].chain_work ||
            serialize_block_header(
                it->second.header) !=
                serialize_block_header(
                    disk.index[i].header)) {
            return StorageError::state_mismatch;
        }
    }

    if (replay.acceptance_order_.size() !=
        disk.index.size()) {
        return StorageError::state_mismatch;
    }

    for (std::size_t i = 0U;
         i < disk.index.size();
         ++i) {
        if (replay.acceptance_order_[i] !=
            disk.index[i].hash) {
            return StorageError::state_mismatch;
        }
    }

    Chainstate active{params_};

    for (const auto& stored :
         disk.active) {
        const auto block_it =
            replay.block_index_.find(
                stored.hash);

        if (block_it ==
            replay.block_index_.end()) {
            return StorageError::state_mismatch;
        }

        if (!block_it->second.block) {
            return StorageError::state_mismatch;
        }

        const auto result =
            active.connect_block(
                *block_it->second.block,
                std::numeric_limits<
                    std::uint64_t>::max());

        if (!result.ok() ||
            !result.activated ||
            active.chain_.empty()) {
            return StorageError::
                consensus_replay_failed;
        }

        const auto& reconstructed =
            active.chain_.back();

        if (reconstructed.hash !=
                stored.hash ||
            reconstructed.height !=
                stored.height ||
            reconstructed.chain_work !=
                stored.chain_work ||
            !equal_undo(
                reconstructed.undo,
                stored.undo)) {
            return StorageError::state_mismatch;
        }
    }

    if (!equal_utxos(
            active.utxos_.coins_,
            disk.utxos)) {
        return StorageError::state_mismatch;
    }

    for (const auto& stored :
         disk.active) {
        const auto it =
            std::find_if(
                disk.index.begin(),
                disk.index.end(),
                [&stored](
                    const IndexMeta& meta) {
                    return meta.hash ==
                        stored.hash;
                });

        if (it == disk.index.end() ||
            it->failed) {
            return StorageError::state_mismatch;
        }
    }

    for (const auto& meta :
         disk.index) {
        auto it =
            replay.block_index_.find(
                meta.hash);

        if (it ==
            replay.block_index_.end()) {
            return StorageError::state_mismatch;
        }

        it->second.failed = meta.failed;
    }

    replay.chain_ =
        std::move(active.chain_);
    replay.utxos_ =
        std::move(active.utxos_);

    chain = std::move(replay);
    return StorageError::none;
}

PersistentChainstate::PersistentChainstate(
    const consensus::ChainParams& params,
    std::filesystem::path directory,
    PrunePolicy prune_policy)
    : chain_(params),
      store_(
          std::move(directory),
          params,
          prune_policy)
{
}

StorageError PersistentChainstate::load()
{
    Chainstate staged{chain_.params()};
    const auto error =
        store_.load(staged);

    if (error == StorageError::none) {
        chain_ = std::move(staged);
    }

    return error;
}

const Chainstate&
PersistentChainstate::chain() const noexcept
{
    return chain_;
}

const ChainstateStore&
PersistentChainstate::store() const noexcept
{
    return store_;
}

PersistentConnectResult
PersistentChainstate::connect_block(
    const Block& block)
{
    Chainstate staged = chain_;
    auto result =
        staged.connect_block(block);

    if (!result.ok()) {
        return PersistentConnectResult{
            .chain = result,
        };
    }

    const auto storage_error =
        store_.commit(staged);

    if (storage_error ==
        StorageError::none) {
        store_.apply_pruning(staged);
        chain_ = std::move(staged);
    }

    return PersistentConnectResult{
        .chain = result,
        .storage_error = storage_error,
    };
}

PersistentConnectResult
PersistentChainstate::connect_block(
    const Block& block,
    std::uint64_t adjusted_time)
{
    Chainstate staged = chain_;
    auto result =
        staged.connect_block(
            block,
            adjusted_time);

    if (!result.ok()) {
        return PersistentConnectResult{
            .chain = result,
        };
    }

    const auto storage_error =
        store_.commit(staged);

    if (storage_error ==
        StorageError::none) {
        store_.apply_pruning(staged);
        chain_ = std::move(staged);
    }

    return PersistentConnectResult{
        .chain = result,
        .storage_error = storage_error,
    };
}

PersistentDisconnectResult
PersistentChainstate::disconnect_tip()
{
    Chainstate staged = chain_;
    const auto result =
        staged.disconnect_tip();

    if (result !=
        ChainDisconnectError::none) {
        return PersistentDisconnectResult{
            .chain = result,
        };
    }

    const auto storage_error =
        store_.commit(staged);

    if (storage_error ==
        StorageError::none) {
        store_.apply_pruning(staged);
        chain_ = std::move(staged);
    }

    return PersistentDisconnectResult{
        .chain = result,
        .storage_error = storage_error,
    };
}

} // namespace quintum
