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
- the persistent address manager is capped at 50,000 entries;
- public addrman admission caps one IPv4 /16 group at 64 entries, preventing one hosting/ISP subnet from filling the entire peer database;
- outbound selection prefers a different /16 from already-connected public peers and falls back only when the network is too small;
- `addr` responses advertise diverse network groups first before filling any remaining slots.

Each address record stores:

- IPv4 address and P2P port;
- service bits;
- last-seen time;
- last connection attempt;
- last successful connection;
- failure count;
- next allowed retry time.

Failed outbound connections use exponential retry backoff. A discovery attempt can automatically move to another eligible peer instead of repeatedly hammering the same failed endpoint.

### RandomX peer self-advertisement

The RandomX Testnet uses P2P protocol version 2. Its `version` payload adds the node's listening TCP port. For inbound v2 connections, the receiver combines that claimed listening port with the **socket-observed remote IPv4 address**, validates the resulting endpoint, stores it in addrman, and can return it to later peers through normal `getaddr/addr` exchange. The sender does not get to choose the advertised IP address.

This removes the single-seed topology trap: once multiple publicly reachable RandomX nodes have connected, the bootstrap node learns them and introduces later nodes to them. Existing legacy SHA-256 networks remain on protocol version 1 and retain their original 32-byte version payload.

Nodes behind NAT still need an actual inbound mapping for the advertised port to be reachable. Automatic UPnP/NAT-PMP remains a separate deployment item; unreachable advertised endpoints simply fail normal connection attempts and enter retry backoff.

The integration suite starts three local RandomX nodes, bootstraps two ordinary nodes through the first node, verifies direct peer discovery, shuts the bootstrap node down, mines a real RandomX block on an ordinary node, relays it directly to the other node, and verifies restart persistence.

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

The public Testnet has its first pinned numeric bootstrap endpoint:

- `212.193.15.139:38444`

Mainnet and Regtest seed lists remain empty. The Testnet endpoint is imported into addrman on first start and normal outbound connection logic handles it exactly like any other eligible peer.

On 2026-10-03 this seed was verified from an external mobile network: TCP port 38444 was reachable, a fresh Windows desktop install started in Testnet mode, discovered the node without any manual IP entry, completed the QUINTUM version/verack handshake and showed one live peer.

DNS seed resolution is now implemented cross-platform. The built-in DNS seed lists intentionally remain empty until a real QUINTUM seed domain is deployed and verified; until then the pinned numeric Testnet seed remains the first-start fallback.

## Security boundary

Public seed nodes can run `quintumd --disable-wallet`. In node-only mode QUINTUM does not create or open `wallet.dat`, `wallet_state.dat` or `wallet_meta.dat`; block/transaction validation, P2P relay, synchronization and an optional explicitly-addressed miner remain available. A public bootstrap server therefore does not need to hold wallet private keys.

P2P does not bypass consensus.

Block and transaction bytes received from peers enter the same validated Chainstate/UTXO/mempool paths used by local node operations. A remote peer cannot directly set height, UTXO, chain work, reward, difficulty or active tip.

## Deployment items not implemented yet

The first public Testnet seed node is deployed and externally verified. Remaining deployment/hardening items are:

- additional independent/geographically separate seed nodes;
- deployment of public DNS seed hostnames backed by multiple independently operated nodes;
- UPnP/NAT-PMP automatic inbound port mapping;
- production-grade peer reputation/eviction policy.

The runtime now also enforces a per-peer message-rate ceiling (256 messages/second by default). A peer that floods the node beyond this policy is disconnected before its messages can monopolize validation/service work. This limit is networking policy, not consensus, and can be tuned without changing block validity.

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


## Live public Testnet verification — 2026-10-03

The Stage 29 deployment was verified across two real machines and the public Internet, not only by unit/integration tests.

Verified path:

1. Ubuntu 24.04 VPS runs `quintumd --testnet` as a persistent systemd service.
2. The service listens on `0.0.0.0:38444`; host firewall allows the port.
3. An external mobile connection successfully opened TCP to `212.193.15.139:38444`.
4. A fresh Windows QUINTUM installer started the GUI in Testnet by default.
5. The Windows node discovered the hardcoded seed automatically with no manual peer/IP configuration.
6. GUI status showed `Network: testnet`, `Peers: 1`, `Node: running`.
7. The Windows reference miner performed real PoW at roughly 37 kH/s and found two valid blocks.
8. Local chain height advanced to 2 and the wallet showed 100 QTM as immature coinbase balance, consistent with two 50 QTM rewards and the 100-block maturity rule.
9. The VPS accepted the relayed blocks through normal P2P/consensus handling and persisted them.
10. After stopping the persistent service and reopening the same Testnet datadir with `quintumd`, the VPS reported `Height: 2` and the matching active tip, proving durable remote acceptance rather than GUI-only/local state.

