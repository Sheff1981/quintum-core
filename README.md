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

**M8 — Wallet hardening (Stage 21):** `wallet.dat` v2 encrypts the recovery seed, private keys and keypool metadata with XChaCha20-Poly1305. The password key is derived with Argon2id (64 MiB, 3 passes), and every rewrite uses a fresh random nonce. The authenticated open header binds the wallet version, network and KDF parameters.

New password-created wallets are deterministic. Key derivation uses BIP32 CKDpriv mechanics (HMAC-SHA512 + libsecp256k1 tweak-add) on the QUINTUM path `m/5329997'/network'/branch/index`.

Legacy Stage 20 v1 wallets remain readable and can be explicitly migrated without changing addresses or private keys. A migrated/imported-key wallet still requires a `wallet.dat` backup; seed-only recovery is exposed only when every wallet key is deterministic.

The wallet remains on the same P2PK/UTXO/mempool/consensus path. Stage 21 changes wallet storage and recovery only; Genesis, PoW, monetary policy, network magic and transaction consensus are unchanged.

Next wallet work: persistent transaction history/indexing and fee policy/estimation, followed by desktop GUI/release hardening.
