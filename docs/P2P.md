# QUINTUM P2P Foundation

Stage 15 introduces the first real peer-to-peer transport layer.

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

This is intentionally the minimal lifecycle layer. Automatic peer discovery and long-running scheduling are the next layer.

## Security boundary

P2P does not bypass consensus.

Stage 15 transports only handshake/liveness messages. Future block and transaction messages must enter through the same validated Chainstate/UTXO paths already used by local mining and persistent storage.

## Not implemented yet

Stage 15 deliberately does not yet add:

- hardcoded seed nodes;
- DNS seeds;
- addr/getaddr;
- persistent addrman/peer database;
- headers-first synchronization;
- block relay;
- transaction relay;
- mempool relay;
- UPnP/NAT-PMP.

Those build on this transport and handshake foundation.
