#include "net/diagnostics.hpp"
#include "crypto/randomx.hpp"
#include <chrono>
#include "net/sync.hpp"

#include "consensus/pow.hpp"
#include "core/serialize.hpp"
#include "primitives/block.hpp"
#include "primitives/transaction.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <string>
#include <utility>

namespace quintum::net {

template<class F>
void observe(
    const std::shared_ptr<Diagnostics>& diagnostics,
    F&& callback) noexcept
{
    if (diagnostics) {
        try {
            callback(*diagnostics);
        } catch (...) {
            // Diagnostics must never change networking or validation results.
        }
    }
}

namespace {

constexpr std::size_t kHeaderSize = 88U;
constexpr std::size_t kMaxInterleavedSyncMessages = 64U;

PeerError receive_sync_response(
    PeerSession& peer,
    std::string_view expected_command,
    bool allow_notfound,
    WireMessage& message,
    const SyncOptions& options = {})
{
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(options.io_timeout_ms);
    for (std::size_t skipped = 0U;
         skipped <= kMaxInterleavedSyncMessages;
         ++skipped) {
        if (options.cancel) {
            while (!peer.wait_readable(100U)) {
                if (options.cancel->load()) return PeerError::timeout;
                if (std::chrono::steady_clock::now() >= deadline)
                    return PeerError::timeout;
            }
            if (options.cancel->load()) return PeerError::timeout;
        }
        WireMessage candidate;
        const auto error =
            peer.receive_command(candidate);

        if (error != PeerError::none) {
            return error;
        }

        if (candidate.command == expected_command ||
            (allow_notfound &&
             candidate.command == "notfound")) {
            message = std::move(candidate);
            return PeerError::none;
        }

        // Live block announcements can legitimately arrive while a peer is
        // serving our synchronous IBD request. They are advisory: the next
        // getheaders round will discover the same (or a newer) tip.
        if (candidate.command == "inv" ||
            candidate.command == "pong") {
            continue;
        }

        // Keep liveness traffic independent from the request/response sync
        // stream so a ping cannot tear down an otherwise healthy peer.
        if (candidate.command == "ping") {
            const auto nonce =
                parse_nonce(candidate.payload);

            if (!nonce) {
                return PeerError::malformed_ping;
            }

            const auto pong =
                peer.send_command(
                    "pong",
                    serialize_nonce(*nonce)
                );

            if (pong != PeerError::none) {
                return pong;
            }

            continue;
        }

        return PeerError::unexpected_message;
    }

    return PeerError::unexpected_message;
}

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

} // namespace

Bytes serialize_chain_work(
    const Hash256& work)
{
    return Bytes(work.begin(), work.end());
}

std::optional<Hash256> parse_chain_work(
    std::span<const Byte> payload)
{
    Hash256 work{};

    if (payload.size() != work.size()) {
        return std::nullopt;
    }

    std::copy(
        payload.begin(),
        payload.end(),
        work.begin()
    );

    return work;
}

bool sync_driver_should_run(
    const Hash256& local_work,
    const std::optional<Hash256>& remote_work,
    std::uint32_t local_height,
    std::uint32_t remote_height,
    bool outbound) noexcept
{
    if (remote_work) {
        if (std::lexicographical_compare(
                local_work.begin(),
                local_work.end(),
                remote_work->begin(),
                remote_work->end())) {
            return true;
        }

        if (std::lexicographical_compare(
                remote_work->begin(),
                remote_work->end(),
                local_work.begin(),
                local_work.end())) {
            return false;
        }

        return outbound;
    }

    const bool local_is_behind =
        remote_height > local_height;

    return local_is_behind ||
        (remote_height == local_height && outbound);
}

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

SyncServiceResult serve_sync_message(
    PeerSession& peer,
    const Chainstate& chain,
    const WireMessage& message)
{
    SyncServiceResult out;

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
            inventory->empty() ||
            inventory->size() >
                kMaxBlockDownloadItems ||
            std::any_of(
                inventory->begin(),
                inventory->end(),
                [](const InventoryItem& item) {
                    return item.type !=
                        kInventoryBlock;
                })) {
            out.error = SyncError::malformed_message;
            return out;
        }

        for (const auto& item : *inventory) {
            const Block* block =
                chain.block(item.hash);

            if (block == nullptr) {
                const std::array<InventoryItem, 1>
                    missing{item};

                const auto payload =
                    serialize_inventory(missing);

                out.peer_error =
                    peer.send_command(
                        "notfound",
                        payload
                    );
            } else {
                const auto payload =
                    serialize_block_payload(*block);

                out.peer_error =
                    peer.send_command(
                        "block",
                        payload
                    );
            }

            if (out.peer_error !=
                PeerError::none) {
                out.error =
                    SyncError::transport_failed;
                return out;
            }
        }

