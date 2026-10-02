#include "net/relay.hpp"

#include "core/serialize.hpp"
#include "primitives/transaction.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace quintum::net {
namespace {

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
        return bytes_raw(
            value.data(),
            value.size()
        );
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

    [[nodiscard]] std::size_t remaining() const noexcept
    {
        return data_.size() - offset_;
    }

private:
    [[nodiscard]] bool bytes_raw(
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

    std::span<const Byte> data_{};
    std::size_t offset_{0U};
};

bool count_fits_remaining(
    std::uint64_t count,
    std::size_t remaining,
    std::size_t minimum_bytes) noexcept
{
    if (count >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max())) {
        return false;
    }

    if (minimum_bytes == 0U) {
        return true;
    }

    return count <=
        static_cast<std::uint64_t>(
            remaining / minimum_bytes);
}

PeerError send_inventory(
    PeerSession& peer,
    std::uint32_t type,
    const Hash256& hash)
{
    const std::array<InventoryItem, 1> items{
        InventoryItem{
            .type = type,
            .hash = hash,
        }
    };

    const auto payload =
        serialize_inventory(items);

    return peer.send_command(
        "inv",
        payload
    );
}

RelayResult submit_transaction_payload(
    std::span<const Byte> payload,
    NodeRuntime& node,
    const std::optional<Hash256>& expected)
{
    RelayResult out;

    const auto transaction =
        parse_transaction_payload(
            payload,
            node.chain().params().limits
        );

    if (!transaction) {
        out.error =
            RelayError::transaction_parse_failed;
        return out;
    }

    const auto txid =
        transaction_id(*transaction);

    if (expected && txid != *expected) {
        out.error =
            RelayError::announced_hash_mismatch;
        return out;
    }

    const auto submitted =
        node.submit_transaction(*transaction);

    out.transaction_error = submitted.error;
    out.mempool_error =
        submitted.mempool.error;
    out.transaction_apply_error =
        submitted.mempool.transaction_error;

    if (!submitted.ok()) {
        if (submitted.mempool.error ==
                MempoolError::duplicate) {
            return out;
        }

        out.error =
            RelayError::transaction_rejected;
        return out;
    }

    out.accepted_transaction = txid;
    return out;
}

RelayResult submit_block_payload(
    std::span<const Byte> payload,
    NodeRuntime& node,
    std::uint64_t adjusted_time,
    const std::optional<Hash256>& expected)
{
    RelayResult out;

    const auto block =
        parse_block_payload(
            payload,
            node.chain().params().limits
        );

    if (!block) {
        out.error = RelayError::block_parse_failed;
        return out;
    }

    const auto hash =
        block_hash(block->header);

    if (expected && hash != *expected) {
        out.error =
            RelayError::announced_hash_mismatch;
        return out;
    }

    const auto submitted =
        node.submit_block_at(
            *block,
            adjusted_time
        );

    out.block_submit_error = submitted.error;
    out.chain_error =
        submitted.connect.chain.error;
    out.storage_error =
        submitted.connect.storage_error;

    if (!submitted.ok()) {
        if (submitted.error ==
            NodeSubmitError::storage_failed) {
            out.error = RelayError::storage_failed;
        } else {
            out.error = RelayError::block_rejected;
        }
        return out;
    }

    out.accepted_block = hash;
    return out;
}

RelayResult request_object(
    PeerSession& peer,
    NodeRuntime& node,
    std::uint64_t adjusted_time,
    const InventoryItem& item)
{
    RelayResult out;

    const std::array<InventoryItem, 1> request{
        item
    };

    const auto request_payload =
        serialize_inventory(request);

    out.peer_error =
        peer.send_command(
            "getdata",
            request_payload
        );

    if (out.peer_error != PeerError::none) {
        out.error = RelayError::transport_failed;
        return out;
    }

    WireMessage response;
    out.peer_error =
        peer.receive_command(response);

    if (out.peer_error != PeerError::none) {
        out.error = RelayError::transport_failed;
        return out;
    }

    if (response.command == "notfound") {
        out.error = RelayError::object_not_found;
        return out;
    }

    if (item.type == kInventoryTransaction) {
        if (response.command != "tx") {
            out.error = RelayError::malformed_message;
            return out;
        }

        return submit_transaction_payload(
            response.payload,
            node,
            item.hash
        );
    }

    if (item.type == kInventoryBlock) {
        if (response.command != "block") {
            out.error = RelayError::malformed_message;
            return out;
        }

        return submit_block_payload(
            response.payload,
            node,
            adjusted_time,
            item.hash
        );
    }

    out.error =
        RelayError::unsupported_inventory;
    return out;
}

} // namespace

