#include "net/compact_block.hpp"

#include "core/serialize.hpp"
#include "crypto/sha256.hpp"
#include "net/relay.hpp"
#include "primitives/transaction.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace quintum::net {
namespace {

constexpr std::size_t kHeaderSize = 88U;
constexpr std::uint64_t kShortIdMask =
    0x0000ffffffffffffULL;

class Reader {
public:
    explicit Reader(std::span<const Byte> data)
        : data_(data)
    {
    }

    template <typename T>
    [[nodiscard]] std::optional<T> little()
    {
        return read_little_endian<T>(
            data_,
            offset_
        );
    }

    [[nodiscard]] std::optional<std::uint64_t>
    compact()
    {
        return read_compact_size(
            data_,
            offset_
        );
    }

    [[nodiscard]] bool raw(
        Byte* destination,
        std::size_t size)
    {
        if (size > remaining()) {
            return false;
        }

        std::copy_n(
            data_.begin() +
                static_cast<std::ptrdiff_t>(
                    offset_),
            static_cast<std::ptrdiff_t>(
                size),
            destination
        );

        offset_ += size;
        return true;
    }

    [[nodiscard]] bool hash(Hash256& hash)
    {
        return raw(
            hash.data(),
            hash.size()
        );
    }

    [[nodiscard]] bool bytes(
        Bytes& out,
        std::size_t size)
    {
        if (size > remaining()) {
            return false;
        }

        out.assign(
            data_.begin() +
                static_cast<std::ptrdiff_t>(
                    offset_),
            data_.begin() +
                static_cast<std::ptrdiff_t>(
                    offset_ + size)
        );

        offset_ += size;
        return true;
    }

