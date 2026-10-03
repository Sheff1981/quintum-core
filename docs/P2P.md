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

Seed endpoints may be pinned as literal IPv4 addresses or DNS hostnames. Hostnames are resolved through the platform socket resolver to IPv4 candidates; duplicates and non-routable addresses are still filtered by the normal address-manager rules before they can become outbound candidates.

The built-in Mainnet/Testnet/Regtest seed lists remain intentionally empty until a real stable public QUINTUM seed node exists. The implementation does not ship a fabricated or unrelated endpoint merely to make discovery appear complete.

A successful peer keeps a short in-process reuse cooldown so discovery can move on to other candidates. When `peers.dat` is loaded by a new process, known-good peers become immediately eligible again; failed peers retain their persisted exponential backoff. This prevents an application update/restart from producing an artificial zero-peer interval while preserving peer diversity during one runtime.

## Security boundary

P2P does not bypass consensus.

Block and transaction bytes received from peers enter the same validated Chainstate/UTXO/mempool paths used by local node operations. A remote peer cannot directly set height, UTXO, chain work, reward, difficulty or active tip.

## Deployment items not implemented yet

The network runtime is functional, and DNS hostname resolution for configured seed endpoints is implemented. Public deployment infrastructure is intentionally still absent:

- live public seed nodes;
- published DNS seed names/records backed by those real nodes;
- UPnP/NAT-PMP automatic inbound port mapping;
- production-grade peer reputation/eviction policy.

These are deployment/hardening layers and do not replace the completed TCP, discovery, synchronization or relay mechanisms.


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


## Continuous node runtime

Stage 19 combines the previously independent P2P components into `NetworkRuntime`, the long-running networking service used by `quintumd`.

### Startup lifecycle

1. load or create the validated local blockchain state;
2. load `peers.dat`;
3. import configured/hardcoded bootstrap endpoints when present;
4. bind and listen on the network P2P port;
5. start the network worker;
6. accept inbound peers;
7. automatically select outbound peers from addrman;
8. perform `version/verack`;
9. run headers-first catch-up and mempool catch-up on new outbound connections;
10. enter continuous message servicing.

### Live peer servicing

Each active peer is polled without blocking the whole node on an idle socket. The runtime handles:

- `ping/pong`;
- `getaddr/addr`;
- `getheaders/headers`;
- `inv/getdata`;
- `tx`;
- `block`;
- `mempool`;
- `notfound`.

Unknown commands are ignored for forward compatibility.

### Automatic relay

Local wallet/API transaction admission queues a transaction inventory announcement. Local mining queues a block inventory announcement. Network-accepted transactions and blocks are re-announced so information propagates beyond the peer that originally supplied it.

The full object is not broadcast blindly: peers first receive `inv` and request unknown objects through `getdata`.

### Liveness and reconnect

The runtime sends asynchronous ping nonces after an idle interval and requires the matching pong before the liveness deadline. A failed outbound peer is removed from the active set and placed into a reconnect schedule while its addrman failure history is updated.

This reconnect path is separate from initial addrman selection, so a previously working live connection can be retried promptly while the persistent address manager still retains longer failure backoff state.

### Graceful shutdown

`quintumd` now remains alive until SIGINT/SIGTERM (for example Ctrl+C). Shutdown requests stop the worker, close peer sockets and the listener, and leave the already durable blockchain/peer databases intact.

### Stage 19 integration QA

The integration suite prepares two persistent nodes with a five-block height difference, starts both continuous runtimes and verifies:

- automatic outbound selection from the peer database/bootstrap set;
- real inbound acceptance and `version/verack`;
- automatic synchronization from height 100 to 105;
- idle `ping/pong` survival;
- signed transaction relay into the remote mempool;
- mining that transaction into height 106;
- live block relay and remote mempool cleanup;
- deliberate server shutdown and connection-loss detection;
- automatic reconnect after the server returns on the same endpoint;
- live propagation of height 107 after reconnect;
- graceful shutdown of both runtimes.