Bytes serialize_transaction_payload(
    const Transaction& transaction)
{
    return serialize_transaction(transaction);
}

std::optional<Transaction>
parse_transaction_payload(
    std::span<const Byte> payload,
    const consensus::ResourceLimits& limits)
{
    if (payload.empty() ||
        payload.size() >
            static_cast<std::size_t>(
                limits.max_block_serialized_bytes)) {
        return std::nullopt;
    }

    Reader reader{payload};
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

        const auto script_size =
            reader.compact();

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
        tx.inputs.push_back(
            std::move(input)
        );
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

        const auto script_size =
            reader.compact();

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

        tx.outputs.push_back(
            std::move(output)
        );
    }

    const auto lock_time =
        reader.little<std::uint32_t>();

    if (!lock_time ||
        reader.remaining() != 0U) {
        return std::nullopt;
    }

    tx.lock_time = *lock_time;

    const auto expected_size =
        serialized_transaction_size(tx);

    if (!expected_size ||
        *expected_size != payload.size()) {
        return std::nullopt;
    }

    return tx;
}

PeerError announce_transaction(
    PeerSession& peer,
    const Hash256& txid)
{
    return send_inventory(
        peer,
        kInventoryTransaction,
        txid
    );
}

PeerError announce_block(
    PeerSession& peer,
    const Hash256& block_hash_value)
{
    return send_inventory(
        peer,
        kInventoryBlock,
        block_hash_value
    );
}

std::size_t broadcast_transaction(
    ConnectionManager& peers,
    const Hash256& txid)
{
    std::size_t sent{0U};

    for (std::size_t i = 0U;
         i < peers.size();
         ++i) {
        auto* peer = peers.peer(i);
        if (peer == nullptr) {
            continue;
        }

        if (announce_transaction(
                *peer,
                txid) ==
            PeerError::none) {
            ++sent;
        } else {
            peer->close();
        }
    }

    peers.prune_closed();
    return sent;
}

std::size_t broadcast_block(
    ConnectionManager& peers,
    const Hash256& block_hash_value)
{
    std::size_t sent{0U};

    for (std::size_t i = 0U;
         i < peers.size();
         ++i) {
        auto* peer = peers.peer(i);
        if (peer == nullptr) {
            continue;
        }

        if (announce_block(
                *peer,
                block_hash_value) ==
            PeerError::none) {
            ++sent;
        } else {
            peer->close();
        }
    }

    peers.prune_closed();
    return sent;
}

PeerError request_mempool_inventory(
    PeerSession& peer)
{
    return peer.send_command(
        "mempool",
        {}
    );
}

RelayResult serve_relay_once(
    PeerSession& peer,
    const NodeRuntime& node)
{
    RelayResult out;
    WireMessage message;

    out.peer_error =
        peer.receive_command(message);

    if (out.peer_error != PeerError::none) {
        out.error = RelayError::transport_failed;
        return out;
    }

    if (message.command == "mempool") {
        if (!message.payload.empty()) {
            out.error = RelayError::malformed_message;
            return out;
        }

        const auto ids =
            node.mempool().transaction_ids(
                kMaxRelayInventoryItems
            );

        std::vector<InventoryItem> items;
        items.reserve(ids.size());

        for (const auto& txid : ids) {
            items.push_back(
                InventoryItem{
                    .type =
                        kInventoryTransaction,
                    .hash = txid,
                }
            );
        }

        const auto payload =
            serialize_inventory(items);

        out.peer_error =
            peer.send_command(
                "inv",
                payload
            );

        if (out.peer_error != PeerError::none) {
            out.error = RelayError::transport_failed;
        }

        return out;
    }

    if (message.command != "getdata") {
        out.error = RelayError::malformed_message;
        return out;
    }

    const auto inventory =
        parse_inventory(message.payload);

    if (!inventory) {
        out.error = RelayError::malformed_message;
        return out;
    }

    std::vector<InventoryItem> missing;

    for (const auto& item : *inventory) {
        if (item.type == kInventoryTransaction) {
            const Transaction* transaction =
                node.mempool().transaction(
                    item.hash
                );

            if (transaction == nullptr) {
                missing.push_back(item);
                continue;
            }

            const auto payload =
                serialize_transaction_payload(
                    *transaction
                );

            out.peer_error =
                peer.send_command(
                    "tx",
                    payload
                );
        } else if (item.type == kInventoryBlock) {
            const Block* block =
                node.chain().block(
                    item.hash
                );

            if (block == nullptr) {
                missing.push_back(item);
                continue;
            }

            const auto payload =
                serialize_block_payload(*block);

            out.peer_error =
                peer.send_command(
                    "block",
                    payload
                );
        } else {
            missing.push_back(item);
            continue;
        }

        if (out.peer_error != PeerError::none) {
            out.error = RelayError::transport_failed;
            return out;
        }
    }

    if (!missing.empty()) {
        const auto payload =
            serialize_inventory(missing);

        out.peer_error =
            peer.send_command(
                "notfound",
                payload
            );

        if (out.peer_error != PeerError::none) {
            out.error = RelayError::transport_failed;
        }
    }

    return out;
}