    [[nodiscard]] std::size_t remaining()
        const noexcept
    {
        return data_.size() - offset_;
    }

private:
    std::span<const Byte> data_{};
    std::size_t offset_{0U};
};

[[nodiscard]] std::uint64_t load64_le(
    const Byte* data) noexcept
{
    std::uint64_t value{0U};

    for (std::size_t i = 0U;
         i < 8U;
         ++i) {
        value |=
            static_cast<std::uint64_t>(
                data[i]
            ) << (8U * i);
    }

    return value;
}

[[nodiscard]] std::uint64_t rotl(
    std::uint64_t value,
    unsigned bits) noexcept
{
    return (value << bits) |
           (value >> (64U - bits));
}

void sip_round(
    std::uint64_t& v0,
    std::uint64_t& v1,
    std::uint64_t& v2,
    std::uint64_t& v3) noexcept
{
    v0 += v1;
    v1 = rotl(v1, 13U);
    v1 ^= v0;
    v0 = rotl(v0, 32U);

    v2 += v3;
    v3 = rotl(v3, 16U);
    v3 ^= v2;

    v0 += v3;
    v3 = rotl(v3, 21U);
    v3 ^= v0;

    v2 += v1;
    v1 = rotl(v1, 17U);
    v1 ^= v2;
    v2 = rotl(v2, 32U);
}

[[nodiscard]] std::uint64_t siphash24(
    std::uint64_t key0,
    std::uint64_t key1,
    std::span<const Byte> message) noexcept
{
    std::uint64_t v0 =
        0x736f6d6570736575ULL ^ key0;
    std::uint64_t v1 =
        0x646f72616e646f6dULL ^ key1;
    std::uint64_t v2 =
        0x6c7967656e657261ULL ^ key0;
    std::uint64_t v3 =
        0x7465646279746573ULL ^ key1;

    std::size_t offset{0U};

    while (message.size() - offset >= 8U) {
        const std::uint64_t word =
            load64_le(
                message.data() + offset
            );

        v3 ^= word;
        sip_round(v0, v1, v2, v3);
        sip_round(v0, v1, v2, v3);
        v0 ^= word;
        offset += 8U;
    }

    std::uint64_t tail =
        static_cast<std::uint64_t>(
            message.size()
        ) << 56U;

    for (std::size_t i = 0U;
         offset + i < message.size();
         ++i) {
        tail |=
            static_cast<std::uint64_t>(
                message[offset + i]
            ) << (8U * i);
    }

    v3 ^= tail;
    sip_round(v0, v1, v2, v3);
    sip_round(v0, v1, v2, v3);
    v0 ^= tail;
    v2 ^= 0xffU;

    for (int i = 0; i < 4; ++i) {
        sip_round(v0, v1, v2, v3);
    }

    return v0 ^ v1 ^ v2 ^ v3;
}

[[nodiscard]] std::pair<
    std::uint64_t,
    std::uint64_t>
compact_keys(
    const BlockHeader& header,
    std::uint64_t nonce) noexcept
{
    Bytes material =
        serialize_block_header(header);

    append_little_endian(
        material,
        nonce
    );

    const Hash256 digest =
        crypto::double_sha256(material);

    return {
        load64_le(digest.data()),
        load64_le(digest.data() + 8U),
    };
}

void append_short_id(
    Bytes& out,
    std::uint64_t short_id)
{
    for (std::size_t i = 0U;
         i < kCompactShortIdBytes;
         ++i) {
        out.push_back(
            static_cast<Byte>(
                (short_id >> (8U * i)) &
                0xffU
            )
        );
    }
}

[[nodiscard]] bool read_short_id(
    Reader& reader,
    std::uint64_t& short_id)
{
    std::array<Byte, kCompactShortIdBytes>
        raw{};

    if (!reader.raw(
            raw.data(),
            raw.size())) {
        return false;
    }

    short_id = 0U;

    for (std::size_t i = 0U;
         i < raw.size();
         ++i) {
        short_id |=
            static_cast<std::uint64_t>(
                raw[i]
            ) << (8U * i);
    }

    return true;
}

[[nodiscard]] std::optional<BlockHeader>
read_header(Reader& reader)
{
    BlockHeader header;

    const auto version =
        reader.little<std::uint32_t>();

    if (!version ||
        !reader.hash(
            header.previous_block) ||
        !reader.hash(
            header.merkle_root)) {
        return std::nullopt;
    }

    const auto timestamp =
        reader.little<std::uint64_t>();
    const auto bits =
        reader.little<std::uint32_t>();
    const auto nonce =
        reader.little<std::uint64_t>();

    if (!timestamp ||
        !bits ||
        !nonce) {
        return std::nullopt;
    }

    header.version = *version;
    header.timestamp = *timestamp;
    header.bits = *bits;
    header.nonce = *nonce;
    return header;
}

void append_transaction(
    Bytes& out,
    const Transaction& transaction)
{
    const Bytes bytes =
        serialize_transaction(transaction);

    append_compact_size(
        out,
        static_cast<std::uint64_t>(
            bytes.size()
        )
    );

    out.insert(
        out.end(),
        bytes.begin(),
        bytes.end()
    );
}

[[nodiscard]] std::optional<Transaction>
read_transaction(
    Reader& reader,
    const consensus::ResourceLimits& limits)
{
    const auto size = reader.compact();

    if (!size ||
        *size == 0U ||
        *size >
            limits.max_block_serialized_bytes ||
        *size >
            static_cast<std::uint64_t>(
                std::numeric_limits<
                    std::size_t>::max())) {
        return std::nullopt;
    }

    Bytes bytes;

    if (!reader.bytes(
            bytes,
            static_cast<std::size_t>(*size))) {
        return std::nullopt;
    }

    return parse_transaction_payload(
        bytes,
        limits
    );
}

[[nodiscard]] bool valid_prefilled(
    const CompactBlock& compact)
{
    const std::size_t total =
        compact.short_ids.size() +
        compact.prefilled.size();

    if (total == 0U ||
        compact.prefilled.empty() ||
        compact.prefilled.front().index != 0U ||
        compact.prefilled.size() >
            kMaxCompactPrefilledTransactions) {
        return false;
    }

    std::uint32_t previous{0U};
    bool first{true};

    for (const auto& entry :
         compact.prefilled) {
        if (entry.index >= total) {
            return false;
        }

        if (!first &&
            entry.index <= previous) {
            return false;
        }

        previous = entry.index;
        first = false;
    }

    return true;
}

struct SlotBuildResult {
    CompactBlockError error{
        CompactBlockError::none
    };
    std::vector<std::optional<Transaction>>
        slots{};
    std::vector<std::uint32_t> missing{};
};

[[nodiscard]] SlotBuildResult build_slots(
    const CompactBlock& compact,
    const Mempool& mempool,
    const BlockTransactions* response)
{
    SlotBuildResult out;

    if (!valid_prefilled(compact)) {
        out.error =
            CompactBlockError::malformed;
        return out;
    }

    const std::size_t total =
        compact.short_ids.size() +
        compact.prefilled.size();

    out.slots.resize(total);

    for (const auto& entry :
         compact.prefilled) {
        out.slots[entry.index] =
            entry.transaction;
    }

    const auto [key0, key1] =
        compact_keys(
            compact.header,
            compact.nonce
        );

    std::unordered_map<
        std::uint64_t,
        const Transaction*> candidates;
    std::unordered_set<std::uint64_t>
        collisions;

    candidates.reserve(
        mempool.size()
    );

    for (const auto& entry :
         mempool.entries()) {
        const std::uint64_t id =
            siphash24(
                key0,
                key1,
                entry.txid
            ) & kShortIdMask;

        const auto [it, inserted] =
            candidates.emplace(
                id,
                &entry.transaction
            );

        if (!inserted &&
            transaction_id(
                *it->second) !=
                entry.txid) {
            collisions.insert(id);
        }
    }

    std::unordered_map<
        std::uint32_t,
        const Transaction*> supplied;

    if (response != nullptr) {
        supplied.reserve(
            response->transactions.size()
        );

        for (const auto& [index, tx] :
             response->transactions) {
            const auto [it, inserted] =
                supplied.emplace(
                    index,
                    &tx
                );

            if (!inserted) {
                out.error =
                    CompactBlockError::
                        response_mismatch;
                return out;
            }
        }
    }

    std::size_t short_offset{0U};

    for (std::size_t index = 0U;
         index < total;
         ++index) {
        if (out.slots[index]) {
            continue;
        }

        if (short_offset >=
            compact.short_ids.size()) {
            out.error =
                CompactBlockError::malformed;
            return out;
        }

        const std::uint64_t short_id =
            compact.short_ids[
                short_offset++
            ];

        const auto supplied_it =
            supplied.find(
                static_cast<std::uint32_t>(
                    index)
            );

        if (supplied_it !=
            supplied.end()) {
            const Hash256 supplied_txid =
                transaction_id(
                    *supplied_it->second
                );

            if ((siphash24(
                    key0,
                    key1,
                    supplied_txid) &
                 kShortIdMask) !=
                short_id) {
                out.error =
                    CompactBlockError::
                        response_mismatch;
                return out;
            }

            out.slots[index] =
                *supplied_it->second;
            continue;
        }

        const auto found =
            candidates.find(short_id);

        if (found != candidates.end() &&
            !collisions.contains(
                short_id)) {
            out.slots[index] =
                *found->second;
            continue;
        }

        out.missing.push_back(
            static_cast<std::uint32_t>(
                index)
        );
    }

    if (short_offset !=
        compact.short_ids.size()) {
        out.error =
            CompactBlockError::malformed;
    }

    return out;
}

[[nodiscard]] CompactReconstructionResult
finish_slots(
    const CompactBlock& compact,
    SlotBuildResult slots)
{
    CompactReconstructionResult out;
    out.error = slots.error;
    out.missing_indexes =
        std::move(slots.missing);

    if (out.error !=
        CompactBlockError::none ||
        !out.missing_indexes.empty()) {
        return out;
    }

    Block block;
    block.header = compact.header;
    block.transactions.reserve(
        slots.slots.size()
    );

    for (auto& slot : slots.slots) {
        if (!slot) {
            out.error =
                CompactBlockError::malformed;
            return out;
        }

        block.transactions.push_back(
            std::move(*slot)
        );
    }

    const auto merkle =
        compute_merkle_root(
            block.transactions
        );

    if (merkle.mutated ||
        merkle.root !=
            block.header.merkle_root) {
        out.error =
            CompactBlockError::
                merkle_mismatch;
        return out;
    }

    out.block = std::move(block);
    return out;
}

} // namespace

