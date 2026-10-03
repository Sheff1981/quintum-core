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

**Recovery UX implemented in Stage 23:** 24-word English recovery representation of the existing 256-bit RecoverySeed, checksum validation, gap-aware receive/change discovery and atomic recovery commit without changing the existing BIP32 path or addresses.

**Send policy implemented in Stage 24:** shared wallet/node fee math, local minimum-relay policy, automatic fee quote/selection from exact P2PK size plus current mempool median, explicit proof that the relay floor is not consensus, and desktop-facing quote/auto-send APIs. Before public release: historical/confirmation-target fee estimation, labels/address book metadata and further adversarial/long-running wallet tests.

**Desktop wallet API implemented in Stage 25:** persistent address-book and transaction labels, network/wallet-bound crash-safe metadata storage, unified desktop snapshot, and a guarded preview/confirm send contract that rejects tampered or stale previews before transaction creation. Before public release: Qt presentation/recovery UX, privacy hardening for metadata, bundled backups, historical/confirmation-target fee estimation and further adversarial/long-running wallet tests.

## M5 — Desktop
**API foundation complete in Stage 25.** Stage 26 begins the Qt 6 GUI: Overview, Send, Receive, Transactions, synchronization state, peers and mining status.

## M6 — Windows distribution
Signed/reproducible release pipeline where possible, installer, safe upgrades, preserved wallet/blockchain data.

## M7 — Public testnet
Multiple geographically separate nodes, adversarial tests, reorgs, disconnect/reconnect, long-duration soak tests.

## M8 — Mainnet
Freeze consensus/network specification, generate and independently verify genesis, publish release hashes and documentation, then launch.