        return out;
    }

    out.error = SyncError::malformed_message;
    return out;

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

    return serve_sync_message(
        peer,
        chain,
        message
    );
}

SyncResult sync_from_peer(
    PeerSession& peer,
    NodeRuntime& node,
    std::uint64_t adjusted_time,
    std::shared_ptr<Diagnostics> diagnostics,
    SyncOptions options)
{
    SyncResult out;
    const auto node_lock = [&] {
        return options.state_mutex ? std::unique_lock<std::mutex>(*options.state_mutex)
                                   : std::unique_lock<std::mutex>{};
    };
    const auto cancelled = [&] { return options.cancel && options.cancel->load(); };

    crypto::RandomXVerificationScope randomx_scope(
        [](void* context, std::string_view name, std::uint64_t microseconds) {
            const auto& journal =
                *static_cast<const std::shared_ptr<Diagnostics>*>(context);
            observe(journal, [&](auto& diagnostics_state) {
                if (name == "randomx_result_reused") {
                    diagnostics_state.counter(name, 1U);
                    return;
                }
                if (name == "randomx_cache_started") {
                    diagnostics_state.gauge("randomx_active", 1U);
                    diagnostics_state.event("randomx", "info", "cache_initializing");
                    return;
                }
                if (name == "randomx_hash_started") {
                    diagnostics_state.gauge("randomx_active", 2U);
                    return;
                }
                if (name == "randomx_operation_complete") {
                    diagnostics_state.gauge("randomx_active", 0U);
                    return;
                }
                if (name == "randomx_verify_ms") {
                    diagnostics_state.counter("randomx_calls", 1U);
                    diagnostics_state.gauge("randomx_last_hash_us", microseconds);
                }
                diagnostics_state.duration(name, microseconds);
            });
        },
        &diagnostics
    );

    struct Finish {
        std::shared_ptr<Diagnostics> diagnostics;
        SyncResult& result;
        PeerSession& peer;

        ~Finish()
        {
            observe(diagnostics, [&](auto& journal) {
                if (!result.ok()) {
                    const auto last_io = peer.last_io_failure();
                    const auto io = result.peer_error != PeerError::none &&
                            last_io.error == result.peer_error
                        ? last_io : PeerIoFailure{};
                    const std::string description =
                        "sync_error=" + std::to_string(static_cast<int>(result.error)) +
                        " peer_error=" + std::to_string(static_cast<int>(result.peer_error)) +
                        " chain_error=" + std::to_string(static_cast<int>(result.chain_error)) +
                        " submit_error=" + std::to_string(static_cast<int>(result.submit_error)) +
                        " storage_error=" + std::to_string(static_cast<int>(result.storage_error)) +
                        " socket_error=" + std::to_string(io.socket_error) +
                        " eof=" + std::to_string(io.remote_closed ? 1 : 0) +
                        " partial_bytes=" + std::to_string(io.partial_io_bytes) +
                        " wire_error=" + std::to_string(static_cast<int>(io.wire_error));
                    journal.failure("sync", static_cast<int>(result.error), description);
                    journal.peer_state("disconnected", description,
                        result.peer_error == PeerError::timeout ? "sync socket or ping timeout" : "");
                }
            });
        }
    } finish{diagnostics, out, peer};

    auto initial_lock = node_lock();
    if (!node.started()) {
        out.error = SyncError::not_started;
        return out;
    }

    node.clear_block_body_recovery();
    if (initial_lock.owns_lock()) initial_lock.unlock();

    struct RecoveryCacheGuard {
        NodeRuntime& node;
        std::mutex* mutex;

        ~RecoveryCacheGuard()
        {
            auto lock = mutex ? std::unique_lock<std::mutex>(*mutex)
                              : std::unique_lock<std::mutex>{};
            node.clear_block_body_recovery();
        }
    };

    [[maybe_unused]] RecoveryCacheGuard recovery_guard{
        node, options.state_mutex
    };

    std::optional<Hash256> continuation;

    for (;;) {
        if (cancelled()) { out.error = SyncError::cancelled; return out; }
        const auto snapshot = [&] {
            auto lock = node_lock();
            return node.chain().header_validation_snapshot();
        }();
        auto locator = build_block_locator(snapshot);

        if (locator.empty()) {
            out.error = SyncError::malformed_message;
            return out;
        }

        if (continuation) {
            locator.erase(
                std::remove(
                    locator.begin(),
                    locator.end(),
                    *continuation
                ),
                locator.end()
            );
            locator.insert(
                locator.begin(),
                *continuation
            );

            if (locator.size() >
                kMaxBlockLocators) {
                locator.resize(
                    kMaxBlockLocators
                );
            }
        }

        GetHeadersRequest request{
            .locator = std::move(locator),
            .stop = {},
        };

        observe(diagnostics, [](auto& d) {
            d.stage("headers_sync", "sync");
        });
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

        observe(diagnostics, [](auto& d) {
            d.stage("header_wait", "sync");
        });
        const auto header_wait_started = std::chrono::steady_clock::now();
        WireMessage response;
        out.peer_error =
            receive_sync_response(
                peer,
                "headers",
                false,
                response, options
            );

        observe(diagnostics, [&](auto& d) {
            d.duration("header_wait_ms", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-header_wait_started).count());
            if (out.peer_error == PeerError::none) d.network_progress();
        });
        if (out.peer_error != PeerError::none) {
            out.error = cancelled() ? SyncError::cancelled : SyncError::transport_failed;
            return out;
        }

        if (response.command != "headers") {
            out.error = SyncError::malformed_message;
            return out;
        }

        observe(diagnostics, [](auto& journal) {
            journal.counter("header_batches_received", 1U);
        });
        const auto headers =
            parse_headers(response.payload);

        if (!headers) {
            observe(diagnostics, [](auto& journal) {
                journal.counter("header_batches_rejected", 1U);
                journal.gauge("header_batch_size", 0U);
            });
            out.error = SyncError::malformed_message;
            return out;
        }

        out.headers_received += headers->size();
        observe(diagnostics, [&](auto& d) {
            d.counter("headers_received", headers->size());
            d.gauge("header_batch_size",headers->size());
            d.stage("header_validation", "sync");
            d.peer_state("validating_headers");
            d.gauge("header_batch_progress", 0U);
        });
        bool header_batch_validated = false;
        std::size_t validated_prefix = 0U;
        struct HeaderBatch {
            std::shared_ptr<Diagnostics> diagnostics;
            std::size_t count;
            bool& validated;
            std::size_t& prefix;
            SyncResult& result;

            ~HeaderBatch()
            {
                observe(diagnostics, [](auto& journal) {
                    journal.gauge("headers_validating", 0U);
                    journal.peer_state("initializing");
                });
                if (!validated && result.error != SyncError::cancelled &&
                    result.error != SyncError::transport_failed) {
                    observe(diagnostics, [&](auto& journal) {
                        // One header caused rejection. The suffix was not
                        // validated; the whole batch remains unaccepted.
                        journal.counter("headers_rejected", count > 0U ? 1U : 0U);
                        journal.counter("headers_unvalidated", count > prefix ? count - prefix - 1U : 0U);
                        journal.counter("header_batches_rejected", 1U);
                    });
                }
            }
        } header_batch{diagnostics, headers->size(), header_batch_validated, validated_prefix, out};

        if (headers->empty()) {
            header_batch_validated = true;
            observe(diagnostics, [](auto& d) {
                d.counter("header_batches_accepted",1);
            });
            return out;
        }

        Hash256 previous =
            headers->front().previous_block;

        const bool parent_was_requested =
            std::find(
                request.locator.begin(),
                request.locator.end(),
                previous
            ) != request.locator.end();

        if (!parent_was_requested ||
            !snapshot.has_block(previous) ||
            (continuation &&
             previous != *continuation)) {
            out.error = SyncError::invalid_header_chain;
            return out;
        }

        for (const auto& header : *headers) {
            if (header.previous_block != previous) {
                out.error = SyncError::invalid_header_chain;
                return out;
            }

            previous = block_hash(header);
        }

        // Reject consensus-invalid header chains before requesting any block
        // body. Chainstate owns the rules so headers-first and full block
        // acceptance cannot diverge on difficulty, MTP or RandomX seeds.
        const auto header_validation_started = std::chrono::steady_clock::now();
        std::optional<std::uint64_t> pending_ping;
        auto last_message = std::chrono::steady_clock::now();
        auto ping_sent = last_message;
        const auto service_validation = [&](std::size_t progress, bool completed) {
            observe(diagnostics, [&](auto& journal) {
                journal.gauge("headers_validating", completed ? 0U : 1U);
                if (completed) {
                    const auto& header = (*headers)[progress - 1U];
                    const bool known = snapshot.has_block(block_hash(header));
                    journal.counter(known ? "headers_known" : "headers_verified", 1U);
                    if (!known) journal.counter("headers_validated", 1U);
                    journal.gauge("header_batch_progress", progress);
                }
            });
            if (cancelled()) return false;
            // The sync worker is the exclusive socket owner. Never race its
            // encrypted receive/send state with the event loop.
            for (std::size_t serviced = 0U;
                 serviced < kMaxInterleavedSyncMessages && peer.wait_readable(0U);
                 ++serviced) {
                WireMessage message;
                out.peer_error = peer.receive_command(message);
                if (out.peer_error != PeerError::none) return false;
                last_message = std::chrono::steady_clock::now();
                observe(diagnostics, [](auto& journal) { journal.network_progress(); });
                if (message.command == "ping") {
                    const auto nonce = parse_nonce(message.payload);
                    if (!nonce) { out.peer_error = PeerError::malformed_ping; return false; }
                    out.peer_error = peer.send_command("pong", serialize_nonce(*nonce));
                    if (out.peer_error != PeerError::none) return false;
                } else if (message.command == "pong") {
                    const auto nonce = parse_nonce(message.payload);
                    if (!nonce || (pending_ping && *nonce != *pending_ping)) {
                        out.peer_error = PeerError::malformed_ping; return false;
                    }
                    pending_ping.reset();
                } else if (message.command == "inv") {
                    if (!parse_inventory(message.payload)) {
                        out.peer_error = PeerError::unexpected_message; return false;
                    }
                } else {
                    out.peer_error = PeerError::unexpected_message; return false;
                }
            }
            const auto now = std::chrono::steady_clock::now();
            if (pending_ping && now - ping_sent >= std::chrono::seconds(options.ping_timeout_seconds)) {
                out.peer_error = PeerError::timeout; return false;
            }
            if (!pending_ping && options.ping_interval_seconds > 0U &&
                now - last_message >= std::chrono::seconds(options.ping_interval_seconds)) {
                pending_ping = static_cast<std::uint64_t>(now.time_since_epoch().count());
                out.peer_error = peer.send_command("ping", serialize_nonce(*pending_ping));
                if (out.peer_error != PeerError::none) return false;
                ping_sent = now;
            }
            return !options.header_progress || options.header_progress(progress, completed);
        };
        const auto validated = snapshot.validate_headers(*headers, adjusted_time, service_validation);

        observe(diagnostics, [&](auto& d) {
            d.duration("headers_verify_ms", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-header_validation_started).count());
        });
        if (!validated.ok()) {
            validated_prefix = validated.header_index;
            out.chain_error = validated.error;
            if (validated.error == ChainConnectError::validation_cancelled) {
                out.error = (cancelled() || out.peer_error == PeerError::none)
                    ? SyncError::cancelled : SyncError::transport_failed;
                return out;
            }
            out.error =
                validated.error ==
                        ChainConnectError::
                            invalid_proof_of_work
                    ? SyncError::
                          invalid_header_pow
                    : SyncError::
                          invalid_header_consensus;
            return out;
        }

        header_batch_validated = true;
        observe(diagnostics, [&](auto& d) {
            d.counter("header_batches_accepted",1);
            d.peer_state("initializing");
        });
        std::vector<BlockHeader> missing_headers;
        missing_headers.reserve(headers->size());

        {
        auto lock = node_lock();
        for (const auto& header : *headers) {
            const Hash256 expected_hash =
                block_hash(header);

            if (node.chain().has_block(
                    expected_hash) &&
                node.chain().has_block_body(
                    expected_hash)) {
                continue;
            }

            missing_headers.push_back(header);
        }

        }

        for (std::size_t offset = 0U;
             offset < missing_headers.size();
             offset += kMaxBlockDownloadItems) {
            if (cancelled()) { out.error = SyncError::cancelled; return out; }
            const std::size_t batch_size =
                std::min<std::size_t>(
                    kMaxBlockDownloadItems,
                    missing_headers.size() - offset
                );

            std::vector<InventoryItem> request_items;
            request_items.reserve(batch_size);

            for (std::size_t i = 0U;
                 i < batch_size;
                 ++i) {
                request_items.push_back(
                    InventoryItem{
                        .type = kInventoryBlock,
                        .hash = block_hash(
                            missing_headers[
                                offset + i
                            ]
                        ),
                    }
                );
            }

            observe(diagnostics, [](auto& d) {
                d.stage("blocks_sync", "sync");
            });
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

            out.blocks_requested += batch_size;
            observe(diagnostics, [&](auto& d) {
                d.counter("blocks_requested",batch_size);
            });
            ++out.block_request_batches;

            for (std::size_t i = 0U;
                 i < batch_size;
                 ++i) {
                if (cancelled()) { out.error = SyncError::cancelled; return out; }
                const auto& header =
                    missing_headers[offset + i];
                const Hash256 expected_hash =
                    block_hash(header);

                observe(diagnostics, [](auto& d) {
                    d.stage("block_wait", "sync");
                });
                const auto block_wait_started = std::chrono::steady_clock::now();
                WireMessage block_message;
                out.peer_error =
                    receive_sync_response(
                        peer,
                        "block",
                        true,
                        block_message, options
                    );

                observe(diagnostics, [&](auto& d) {
                    d.duration("block_wait_ms", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-block_wait_started).count());
                    if (out.peer_error == PeerError::none) d.network_progress();
                });
                if (out.peer_error !=
                    PeerError::none) {
                    out.error =
                        SyncError::transport_failed;
                    return out;
                }

                if (block_message.command ==
                    "notfound") {
                    const auto notfound =
                        parse_inventory(
                            block_message.payload
                        );

                    if (!notfound ||
                        notfound->size() != 1U ||
                        notfound->front().type !=
                            kInventoryBlock ||
                        notfound->front().hash !=
                            expected_hash) {
                        out.error =
                            SyncError::malformed_message;
                        return out;
                    }

                    out.error =
                        SyncError::block_not_found;
                    return out;
                }

                if (block_message.command !=
                    "block") {
                    out.error =
                        SyncError::malformed_message;
                    return out;
                }

                observe(diagnostics, [](auto& d) {
                    d.counter("blocks_received",1);
                    d.stage("block_validation", "sync");
                });
                bool block_accepted = false;
                const auto block_validation_started = std::chrono::steady_clock::now();
                struct BlockValidation {
                    std::shared_ptr<Diagnostics> diagnostics;
                    bool& accepted;
                    std::chrono::steady_clock::time_point started;

                    ~BlockValidation()
                    {
                        observe(diagnostics, [&](auto& journal) {
                            journal.duration("block_verify_ms",
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                    std::chrono::steady_clock::now() - started).count());
                            if (!accepted) {
                                journal.counter("blocks_rejected", 1U);
                            }
                        });
                    }
                } block_validation{diagnostics, block_accepted, block_validation_started};
                const auto block =
                    parse_block_payload(
                        block_message.payload,
                        snapshot.params().limits
                    );

                if (!block) {
                    out.error =
                        SyncError::block_parse_failed;
                    return out;
                }

                if (!headers_match(
                        block->header,
                        header) ||
                    block_hash(block->header) !=
                        expected_hash) {
                    out.error =
                        SyncError::
                            announced_block_mismatch;
                    return out;
                }

                bool metadata_known = false;
                std::optional<std::uint32_t> accepted_height;
                const auto submitted = [&] {
                    auto lock = node_lock();
                    metadata_known = node.chain().has_block(expected_hash);
                    const auto result = metadata_known
                        ? node.restore_block_body(*block)
                        : node.submit_block_at(*block, adjusted_time);
                    if (result.ok()) accepted_height = node.chain().height();
                    return result;
                }();

                out.submit_error =
                    submitted.error;
                out.chain_error =
                    submitted.connect.chain.error;
                out.storage_error =
                    submitted.connect.storage_error;

                if (!submitted.ok()) {
                    if (submitted.error ==
                        NodeSubmitError::
                            storage_failed) {
                        out.error =
                            SyncError::storage_failed;
                    } else {
                        out.error =
                            SyncError::block_rejected;
                    }
                    return out;
                }

                block_accepted = true;
                observe(diagnostics, [&](auto& d) {
                    d.counter("blocks_accepted",1);
                    d.counter("headers_committed", metadata_known ? 0U : 1U);
                    if (accepted_height) d.local_height(*accepted_height);
                });
                if (metadata_known) {
                    ++out.block_bodies_restored;
                } else {
                    ++out.blocks_accepted;
                }

                out.reorganized =
                    out.reorganized ||
                    submitted.connect.chain.reorganized;
            }
        }

        continuation =
            block_hash(headers->back());

        if (!header_batch_may_continue(
                headers->size())) {
            return out;
        }
    }
}

} // namespace quintum::net