std::uint64_t compact_short_id(
    const BlockHeader& header,
    std::uint64_t nonce,
    const Hash256& txid) noexcept
{
    const auto [key0, key1] =
        compact_keys(
            header,
            nonce
        );

    return siphash24(
        key0,
        key1,
        txid
    ) & kShortIdMask;
}

Bytes serialize_compact_block(
    const Block& block,
    std::uint64_t nonce)
{
    if (block.transactions.empty()) {
        return {};
    }

    Bytes out =
        serialize_block_header(block.header);

    append_little_endian(
        out,
        nonce
    );

    append_compact_size(
        out,
        static_cast<std::uint64_t>(
            block.transactions.size() - 1U)
    );

    for (std::size_t i = 1U;
         i < block.transactions.size();
         ++i) {
        append_short_id(
            out,
            compact_short_id(
                block.header,
                nonce,
                transaction_id(
                    block.transactions[i])
            )
        );
    }

    append_compact_size(out, 1U);
    append_compact_size(out, 0U);
    append_transaction(
        out,
        block.transactions.front()
    );

    return out;
}

std::optional<CompactBlock>
parse_compact_block(
    std::span<const Byte> payload,
    const consensus::ResourceLimits& limits)
{
    if (payload.size() >
        static_cast<std::size_t>(
            limits.max_block_serialized_bytes) ||
        payload.size() <
            kHeaderSize + sizeof(std::uint64_t) +
                3U) {
        return std::nullopt;
    }

    Reader reader{payload};
    const auto header =
        read_header(reader);
    const auto nonce =
        reader.little<std::uint64_t>();

    if (!header || !nonce) {
        return std::nullopt;
    }

    const auto short_count =
        reader.compact();

    if (!short_count ||
        *short_count >
            limits.max_block_transactions ||
        *short_count >
            static_cast<std::uint64_t>(
                reader.remaining() /
                kCompactShortIdBytes)) {
        return std::nullopt;
    }

    CompactBlock out;
    out.header = *header;
    out.nonce = *nonce;
    out.short_ids.reserve(
        static_cast<std::size_t>(
            *short_count)
    );

    for (std::uint64_t i = 0U;
         i < *short_count;
         ++i) {
        std::uint64_t short_id{0U};

        if (!read_short_id(
                reader,
                short_id)) {
            return std::nullopt;
        }

        out.short_ids.push_back(
            short_id
        );
    }

    const auto prefilled_count =
        reader.compact();

    if (!prefilled_count ||
        *prefilled_count == 0U ||
        *prefilled_count >
            kMaxCompactPrefilledTransactions ||
        *prefilled_count >
            limits.max_block_transactions) {
        return std::nullopt;
    }

    out.prefilled.reserve(
        static_cast<std::size_t>(
            *prefilled_count)
    );

    for (std::uint64_t i = 0U;
         i < *prefilled_count;
         ++i) {
        const auto index =
            reader.compact();

        if (!index ||
            *index >
                std::numeric_limits<
                    std::uint32_t>::max()) {
            return std::nullopt;
        }

        auto tx =
            read_transaction(
                reader,
                limits
            );

        if (!tx) {
            return std::nullopt;
        }

        out.prefilled.push_back(
            CompactPrefilledTransaction{
                .index =
                    static_cast<std::uint32_t>(
                        *index),
                .transaction =
                    std::move(*tx),
            }
        );
    }

    const std::uint64_t total =
        *short_count +
        *prefilled_count;

    if (reader.remaining() != 0U ||
        total == 0U ||
        total >
            limits.max_block_transactions ||
        !valid_prefilled(out)) {
        return std::nullopt;
    }

    return out;
}

