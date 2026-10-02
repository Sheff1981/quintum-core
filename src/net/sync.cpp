#include "net/sync.hpp"

#include "consensus/pow.hpp"
#include "core/serialize.hpp"
#include "primitives/block.hpp"
#include "primitives/transaction.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace quintum::net {
namespace {

constexpr std::size_t kHeaderSize = 88U;

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

    [[nodiscard]] std::optional<std::uint64_t> compact()
    {
        return read_compact_size(
            data_,
            offset_
        );
    }

    [[nodiscard]] bool hash(Hash256& value)
    {
        return raw(value.data(), value.size());
    }

    [[nodiscard]] bool bytes(
        Bytes& value,
        std::size_t size)
    {
        if (size > remaining()) {
            return false;
        }

        value.assign(
            data_.begin() +
                static_cast<std::ptrdiff_t>(offset_),
            data_.begin() +
                static_cast<std::ptrdiff_t>(
                    offset_ + size)
        );
        offset_ += size;
        return true;
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
                static_cast<std::ptrdiff_t>(offset_),
            static_cast<std::ptrdiff_t>(size),
            destination
        );
        offset_ += size;
        return true;
    }

    [[nodiscard]] std::size_t remaining() const noexcept
    {
        return data_.size() - offset_;
    }

private:
    std::span<const Byte> data_{};
    std::size_t offset_{0U};
};

bool count_fits_remaining(
    std::uint64_t count,
    std::size_t remaining,
    std::size_t minimum_size) noexcept
{
    if (count >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max())) {
        return false;
    }

    if (minimum_size == 0U) {
        return true;
    }

    return count <=
        static_cast<std::uint64_t>(
            remaining / minimum_size);
}

std::optional<BlockHeader> read_header(
    Reader& reader)
{
    BlockHeader header;

    const auto version =
        reader.little<std::uint32_t>();

    if (!version ||
        !reader.hash(header.previous_block) ||
        !reader.hash(header.merkle_root)) {
        return std::nullopt;
    }

    const auto timestamp =
        reader.little<std::uint64_t>();
    const auto bits =
        reader.little<std::uint32_t>();
    const auto nonce =
        reader.little<std::uint64_t>();

    if (!timestamp || !bits || !nonce) {
        return std::nullopt;
    }

    header.version = *version;
    header.timestamp = *timestamp;
    header.bits = *bits;
    header.nonce = *nonce;
    return header;
}

std::optional<Transaction> read_transaction(
    Reader& reader,
    const consensus::ResourceLimits& limits)
{
    Transaction tx;

    const auto version =
        reader.little<std::uint32_t>();
    if (!version) {
        return std::nullopt;
    }
    tx.version = *version;

    const auto input_count = reader.compact();
    if (!input_count ||
        !count_fits_remaining(
            *input_count,
            reader.remaining(),
            41U)) {
        return std::nullopt;
    }

    tx.inputs.reserve(
        static_cast<std::size_t>(*input_count)
    );

    for (std::uint64_t i = 0U;
         i < *input_count;
         ++i) {
        TxInput input;

        if (!reader.hash(
                input.previous_output.txid)) {
            return std::nullopt;
        }

        const auto index =
            reader.little<std::uint32_t>();
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
                static_cast<std::size_t>(
                    *script_size))) {
            return std::nullopt;
        }

        const auto sequence =
            reader.little<std::uint32_t>();
        if (!sequence) {
            return std::nullopt;
        }
        input.sequence = *sequence;

        tx.inputs.push_back(std::move(input));
    }

    const auto output_count = reader.compact();
    if (!output_count ||
        !count_fits_remaining(
            *output_count,
            reader.remaining(),
            9U)) {
        return std::nullopt;
    }

    tx.outputs.reserve(
        static_cast<std::size_t>(*output_count)
    );

    for (std::uint64_t i = 0U;
         i < *output_count;
         ++i) {
        TxOutput output;

        const auto value =
            reader.little<Amount>();
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
                static_cast<std::size_t>(
                    *script_size))) {
            return std::nullopt;
        }

        tx.outputs.push_back(std::move(output));
    }

    const auto lock_time =
        reader.little<std::uint32_t>();
    if (!lock_time) {
        return std::nullopt;
    }
    tx.lock_time = *lock_time;

    return tx;
}

bool headers_match(
    const BlockHeader& lhs,
    const BlockHeader& rhs)
{
    return serialize_block_header(lhs) ==
           serialize_block_header(rhs);
}

