# QUINTUM P2P Foundation

Stages 15–16 implement the first real peer-to-peer transport and peer-discovery layers.

## Wire message framing

Every QUINTUM P2P message uses a fixed 24-byte header:

- 4 bytes: network magic from ChainParams;
- 12 bytes: zero-padded ASCII command;
- 4 bytes: little-endian payload size;
- 4 bytes: first four bytes of double-SHA-256(payload);
- payload bytes follow the header.

The current payload ceiling is 2,000,000 bytes. Oversized frames are rejected before payload allocation.

Mainnet, Testnet and Regtest use their existing independent network magic values. A message framed for one network is rejected by another network.

## Handshake

The initial handshake is intentionally Bitcoin-like.

Outbound peer:

1. connect TCP;
2. send `version`;
3. receive and validate remote `version`;
4. send `verack`;
5. receive `verack`;
6. peer becomes active.

Inbound peer:

1. accept TCP;
2. receive and validate remote `version`;
3. send local `version`;
4. receive `verack`;
5. send `verack`;
6. peer becomes active.

The version payload currently contains:

- protocol version;
- service bits;
- timestamp;
- random/local connection nonce;
- reported blockchain height.

A connection using the same nonce on both ends is rejected as a self-connection.

## Liveness

After the handshake, Stage 15 supports:

- `ping`;
- `pong`;
- nonce matching;
- socket send/receive timeouts;
- orderly disconnect;
- reconnection;
- inbound and outbound sessions.

On POSIX systems sends use `MSG_NOSIGNAL`, so a remote disconnect cannot terminate the node through SIGPIPE.

## Peer lifecycle

`ConnectionManager` owns active `PeerSession` objects and supports:

- adding a validated peer;
- querying active peer count;
- closing peers;
- pruning closed sessions;
- disconnecting all peers.

This is intentionally the minimal lifecycle layer.

## Peer discovery and address manager

Stage 16 adds Bitcoin-style peer address exchange and durable address management:

- `getaddr` asks a connected peer for known endpoints;
- `addr` carries up to 1,000 validated IPv4 endpoints per message;
- duplicate endpoints are collapsed;
- non-routable/private addresses are rejected on public networks;
- loopback/private addresses may be enabled explicitly for Regtest and local QA;
- the persistent address manager is capped at 50,000 entries.

Each address record stores:

- IPv4 address and P2P port;
- service bits;
- last-seen time;
- last connection attempt;
- last successful connection;
- failure count;
- next allowed retry time.

Failed outbound connections use exponential retry backoff. A discovery attempt can automatically move to another eligible peer instead of repeatedly hammering the same failed endpoint.

### Persistent `peers.dat`

The address manager writes `peers.dat` under the node data directory.

The file contains:

- QUINTUM peer-store magic/version;
- network identity and network magic;
- peer records and retry state;
- double-SHA-256 checksum.

Writes use a temporary file plus durable replacement. Windows uses a Unicode-safe wide path and `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`; POSIX uses `rename()` and directory fsync.

A corrupt or wrong-network peer database is rejected. It is not silently treated as a valid peer source.

### Seed bootstrap

The discovery layer supports pinned numeric hardcoded seed endpoints and can import them into the address manager on first start.

The built-in Mainnet/Testnet/Regtest seed lists are currently intentionally empty because there is not yet a real public QUINTUM seed node. The implementation does not invent a developer-controlled server merely to make discovery appear complete.

DNS seed resolution is a later network-deployment step.

## Security boundary

P2P does not bypass consensus.

Stage 15 transports only handshake/liveness messages. Future block and transaction messages must enter through the same validated Chainstate/UTXO paths already used by local mining and persistent storage.

## Not implemented yet

The current network layer does not yet add:

- live public seed infrastructure;
- DNS seeds;
- headers-first synchronization;
- block relay;
- transaction relay;
- mempool relay;
- long-running connection scheduler in the final GUI/node runtime;
- UPnP/NAT-PMP.

Those build on the completed transport and discovery foundation.


