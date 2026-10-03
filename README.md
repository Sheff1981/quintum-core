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

**M14 — Operational Qt desktop wallet (Stage 27):** the desktop application now covers the first complete day-to-day wallet loop without moving security-sensitive logic into Qt.

On first launch the user can either create a new encrypted wallet or recover one from the existing QUINTUM **24-word recovery phrase**. Recovery is performed by `NetworkRuntime` against the real chain before normal wallet startup, preserves the existing BIP32 derivation, rescans with the Stage 23 gap policy, and refuses to overwrite an existing `wallet.dat`.

The GUI now includes **Overview, Send, Receive, Transactions, Address Book, Mining and Settings**. Overview shows local height, peer best height and synchronization progress. Address-book edits persist through `wallet_meta.dat`. Mining performs real PoW through the node runtime, pays coinbase to a wallet-owned key, reports measured hash attempts/rate, submits valid blocks locally and relays them to peers.

Recovery words can be revealed only after an explicit warning **and successful re-entry of the encrypted-wallet password**. Password verification reuses the wallet's Argon2id parameters and a full fixed-length key comparison. Temporary desktop password/mnemonic buffers are best-effort erased after use. Successful mnemonic recovery also replaces orphaned metadata with a new wallet-bound empty metadata store, so stale labels from a removed wallet cannot break the next restart. Send/recovery/mining errors are mapped to user-facing messages instead of generic failures.

Stage 27 adds a 25th regression suite covering runtime mnemonic recovery, password verification, non-overwrite safety, orphaned-metadata recovery/restart, real two-node peer-height synchronization and wallet-directed mining. The final code passed **25/25 core suites on Linux and Windows**, plus Qt build and live runtime smoke on both platforms.

Next: **Stage 28 — wallet release hardening:** encrypt privacy-sensitive metadata, create a complete backup bundle, finish Windows file ACL handling, and harden shutdown/restart behavior before Windows packaging/installer.