std::optional<std::uint32_t> common_locator_height(
    const Chainstate& chain,
    std::span<const Hash256> locator)
{
    for (const auto& hash : locator) {
        const auto height =
            chain.active_height(hash);
        if (height) {
            return height;
        }
    }
    return std::nullopt;
}

std::vector<BlockHeader> headers_after(
    const Chainstate& chain,
    std::uint32_t height,
    const Hash256& stop)
{
    std::vector<BlockHeader> out;
    out.reserve(kMaxHeadersPerMessage);

    const auto tip = chain.height();
    if (!tip || height >= *tip) {
        return out;
    }

    for (std::uint32_t current = height + 1U;
         current <= *tip &&
         out.size() < kMaxHeadersPerMessage;
         ++current) {
        const auto header =
            chain.active_header(current);
        if (!header) {
            break;
        }

        out.push_back(*header);

        const auto hash = block_hash(*header);
        if (hash == stop) {
            break;
        }

        if (current ==
            std::numeric_limits<std::uint32_t>::max()) {
            break;
        }
    }

    return out;
}

bool is_zero_hash(
    const Hash256& hash) noexcept
{
    return std::all_of(
        hash.begin(),
        hash.end(),
        [](Byte value) {
            return value == 0U;
        }
    );
}

} // namespace

std::vector<Hash256> build_block_locator(
    const Chainstate& chain)
{
    std::vector<Hash256> locator;

    const auto tip = chain.height();
    if (!tip) {
        return locator;
    }

    std::uint32_t height = *tip;
    std::uint32_t step{1U};

    while (locator.size() < kMaxBlockLocators) {
        const auto hash =
            chain.active_hash(height);
        if (!hash) {
            break;
        }

        locator.push_back(*hash);

        if (height == 0U) {
            break;
        }

        if (locator.size() > 10U &&
            step <=
                std::numeric_limits<std::uint32_t>::max() / 2U) {
            step *= 2U;
        }

        height =
            height > step
                ? height - step
                : 0U;
    }

    if (!locator.empty() &&
        chain.active_hash(0U) &&
        locator.back() != *chain.active_hash(0U) &&
        locator.size() < kMaxBlockLocators) {
        locator.push_back(
            *chain.active_hash(0U)
        );
    }

    return locator;
}

Bytes serialize_getheaders(
    const GetHeadersRequest& request)
{
    const std::size_t count =
        std::min<std::size_t>(
            request.locator.size(),
            kMaxBlockLocators
        );

    Bytes out;
    append_compact_size(
        out,
        static_cast<std::uint64_t>(count)
    );

    for (std::size_t i = 0U;
         i < count;
         ++i) {
        out.insert(
            out.end(),
            request.locator[i].begin(),
            request.locator[i].end()
        );
    }

    out.insert(
        out.end(),
        request.stop.begin(),
        request.stop.end()
    );

    return out;
}

std::optional<GetHeadersRequest> parse_getheaders(
    std::span<const Byte> payload)
{
    Reader reader{payload};
    const auto count = reader.compact();

    if (!count ||
        *count == 0U ||
        *count > kMaxBlockLocators ||
        !count_fits_remaining(
            *count,
            reader.remaining(),
            32U)) {
        return std::nullopt;
    }

    GetHeadersRequest request;
    request.locator.reserve(
        static_cast<std::size_t>(*count)
    );

    for (std::uint64_t i = 0U;
         i < *count;
         ++i) {
        Hash256 hash{};
        if (!reader.hash(hash)) {
            return std::nullopt;
        }
        request.locator.push_back(hash);
    }

    if (!reader.hash(request.stop) ||
        reader.remaining() != 0U) {
        return std::nullopt;
    }

    return request;
}

Bytes serialize_headers(
    std::span<const BlockHeader> headers)
{
    const std::size_t count =
        std::min<std::size_t>(
            headers.size(),
            kMaxHeadersPerMessage
        );

    Bytes out;
    append_compact_size(
        out,
        static_cast<std::uint64_t>(count)
    );

    for (std::size_t i = 0U;
         i < count;
         ++i) {
        const auto bytes =
            serialize_block_header(headers[i]);
        out.insert(
            out.end(),
            bytes.begin(),
            bytes.end()
        );
    }

    return out;
}

std::optional<std::vector<BlockHeader>> parse_headers(
    std::span<const Byte> payload)
{
    Reader reader{payload};
    const auto count = reader.compact();

    if (!count ||
        *count > kMaxHeadersPerMessage ||
        !count_fits_remaining(
            *count,
            reader.remaining(),
            kHeaderSize)) {
        return std::nullopt;
    }

    std::vector<BlockHeader> headers;
    headers.reserve(
        static_cast<std::size_t>(*count)
    );

    for (std::uint64_t i = 0U;
         i < *count;
         ++i) {
        auto header = read_header(reader);
        if (!header) {
            return std::nullopt;
        }
        headers.push_back(*header);
    }

    if (reader.remaining() != 0U) {
        return std::nullopt;
    }

    return headers;
}

