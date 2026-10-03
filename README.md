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
- Qt 6 desktop wallet planned after the node and network are stable

## Repository map

- `src/` — node/core source code
- `docs/` — protocol and architecture documentation
- `docs/ru/START_HERE.md` — plain-language Russian project guide
- `.github/workflows/` — reproducible CI builds

## Development rule

Consensus-critical constants are **DRAFT** until the genesis block and mainnet specification are deliberately frozen. After mainnet launch, incompatible consensus changes require explicit network-upgrade rules.

## Current milestone

**M10 — Recovery UX foundation (Stage 23):** seed-native encrypted wallets now expose a 24-word English recovery phrase that reversibly represents the existing 256-bit QUINTUM RecoverySeed. The existing BIP32 path and derived addresses are unchanged.

Recovery validates the word list and checksum, discovers both receive and internal/change branches with a default gap limit of 100, rebuilds wallet history/index state from the authoritative active blockchain and commits encrypted `wallet.dat` only after successful recovery synchronization.

The phrase is available only through an explicit recovery API; it is never placed in normal runtime status or P2P data. Wallets containing legacy/imported random private keys continue to require a `wallet.dat` backup and do not claim complete seed-only recovery.

Next wallet work: automatic fee selection/relay policy and the remaining desktop-facing APIs before Qt GUI integration.