CompactReconstructionResult
reconstruct_compact_block(
    const CompactBlock& compact,
    const Mempool& mempool)
{
    return finish_slots(
        compact,
        build_slots(
            compact,
            mempool,
            nullptr
        )
    );
}

Bytes serialize_getblocktxn(
    const BlockTransactionsRequest& request)
{
    Bytes out;
    out.insert(
        out.end(),
        request.block_hash.begin(),
        request.block_hash.end()
    );

    append_compact_size(
        out,
        static_cast<std::uint64_t>(
            request.indexes.size())
    );

    for (const auto index :
         request.indexes) {
        append_compact_size(
            out,
            index
        );
    }

    return out;
}

std::optional<BlockTransactionsRequest>
parse_getblocktxn(
    std::span<const Byte> payload,
    std::uint32_t max_transactions)
{
    Reader reader{payload};
    BlockTransactionsRequest out;

    if (!reader.hash(
            out.block_hash)) {
        return std::nullopt;
    }

    const auto count =
        reader.compact();

    if (!count ||
        *count == 0U ||
        *count > max_transactions) {
        return std::nullopt;
    }

    out.indexes.reserve(
        static_cast<std::size_t>(*count)
    );

    std::uint32_t previous{0U};
    bool first{true};

    for (std::uint64_t i = 0U;
         i < *count;
         ++i) {
        const auto index =
            reader.compact();

        if (!index ||
            *index >= max_transactions ||
            *index >
                std::numeric_limits<
                    std::uint32_t>::max()) {
            return std::nullopt;
        }

        const auto value =
            static_cast<std::uint32_t>(
                *index);

        if (!first &&
            value <= previous) {
            return std::nullopt;
        }

        out.indexes.push_back(value);
        previous = value;
        first = false;
    }

    if (reader.remaining() != 0U) {
        return std::nullopt;
    }

    return out;
}

