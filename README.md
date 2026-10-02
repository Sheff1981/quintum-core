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

**M3 — P2P network foundation:** real TCP transport, version/verack handshake, ping/pong, network isolation, persistent peer address manager, addr/getaddr exchange, seed bootstrap hooks, retry/backoff and automatic outbound peer selection.

The public QUINTUM seed list is intentionally empty until real independent seed nodes exist; no fake or developer-only endpoint is embedded.

Next: **M4 — headers-first synchronization and relay:** exchange chain headers, request missing blocks, validate every received block through the existing consensus/Chainstate path, then add block/transaction relay.