RelayResult receive_relay_once(
    PeerSession& peer,
    NodeRuntime& node,
    std::uint64_t adjusted_time)
{
    RelayResult out;

    if (!node.started()) {
        out.error = RelayError::not_started;
        return out;
    }

    WireMessage message;
    out.peer_error =
        peer.receive_command(message);

    if (out.peer_error != PeerError::none) {
        out.error = RelayError::transport_failed;
        return out;
    }

    if (message.command == "tx") {
        return submit_transaction_payload(
            message.payload,
            node,
            std::nullopt
        );
    }

    if (message.command == "block") {
        return submit_block_payload(
            message.payload,
            node,
            adjusted_time,
            std::nullopt
        );
    }

    if (message.command != "inv") {
        out.error = RelayError::malformed_message;
        return out;
    }

    const auto inventory =
        parse_inventory(message.payload);

    if (!inventory ||
        inventory->size() >
            kMaxRelayInventoryItems) {
        out.error = RelayError::malformed_message;
        return out;
    }

    for (const auto& item : *inventory) {
        if (item.type ==
            kInventoryTransaction) {
            if (node.mempool().contains(
                    item.hash)) {
                continue;
            }
        } else if (item.type ==
                   kInventoryBlock) {
            if (node.chain().has_block(
                    item.hash)) {
                continue;
            }
        } else {
            continue;
        }

        auto received =
            request_object(
                peer,
                node,
                adjusted_time,
                item
            );

        if (!received.ok()) {
            return received;
        }

        if (received.accepted_transaction) {
            out.accepted_transaction =
                received.accepted_transaction;
        }

        if (received.accepted_block) {
            out.accepted_block =
                received.accepted_block;
        }
    }

    return out;
}

RelayResult sync_mempool_from_peer(
    PeerSession& peer,
    NodeRuntime& node)
{
    RelayResult out;

    if (!node.started()) {
        out.error = RelayError::not_started;
        return out;
    }

    out.peer_error =
        request_mempool_inventory(peer);

    if (out.peer_error != PeerError::none) {
        out.error = RelayError::transport_failed;
        return out;
    }

    WireMessage message;
    out.peer_error =
        peer.receive_command(message);

    if (out.peer_error != PeerError::none) {
        out.error = RelayError::transport_failed;
        return out;
    }

    if (message.command != "inv") {
        out.error = RelayError::malformed_message;
        return out;
    }

    const auto inventory =
        parse_inventory(message.payload);

    if (!inventory) {
        out.error = RelayError::malformed_message;
        return out;
    }

    for (const auto& item : *inventory) {
        if (item.type !=
            kInventoryTransaction) {
            continue;
        }

        if (node.mempool().contains(item.hash)) {
            continue;
        }

        auto received =
            request_object(
                peer,
                node,
                0U,
                item
            );

        if (!received.ok()) {
            return received;
        }

        if (received.accepted_transaction) {
            out.accepted_transaction =
                received.accepted_transaction;
        }
    }

    return out;
}

} // namespace quintum::net