Bytes serialize_inventory(
    std::span<const InventoryItem> items)
{
    const std::size_t count =
        std::min<std::size_t>(
            items.size(),
            kMaxGetDataItems
        );

    Bytes out;
    append_compact_size(
        out,
        static_cast<std::uint64_t>(count)
    );

    for (std::size_t i = 0U;
         i < count;
         ++i) {
        append_little_endian(
            out,
            items[i].type
        );
        out.insert(
            out.end(),
            items[i].hash.begin(),
            items[i].hash.end()
        );
    }

    return out;
}

std::optional<std::vector<InventoryItem>>
parse_inventory(
    std::span<const Byte> payload)
{
    Reader reader{payload};
    const auto count = reader.compact();

    if (!count ||
        *count == 0U ||
        *count > kMaxGetDataItems ||
        !count_fits_remaining(
            *count,
            reader.remaining(),
            36U)) {
        return std::nullopt;
    }

    std::vector<InventoryItem> items;
    items.reserve(
        static_cast<std::size_t>(*count)
    );

    for (std::uint64_t i = 0U;
         i < *count;
         ++i) {
        const auto type =
            reader.little<std::uint32_t>();
        Hash256 hash{};

        if (!type || !reader.hash(hash)) {
            return std::nullopt;
        }

        items.push_back(
            InventoryItem{
                .type = *type,
                .hash = hash,
            }
        );
    }

    if (reader.remaining() != 0U) {
        return std::nullopt;
    }

    return items;
}

Bytes serialize_block_payload(
    const Block& block)
{
    Bytes out =
        serialize_block_header(block.header);

    append_compact_size(
        out,
        static_cast<std::uint64_t>(
            block.transactions.size())
    );

    for (const auto& tx : block.transactions) {
        const auto bytes =
            serialize_transaction(tx);
        out.insert(
            out.end(),
            bytes.begin(),
            bytes.end()
        );
    }

    return out;
}

std::optional<Block> parse_block_payload(
    std::span<const Byte> payload,
    const consensus::ResourceLimits& limits)
{
    if (payload.size() >
        static_cast<std::size_t>(
            limits.max_block_serialized_bytes)) {
        return std::nullopt;
    }

    Reader reader{payload};
    auto header = read_header(reader);
    if (!header) {
        return std::nullopt;
    }

    Block block;
    block.header = *header;

    const auto count = reader.compact();
    if (!count ||
        *count > limits.max_block_transactions ||
        !count_fits_remaining(
            *count,
            reader.remaining(),
            10U)) {
        return std::nullopt;
    }

    block.transactions.reserve(
        static_cast<std::size_t>(*count)
    );

    for (std::uint64_t i = 0U;
         i < *count;
         ++i) {
        auto tx =
            read_transaction(reader, limits);
        if (!tx) {
            return std::nullopt;
        }
        block.transactions.push_back(
            std::move(*tx)
        );
    }

    if (reader.remaining() != 0U) {
        return std::nullopt;
    }

    const auto expected =
        serialized_block_size(block);

    if (!expected ||
        *expected != payload.size()) {
        return std::nullopt;
    }

    return block;
}

SyncServiceResult serve_sync_once(
    PeerSession& peer,
    const Chainstate& chain)
{
    SyncServiceResult out;
    WireMessage message;

    out.peer_error =
        peer.receive_command(message);

    if (out.peer_error != PeerError::none) {
        out.error = SyncError::transport_failed;
        return out;
    }

    if (message.command == "getheaders") {
        const auto request =
            parse_getheaders(message.payload);

        if (!request) {
            out.error = SyncError::malformed_message;
            return out;
        }

        const auto common =
            common_locator_height(
                chain,
                request->locator
            );

        std::vector<BlockHeader> headers;
        if (common) {
            headers = headers_after(
                chain,
                *common,
                request->stop
            );
        }

        const auto payload =
            serialize_headers(headers);

        out.peer_error =
            peer.send_command(
                "headers",
                payload
            );

        if (out.peer_error != PeerError::none) {
            out.error = SyncError::transport_failed;
        }

        return out;
    }

    if (message.command == "getdata") {
        const auto inventory =
            parse_inventory(message.payload);

        if (!inventory ||
            inventory->size() != 1U ||
            inventory->front().type !=
                kInventoryBlock) {
            out.error = SyncError::malformed_message;
            return out;
        }

        const auto& item = inventory->front();
        const Block* block =
            chain.block(item.hash);

        if (block == nullptr) {
            const auto payload =
                serialize_inventory(*inventory);

            out.peer_error =
                peer.send_command(
                    "notfound",
                    payload
                );

            if (out.peer_error != PeerError::none) {
                out.error =
                    SyncError::transport_failed;
            }
            return out;
        }

        const auto payload =
            serialize_block_payload(*block);

        out.peer_error =
            peer.send_command(
                "block",
                payload
            );

        if (out.peer_error != PeerError::none) {
            out.error = SyncError::transport_failed;
        }

        return out;
    }

    out.error = SyncError::malformed_message;
    return out;
}

