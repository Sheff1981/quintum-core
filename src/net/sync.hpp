#pragma once

#include "chain/chainstate.hpp"
#include "net/peer.hpp"
#include "node/node.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace quintum::net {

inline constexpr std::size_t kMaxBlockLocators = 32U;
inline constexpr std::size_t kMaxHeadersPerMessage = 2'000U;
inline constexpr std::size_t kMaxGetDataItems = 128U;
inline constexpr std::uint32_t kInventoryTransaction = 1U;
inline constexpr std::uint32_t kInventoryBlock = 2U;

struct GetHeadersRequest {
    std::vector<Hash256> locator{};
    Hash256 stop{};
};

struct InventoryItem {
    std::uint32_t type{0U};
    Hash256 hash{};
};

enum class SyncError {
    none,
    not_started,
    transport_failed,
    malformed_message,
    invalid_header_chain,
    invalid_header_pow,
    stalled,
    block_not_found,
    block_parse_failed,
    announced_block_mismatch,
    block_rejected,
    storage_failed,
};

struct SyncResult {
    SyncError error{SyncError::none};
    PeerError peer_error{PeerError::none};
    NodeSubmitError submit_error{NodeSubmitError::none};
    ChainConnectError chain_error{ChainConnectError::none};
    StorageError storage_error{StorageError::none};
    std::size_t headers_received{0U};
    std::size_t blocks_requested{0U};
    std::size_t blocks_accepted{0U};
    bool reorganized{false};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == SyncError::none;
    }
};

struct SyncServiceResult {
    SyncError error{SyncError::none};
    PeerError peer_error{PeerError::none};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == SyncError::none;
    }
};

[[nodiscard]] std::vector<Hash256> build_block_locator(
    const Chainstate& chain
);

[[nodiscard]] Bytes serialize_getheaders(
    const GetHeadersRequest& request
);

[[nodiscard]] std::optional<GetHeadersRequest> parse_getheaders(
    std::span<const Byte> payload
);

[[nodiscard]] Bytes serialize_headers(
    std::span<const BlockHeader> headers
);

[[nodiscard]] std::optional<std::vector<BlockHeader>> parse_headers(
    std::span<const Byte> payload
);

[[nodiscard]] Bytes serialize_inventory(
    std::span<const InventoryItem> items
);

[[nodiscard]] std::optional<std::vector<InventoryItem>>
parse_inventory(
    std::span<const Byte> payload
);

[[nodiscard]] Bytes serialize_block_payload(
    const Block& block
);

[[nodiscard]] std::optional<Block> parse_block_payload(
    std::span<const Byte> payload,
    const consensus::ResourceLimits& limits
);

[[nodiscard]] SyncServiceResult serve_sync_message(
    PeerSession& peer,
    const Chainstate& chain,
    const WireMessage& message
);

[[nodiscard]] SyncServiceResult serve_sync_once(
    PeerSession& peer,
    const Chainstate& chain
);

[[nodiscard]] SyncResult sync_from_peer(
    PeerSession& peer,
    NodeRuntime& node,
    std::uint64_t adjusted_time
);

} // namespace quintum::net