Bytes serialize_blocktxn(
    const BlockTransactions& response)
{
    Bytes out;
    out.insert(
        out.end(),
        response.block_hash.begin(),
        response.block_hash.end()
    );

    append_compact_size(
        out,
        static_cast<std::uint64_t>(
            response.transactions.size())
    );

    for (const auto& [index, tx] :
         response.transactions) {
        append_compact_size(
            out,
            index
        );
        append_transaction(out, tx);
    }

    return out;
}

std::optional<BlockTransactions>
parse_blocktxn(
    std::span<const Byte> payload,
    const consensus::ResourceLimits& limits)
{
    if (payload.size() >
        static_cast<std::size_t>(
            limits.max_block_serialized_bytes)) {
        return std::nullopt;
    }

    Reader reader{payload};
    BlockTransactions out;

    if (!reader.hash(
            out.block_hash)) {
        return std::nullopt;
    }

    const auto count =
        reader.compact();

    if (!count ||
        *count == 0U ||
        *count >
            limits.max_block_transactions) {
        return std::nullopt;
    }

    out.transactions.reserve(
        static_cast<std::size_t>(*count)
    );

    std::uint32_t previous{0U};
    bool first{true};

    for (std::uint64_t i = 0U;
         i < *count;
         ++i) {
        const auto index =
            reader.compact();

        if (!index ||
            *index >=
                limits.max_block_transactions ||
            *index >
                std::numeric_limits<
                    std::uint32_t>::max()) {
            return std::nullopt;
        }

        const auto value =
            static_cast<std::uint32_t>(
                *index);

        if (!first &&
            value <= previous) {
            return std::nullopt;
        }

        auto tx =
            read_transaction(
                reader,
                limits
            );

        if (!tx) {
            return std::nullopt;
        }

        out.transactions.emplace_back(
            value,
            std::move(*tx)
        );

        previous = value;
        first = false;
    }

    if (reader.remaining() != 0U) {
        return std::nullopt;
    }

    return out;
}

CompactReconstructionResult
complete_compact_block(
    const CompactBlock& compact,
    const Mempool& mempool,
    const BlockTransactions& response)
{
    CompactReconstructionResult out;

    if (response.block_hash !=
        block_hash(compact.header)) {
        out.error =
            CompactBlockError::
                response_mismatch;
        return out;
    }

    auto completed =
        build_slots(
            compact,
            mempool,
            &response
        );

    if (completed.error !=
        CompactBlockError::none) {
        return finish_slots(
            compact,
            std::move(completed)
        );
    }

    if (!completed.missing.empty()) {
        out.error =
            CompactBlockError::
                response_mismatch;
        return out;
    }

    return finish_slots(
        compact,
        std::move(completed)
    );
}

} // namespace quintum::net