SyncResult sync_from_peer(
    PeerSession& peer,
    NodeRuntime& node,
    std::uint64_t adjusted_time)
{
    SyncResult out;

    if (!node.started()) {
        out.error = SyncError::not_started;
        return out;
    }

    GetHeadersRequest request{
        .locator =
            build_block_locator(node.chain()),
        .stop = {},
    };

    if (request.locator.empty()) {
        out.error = SyncError::malformed_message;
        return out;
    }

    const auto request_payload =
        serialize_getheaders(request);

    out.peer_error =
        peer.send_command(
            "getheaders",
            request_payload
        );

    if (out.peer_error != PeerError::none) {
        out.error = SyncError::transport_failed;
        return out;
    }

    WireMessage response;
    out.peer_error =
        peer.receive_command(response);

    if (out.peer_error != PeerError::none) {
        out.error = SyncError::transport_failed;
        return out;
    }

    if (response.command != "headers") {
        out.error = SyncError::malformed_message;
        return out;
    }

    const auto headers =
        parse_headers(response.payload);

    if (!headers) {
        out.error = SyncError::malformed_message;
        return out;
    }

    out.headers_received = headers->size();

    if (headers->empty()) {
        return out;
    }

    Hash256 previous =
        headers->front().previous_block;

    if (!node.chain().is_on_active_chain(previous)) {
        out.error = SyncError::invalid_header_chain;
        return out;
    }

    for (const auto& header : *headers) {
        if (header.previous_block != previous) {
            out.error = SyncError::invalid_header_chain;
            return out;
        }

        if (consensus::check_proof_of_work(
                header,
                node.chain().params().pow) !=
            consensus::PowCheckError::none) {
            out.error = SyncError::invalid_header_pow;
            return out;
        }

        previous = block_hash(header);
    }

    for (const auto& header : *headers) {
        const Hash256 expected_hash =
            block_hash(header);

        if (node.chain().has_block(expected_hash)) {
            continue;
        }

        const std::array<InventoryItem, 1>
            request_items{
                InventoryItem{
                    .type = kInventoryBlock,
                    .hash = expected_hash,
                }
            };

        const auto inventory =
            serialize_inventory(request_items);

        out.peer_error =
            peer.send_command(
                "getdata",
                inventory
            );

        if (out.peer_error != PeerError::none) {
            out.error = SyncError::transport_failed;
            return out;
        }

        ++out.blocks_requested;

        WireMessage block_message;
        out.peer_error =
            peer.receive_command(block_message);

        if (out.peer_error != PeerError::none) {
            out.error = SyncError::transport_failed;
            return out;
        }

        if (block_message.command == "notfound") {
            out.error = SyncError::block_not_found;
            return out;
        }

        if (block_message.command != "block") {
            out.error = SyncError::malformed_message;
            return out;
        }

        const auto block =
            parse_block_payload(
                block_message.payload,
                node.chain().params().limits
            );

        if (!block) {
            out.error = SyncError::block_parse_failed;
            return out;
        }

        if (!headers_match(
                block->header,
                header) ||
            block_hash(block->header) !=
                expected_hash) {
            out.error =
                SyncError::announced_block_mismatch;
            return out;
        }

        const auto submitted =
            node.submit_block_at(
                *block,
                adjusted_time
            );

        out.submit_error = submitted.error;
        out.chain_error =
            submitted.connect.chain.error;
        out.storage_error =
            submitted.connect.storage_error;

        if (!submitted.ok()) {
            if (submitted.error ==
                NodeSubmitError::storage_failed) {
                out.error = SyncError::storage_failed;
            } else {
                out.error = SyncError::block_rejected;
            }
            return out;
        }

        ++out.blocks_accepted;
        out.reorganized =
            out.reorganized ||
            submitted.connect.chain.reorganized;
    }

    return out;
}

} // namespace quintum::net
