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

**M5 — Live transaction/block relay:** real TCP P2P transport, persistent peer discovery, headers-first blockchain synchronization, an in-memory validated mempool, transaction inventory relay, block inventory relay and mempool catch-up for newly connected peers.

Transactions admitted to the mempool are checked against the active UTXO set plus earlier mempool entries, including authorization, coinbase maturity, double-spend conflicts, money range and script/resource policy. Miners can build blocks directly from the mempool, and confirmed/conflicting entries are revalidated and removed after block activation or reorg.

Every received block still enters the existing consensus/Chainstate path; networking cannot bypass PoW, difficulty, timestamps, transactions, UTXO, coinbase reward or storage checks.

The public QUINTUM seed list is intentionally empty until real independent seed nodes exist; no fake or developer-only endpoint is embedded.

Next: **M6 — long-running network runtime:** combine discovery, reconnect, inbound/outbound peer servicing, headers/block sync and live relay into the continuously running node process.
