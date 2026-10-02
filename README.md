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
- Durable wallet key storage with atomic replacement and backup support
- Qt 6 desktop wallet planned after the node and network are stable

## Repository map

- `src/` — node/core source code
- `docs/` — protocol and architecture documentation
- `docs/ru/START_HERE.md` — plain-language Russian project guide
- `.github/workflows/` — reproducible CI builds

## Development rule

Consensus-critical constants are **DRAFT** until the genesis block and mainnet specification are deliberately frozen. After mainnet launch, incompatible consensus changes require explicit network-upgrade rules.

## Current milestone

**M7 — Wallet Core:** `quintumd` now owns a persistent QUINTUM wallet in addition to the continuous P2P node. The wallet creates private keys from the operating-system CSPRNG, derives network-specific Bech32m receive addresses, tracks wallet-owned active-chain/mempool outputs, reports confirmed/available/pending/immature balances, constructs and signs spends, handles change, and supports durable `wallet.dat` backup/recovery.

The wallet uses the same P2PK authorization, UTXO and mempool validation paths as the rest of the node. A locally created transaction gets no consensus privilege: it must validate normally before it is relayed.

A pre-generated receive/change keypool is persisted before use so an older wallet backup can recover a bounded set of future addresses. Imported keys and keypool refill explicitly require a fresh backup.

**Security limitation:** current `wallet.dat` is checksummed and crash-safe but is not yet password-encrypted at rest. QUINTUM remains pre-mainnet and must not be treated as production-money software until wallet encryption/HD recovery and further hardening are completed.

The public QUINTUM seed list is intentionally empty until real independent seed infrastructure exists; no fake or developer-only endpoint is embedded.

Next: **M8 — Wallet hardening:** encrypted key storage, deterministic/HD recovery, persistent transaction history, fee policy/estimation and incremental wallet indexing before GUI/release work.
