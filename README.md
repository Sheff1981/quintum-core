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

**M11 — Automatic fee + relay policy (Stage 24):** wallet sends now support an automatic fee mode built on one shared fee-rate calculation used by both wallet policy and node relay policy.

The default wallet and minimum-relay rate are currently **1,000 atomic units per 1,000 serialized bytes**. Auto mode estimates the signed P2PK transaction size from its selected UTXOs, uses the greater of the wallet default, the node relay floor and the current mempool median rate, and produces a fee quote before sending.

Transactions below the local relay floor are rejected from the mempool, but that rule is deliberately **not consensus**: an otherwise-valid low-fee transaction can still be valid inside a block. `NetworkRuntime` exposes fee quote, minimum relay rate, recommended rate and auto-send APIs for the future desktop wallet. The development CLI uses Auto when `--fee` is omitted and preserves explicit fee override.

Next wallet work: stronger confirmation-target fee estimation, transaction metadata/address book and remaining desktop-facing APIs before Qt GUI integration.
