# QUINTUM Public Testnet Deployment Gate

Stage 29 prepares the software for a real multi-host Testnet. This document deliberately separates code readiness from public infrastructure readiness.

## Network identity

The existing Testnet identity is unchanged:

- network: `testnet`;
- P2P port: `38444/TCP`;
- RPC port reservation: `38445`;
- message start: `b7 d7 16 5a`;
- block target spacing: 600 seconds;
- Testnet allows minimum-difficulty blocks under the existing consensus rules;
- Testnet has its own Genesis block and its own data directory.

A normal Stage 29 desktop launch selects Testnet. Regtest and Mainnet remain separate networks.

## Bootstrap flow

Once a real seed endpoint is pinned, a fresh client needs no manually entered peer IP:

1. load `peers.dat` when it exists;
2. otherwise import configured/hardcoded seed endpoints;
3. resolve a seed hostname to IPv4 when necessary;
4. connect with QUINTUM `version/verack`;
5. perform headers-first synchronization and validated block download;
6. learn additional addresses through `getaddr/addr`;
7. persist usable peers in `peers.dat`;
8. reconnect from the peer database on later starts.

All downloaded blocks and transactions still pass through the normal consensus, UTXO, signature, mempool and chain-work validation paths.

## Current deployment gate

The repository intentionally contains no fabricated public seed. Before declaring a public Testnet release candidate, deploy at least one stable independently reachable node with TCP 38444 exposed, verify it from a different network, then pin its real IPv4 address or DNS hostname in the Testnet seed list.

After the first seed is pinned, perform external multi-host QA:

- fresh Windows install with no pre-existing `peers.dat`;
- automatic peer discovery with no manual IP entry;
- headers/blocks synchronization;
- transaction relay and confirmation;
- mining and block relay;
- forced disconnect/reconnect;
- competing-branch heavier-chain reorg;
- restart/update while preserving wallet, blockchain and peers;
- long-duration soak with at least three geographically separate nodes.

Mainnet must not be frozen or launched from this stage.