## Headers-first blockchain synchronization

Stage 17 adds active-chain synchronization over the Stage 15 transport.

### Block locator

A syncing node sends a Bitcoin-style locator containing recent active-chain hashes and exponentially older hashes back to Genesis. The remote node finds the first locator hash on its active chain and returns headers after that common point.

A `headers` message is bounded to 2,000 headers. If the remote chain is farther ahead, synchronization continues in additional batches using the last received header as the continuation locator.

### Messages

- `getheaders` — block locator plus optional stop hash;
- `headers` — up to 2,000 serialized QUINTUM block headers;
- `getdata` — bounded inventory request;
- `block` — complete serialized block;
- `notfound` — requested block was not available.

Before requesting block bodies, the client verifies header linkage and Proof of Work. Header discovery is not authoritative state: contextual difficulty, timestamp, Merkle root, transaction, UTXO, coinbase and chain-work rules are enforced again when the complete block is submitted.

### Block parser limits

Network block decoding is bounded before large allocations:

- P2P envelope limit remains 2,000,000 bytes;
- accepted block payload cannot exceed the consensus serialized-block limit;
- transaction count is bounded by the consensus block limit;
- scripts are bounded by the consensus script limit;
- CompactSize counts are checked against remaining input before reserve/allocation.

### Consensus boundary

A received block is passed through `NodeRuntime::submit_block_at()`, which delegates to `PersistentChainstate::connect_block()`.

Therefore a peer cannot directly set height, UTXO, chain work or active tip. If a downloaded branch becomes strictly heavier and is fully valid, the already-existing Chainstate reorg mechanism activates it. The resulting state is durably committed before the sync operation treats the block as accepted.

Stage 17 integration QA covers both ordinary catch-up and a competing-branch reorg followed by restart recovery.


## Live inventory relay and mempool

Stage 18 adds the live object-relay layer used after initial blockchain synchronization.

### Transaction mempool

`NodeRuntime` now owns an in-memory mempool. A candidate transaction is accepted only when:

- it is not a coinbase transaction;
- its txid is not already in the mempool;
- its serialized size and scripts stay within policy/consensus resource ceilings;
- all existing mempool entries can still be applied to a copy of the current active UTXO set;
- the new transaction then applies successfully to that same staged UTXO view.

Because admission uses `UtxoSet::apply_transaction()`, the same authorization, missing-input, coinbase-maturity, amount and double-spend rules used for blocks are reused for mempool admission.

The mempool is bounded to 50,000 transactions and 64 MiB. It is intentionally memory-only at this stage; a restart drops unconfirmed transactions, which can be learned again from connected peers.

After a block is accepted or a reorg changes the active chain, the mempool is reconciled against the new UTXO view. Confirmed, conflicting or otherwise invalidated transactions are removed while still-valid dependent transactions are retained.

### Mining from mempool

A miner can build a block directly from mempool transactions. Selection is bounded by the consensus transaction-count and serialized-block limits. If the mempool is larger than one block, the node chooses the largest valid prefix that fits the real block template rather than attempting to place the whole pool into one block.

### Inventory relay

Stage 18 uses Bitcoin-style inventory types:

- transaction inventory type 1;
- block inventory type 2.

A live transaction relay is:

`inv(txid) -> getdata(txid) -> tx`

A live block relay is:

`inv(blockhash) -> getdata(blockhash) -> block`

Only the hash is announced first. The receiving peer requests the full object only when it does not already have it.

The receiver verifies that the downloaded object's computed hash matches the announced inventory hash before local admission.

### Mempool catch-up

A newly connected peer can send `mempool`. The remote peer answers with transaction inventory, after which the requester fetches missing transactions through the normal `getdata -> tx` path.

### Broadcast

The relay layer can announce a new transaction or block to every currently connected `ConnectionManager` peer. Peers that fail during announcement are closed and pruned.

The continuous event loop that automatically invokes discovery, servicing, synchronization and relay for the lifetime of `quintumd` is the next runtime milestone.
