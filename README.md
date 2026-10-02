# QUINTUM Core

QUINTUM is an independent proof-of-work cryptocurrency and peer-to-peer network being built from the ground up in modern C++23.

> Status: **pre-alpha / protocol design**. No mainnet exists yet. Coins created in development networks have no monetary value.

## Principles

- Independent network and genesis block
- Proof of Work
- UTXO accounting model
- Fully validating nodes
- Peer-to-peer block and transaction relay
- No premine, hidden mint, master key, or developer backdoor
- Private keys remain under the user's control
- Windows-first desktop experience, with portable core architecture
- Consensus rules are documented and tested before mainnet launch

## Technology

- C++23
- CMake
- secp256k1 for transaction signatures
- SHA-256 family cryptographic hashing
- LevelDB-class chainstate storage
- SQLite-class wallet metadata storage
- Qt 6 desktop wallet planned after the node and network are stable

## Repository map

- `src/` — node/core source code
- `docs/` — protocol and architecture documentation
- `docs/ru/START_HERE.md` — plain-language Russian project guide
- `.github/workflows/` — reproducible CI builds

## Development rule

Consensus-critical constants are **DRAFT** until the genesis block and mainnet specification are deliberately frozen. After mainnet launch, incompatible consensus changes require explicit network-upgrade rules.

## Current milestone

**M6 — Continuous P2P node runtime:** `quintumd` now combines TCP listening, persistent peer discovery, automatic outbound connections, startup blockchain synchronization, live transaction/block relay, mempool catch-up, asynchronous ping/pong liveness, reconnect scheduling and graceful shutdown in one long-running node process.

The runtime listens on the network's own P2P port, learns and persists peer endpoints, maintains outbound connectivity, services inbound peers, requests missing chain data, relays newly accepted transactions and blocks, and automatically reconnects after a live outbound connection is lost.

Transactions and blocks received from the network still enter the same local validation paths used by the node itself. Networking cannot directly set height, chain work, UTXO state, rewards, difficulty or active tip.

The public QUINTUM seed list is intentionally empty until real independent seed infrastructure exists; no fake or developer-only endpoint is embedded.

Next: **M7 — Wallet Core:** persistent private-key storage, QUINTUM addresses, wallet-owned UTXO tracking, balances, transaction construction/signing and safe backup/recovery foundations.
