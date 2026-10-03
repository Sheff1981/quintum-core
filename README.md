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

**M12 — Desktop wallet API foundation (Stage 25):** the core now exposes the stable data and confirmation model needed by a Qt desktop wallet without allowing the UI to bypass wallet, mempool or consensus validation.

Stage 25 adds durable address-book and transaction labels in a separate crash-safe `wallet_meta.dat`, bound to the QUINTUM network and an owned wallet-key anchor. Corrupt, wrong-network or wrong-wallet metadata fails explicitly instead of silently discarding user labels. Addresses are canonicalized before storage.

The desktop send flow is now two-step: `preview_send()` returns destination, amount, automatic fee quote, optional recipient label, a node-state hash and a tamper-evident preview id. `confirm_send()` revalidates the preview under the runtime lock and rejects a changed chain/mempool as `stale_preview` before any transaction is created or broadcast.

`desktop_snapshot()` exposes one GUI-facing view of runtime/network status, balances, labeled transaction history and the address book.

Next: Stage 26 — the first Qt 6 desktop wallet shell using these APIs: Overview, Send, Receive, Transactions and live synchronization/network status.
