#pragma once

#include "net/peer.hpp"
#include "net/sync.hpp"
#include "node/node.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace quintum::net {

inline constexpr std::size_t kMaxRelayInventoryItems = 128U;

enum class RelayError {
    none,
    not_started,
    transport_failed,
    malformed_message,
    unsupported_inventory,
    object_not_found,
    transaction_parse_failed,
    transaction_rejected,
    block_parse_failed,
    announced_hash_mismatch,
    block_rejected,
    storage_failed,
};

struct RelayResult {
    RelayError error{RelayError::none};
    PeerError peer_error{PeerError::none};
    NodeTransactionError transaction_error{
        NodeTransactionError::none
    };
    MempoolError mempool_error{MempoolError::none};
    UtxoApplyError transaction_apply_error{
        UtxoApplyError::none
    };
    NodeSubmitError block_submit_error{
        NodeSubmitError::none
    };
    ChainConnectError chain_error{
        ChainConnectError::none
    };
    StorageError storage_error{
        StorageError::none
    };
    std::optional<Hash256> accepted_transaction{};
    std::optional<Hash256> accepted_block{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == RelayError::none;
    }
};

[[nodiscard]] Bytes serialize_transaction_payload(
    const Transaction& transaction
);

[[nodiscard]] std::optional<Transaction>
parse_transaction_payload(
    std::span<const Byte> payload,
    const consensus::ResourceLimits& limits
);

[[nodiscard]] PeerError announce_transaction(
    PeerSession& peer,
    const Hash256& txid
);

[[nodiscard]] PeerError announce_block(
    PeerSession& peer,
    const Hash256& block_hash_value
);

[[nodiscard]] PeerError request_mempool_inventory(
    PeerSession& peer
);

[[nodiscard]] RelayResult serve_relay_once(
    PeerSession& peer,
    const NodeRuntime& node
);

[[nodiscard]] RelayResult receive_relay_once(
    PeerSession& peer,
    NodeRuntime& node,
    std::uint64_t adjusted_time
);

[[nodiscard]] RelayResult sync_mempool_from_peer(
    PeerSession& peer,
    NodeRuntime& node
);

} // namespace quintum::net