This verifies the live end-to-end bootstrap and block path:

`fresh Windows install -> hardcoded seed discovery -> TCP handshake -> live peer -> real PoW -> local validation/storage -> inv/getdata/block relay -> remote consensus acceptance -> remote durable storage`.

No manual IP entry, PowerShell peer injection, private consensus bypass or developer mint path was used.


### Live reconnect observation — 2026-10-03

A real Windows laptop temporarily showed `Peers: 0` while the public Testnet peer was unavailable/retrying. No manual peer command, IP entry, phone-side server command or configuration change was made. After the runtime retry interval elapsed, the node automatically re-established the peer connection and the GUI changed to:

- `Local block height: 2`;
- `Peer best height: 2`;
- `Peers: 1`;
- `Synchronization: Up to date`;
- `Progress: 100%`.

This confirms the live reconnect/backoff path and the desktop peer-height/synchronization presentation on a real Windows install.


## RandomX Testnet bootstrap

The isolated RandomX Testnet uses P2P port **39444** and currently carries `212.193.15.139:39444` as its first hardcoded bootstrap candidate. This does not make that VPS a consensus dependency: after peers learn addresses through `addr/getaddr` and persist them in `peers.dat`, nodes connect directly. Public DNS seed hostnames remain a deployment step.

The Linux CI now publishes a `quintum-linux-x64` artifact containing `quintumd`, SHA-256 checksums, a hardened `quintumd-randomx-testnet.service`, and `install-randomx-testnet.sh`. The service runs with `--disable-wallet` and stores the new chain under `/var/lib/quintum-randomx/randomx-testnet`. It uses a separate executable and service name, so the historical SHA-256 Testnet service and data are not overwritten. The installer can open host UFW port 39444 when UFW is already active; provider-side firewall/NAT rules remain deployment infrastructure.
## Stage 31 compact block relay

New nodes advertise the `kServiceCompactBlocks` service bit during the existing version handshake. Block announcements remain ordinary `inv` entries, so legacy peers stay compatible.

When both peers support compact relay, the receiver requests inventory type `kInventoryCompactBlock`. The sender replies with `cmpctblock`: the normal block header, a per-announcement nonce, 48-bit SipHash transaction short IDs, and the coinbase transaction prefilled in full. The receiver reconstructs known transactions from its mempool. Missing or colliding entries are requested by exact block index with `getblocktxn` and returned with `blocktxn`.

A reconstructed block is accepted only after short-ID checks, index/order checks and Merkle-root reconstruction, then it is submitted through the same `NodeRuntime::submit_block_at` consensus/storage path as a full `block` message. If a peer does not advertise compact support, QUINTUM keeps the existing `inv -> getdata -> block` path.

## Stage 32 encrypted authenticated P2P transport

New nodes advertise the `kServiceEncryptedTransport` service bit in the existing `version` handshake. The initial `version/verack` exchange remains compatible with legacy QUINTUM peers. If either side lacks the service bit, the connection continues on the existing plaintext framing.

When both peers advertise encrypted transport, the initiator sends a 64-byte secp256k1 ElligatorSwift ephemeral public encoding in `encinit`; the responder returns its 64-byte encoding in `encack`. Both sides derive the same forward-secret X-only ECDH result using libsecp256k1's BIP324 ElligatorSwift hash function. HKDF-SHA256, domain-separated by QUINTUM network magic, derives independent initiator/responder traffic keys and a session identifier.

Before the peer session becomes active, both sides prove possession of the negotiated traffic keys with an AEAD-protected `encconf`. After confirmation, every normal QUINTUM wire message is carried inside a bounded ChaCha20-Poly1305 packet. The existing inner message header, command, checksum and payload are encrypted; packet length plus network magic are authenticated as additional data. Monocypher's incremental AEAD ratchets the directional key after each packet, so packet modification or reordering fails authentication and disconnects the peer.

Ephemeral private keys, ECDH material, HKDF intermediates and live AEAD contexts are explicitly wiped when no longer needed. The transport is opportunistic and backward compatible: it authenticates the encrypted session/integrity, not a permanent real-world peer identity. It is BIP324-inspired but intentionally not Bitcoin-BIP324 wire-compatible because QUINTUM preserves its existing `version/verack` negotiation and framing.
