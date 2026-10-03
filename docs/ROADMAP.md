# QUINTUM Roadmap

## M0 — Foundation
C++23 build, repository structure, CI, specification, deterministic test harness.

## M1 — Local blockchain
Serialization, hashes, transactions, UTXO, blocks, PoW validation, chainstate, disk persistence, reorg tests.

## M2 — Development networks
Regtest and testnet parameters, unique genesis blocks, mining and restart/recovery testing.

## M3 — P2P
Handshake, peer database, seeds, address relay, headers-first synchronization, block and transaction propagation.

## M4 — Wallet
**Core implemented in Stage 20:** OS-CSPRNG key generation, network-specific address encoding, receive/send, explicit fees, coin selection, signing, balances and keypool-based backup/recovery.

**Hardening implemented in Stage 21:** backward-compatible encrypted `wallet.dat` v2, Argon2id + XChaCha20-Poly1305, explicit v1 migration, BIP32 deterministic receive/change derivation and seed recovery foundation. Before public release: user-facing mnemonic/recovery UX, persistent transaction history/indexing, fee policy/estimation and further adversarial recovery testing.

**State/indexing implemented in Stage 22:** persistent confirmed/unconfirmed/inactive transaction history, restart-safe incremental wallet indexing with automatic corruption/reorg/key-set rebuild, and local size/mempool-based fee-policy foundation. Before public release: user-facing mnemonic/recovery UX, confirmation-target fee selection, labels/address book metadata and further adversarial/long-running recovery tests.

## M5 — Desktop
Qt 6 GUI, synchronization state, peers, balances, transaction history and mining status.

## M6 — Windows distribution
Signed/reproducible release pipeline where possible, installer, safe upgrades, preserved wallet/blockchain data.

## M7 — Public testnet
Multiple geographically separate nodes, adversarial tests, reorgs, disconnect/reconnect, long-duration soak tests.

## M8 — Mainnet
Freeze consensus/network specification, generate and independently verify genesis, publish release hashes and documentation, then launch.
