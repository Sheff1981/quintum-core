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
- BIP32 deterministic recovery foundation with atomic replacement and backup support
- Qt 6 desktop wallet planned after the node and network are stable

## Repository map

- `src/` — node/core source code
- `docs/` — protocol and architecture documentation
- `docs/ru/START_HERE.md` — plain-language Russian project guide
- `.github/workflows/` — reproducible CI builds

## Development rule

Consensus-critical constants are **DRAFT** until the genesis block and mainnet specification are deliberately frozen. After mainnet launch, incompatible consensus changes require explicit network-upgrade rules.

## Current milestone

**M9 — Wallet state/indexing (Stage 22):** the wallet now keeps a durable `wallet_state.dat` cache containing wallet-owned confirmed UTXOs and transaction history. Normal synchronization advances from the previously indexed active-chain tip instead of rescanning from Genesis every time.

The cache contains no private keys. It is checksummed and bound to the network, active-chain tip and complete wallet public-key set. Corruption, reorgs, wallet-key changes or importing a historical key automatically force a correctness-first rebuild from the authoritative blockchain.

History tracks confirmed, unconfirmed and inactive wallet transactions with received/spent amounts, confirmations and exact fees when they are provable. Because the mempool remains memory-only, previously unconfirmed transactions become inactive after restart until seen again.

Stage 22 also adds a local fee-policy foundation: size-based fee arithmetic and a current-mempool fee-rate recommendation exposed through `NetworkRuntimeStatus`. It is wallet policy only and does not alter consensus, monetary policy or block validity.

Next wallet work: user-facing recovery/mnemonic workflow, confirmation-target fee selection and remaining desktop-facing wallet APIs before Qt GUI/release hardening.
