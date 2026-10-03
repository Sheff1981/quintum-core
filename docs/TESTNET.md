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


## Seed deployment package

Stage 29 CI now publishes a Linux x86-64 headless seed package containing `quintumd`, a hardened systemd unit and an operator runbook. The package does not fabricate or pin any public endpoint.

The headless daemon also supports two deployment-QA controls:

- `--addnode HOST[:PORT]` resolves an explicit IPv4/DNS peer and places it into the normal address manager;
- `--run-seconds N` runs the normal P2P runtime for a bounded interval and prints the final peer count.

These switches exist so a newly provisioned seed can be verified from a second network before its real endpoint is committed to the hardcoded Testnet seed list. They do not change consensus or bypass handshake/block validation.


### Walletless seed operation

Public bootstrap infrastructure now runs with `--network-only`. In this mode the node starts the full chainstate, mempool, P2P listener, peer discovery, headers/block synchronization and relay path, but it does not start a wallet or create `wallet.dat`, `wallet_meta.dat` or `wallet_state.dat`. This avoids putting unnecessary private keys on a public seed host.
