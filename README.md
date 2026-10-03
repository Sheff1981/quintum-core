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
- Durable restart-safe blockchain storage
- Authenticated encrypted wallet storage with Argon2id + XChaCha20-Poly1305
- BIP32 deterministic recovery with 24-word English recovery phrases and atomic restoration
- Qt 6 Widgets desktop wallet shell, kept separate from the consensus/core library

## Repository map

- `src/` — node/core source code
- `docs/` — protocol and architecture documentation
- `docs/ru/START_HERE.md` — plain-language Russian project guide
- `.github/workflows/` — reproducible CI builds

## Development rule

Consensus-critical constants are **DRAFT** until the genesis block and mainnet specification are deliberately frozen. After mainnet launch, incompatible consensus changes require explicit network-upgrade rules.

## Current milestone

**M13 — Qt 6 desktop wallet shell (Stage 26):** QUINTUM now has its first real desktop application target, built with Qt 6 Widgets on top of the existing `NetworkRuntime` API.

The GUI provides **Overview, Send, Receive and Transactions** pages. It shows confirmed/available/pending/immature balances, current block height, peers and mempool state; creates and copies receive addresses; displays persistent labeled transaction history; and sends through the guarded Stage 25 Preview -> Confirm path with automatic fees.

The desktop executable is `QUINTUM` (`QUINTUM.exe` on Windows). New GUI wallets require a password and are created through the existing encrypted wallet path. Pre-alpha builds default to **regtest** unless `--testnet` or `--mainnet` is explicitly selected.

Qt remains an optional build dependency: `quintum_core` and `quintumd` build without Qt. A dedicated GUI CI installs pinned Qt 6.8.0 and builds the desktop target on Linux and Windows. Its smoke mode starts a real regtest node, encrypted wallet and `MainWindow` in a temporary data directory, then shuts them down cleanly.

Next: Stage 27 — finish the operational desktop wallet: recovery phrase UX, richer sync status, address-book editing, mining/status controls and settings before Windows packaging.
