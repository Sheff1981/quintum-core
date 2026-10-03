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
**Qt shell implemented in Stage 26:** optional Qt 6 Widgets target `QUINTUM` with Overview, Send, Receive and Transactions pages, password-protected wallet startup, live balance/block/peer/mempool refresh, guarded Preview -> Confirm sending, and Linux/Windows GUI build + live runtime smoke CI.

**Operational desktop implemented in Stage 27:** first-launch Create/Recover flow with 24 words, password-gated seed reveal, peer-target synchronization status, persistent address-book editing, real wallet-directed PoW mining with measured hash rate, Settings/recovery/backup controls, and user-facing error mapping. Runtime recovery refuses to overwrite an existing wallet, safely rebinds orphaned metadata after recovery, and the new regression suite verifies recovery/restart, password checking, real two-node peer-height synchronization and wallet-owned mining.

**Release hardening implemented in Stage 28:** authenticated-encrypted wallet metadata v2 with legacy migration, complete network-bound `.qtmbackup` backup/restore, current-user-only Windows wallet/metadata ACLs, Qt runtime deployment and release checksums.

## M6 — Windows distribution
**Installer pipeline implemented in Stage 28:** per-user Inno Setup installer, portable Qt deployment, Start Menu/optional desktop shortcut, live-process update closure, safe reinstall/update, preserved AppData wallet/blockchain data on update/uninstall, and CI-published Windows artifacts. Production code-signing remains pending a real signing certificate.

## M7 — Public testnet
**Stage 29 live deployment verified:** the first public Testnet seed `212.193.15.139:38444` is deployed as a persistent Ubuntu/systemd node; a fresh Windows installer defaults to Testnet, discovers the seed without manual IP configuration, completes a live peer connection, mines real PoW blocks and relays them over the public Internet. Two Windows-mined blocks were independently accepted and durably persisted by the VPS; both machines reached height 2 and the VPS recovered the same height from disk after reopening the datadir.

Remaining Testnet work: additional independent/geographically separate nodes, DNS seeds, live wallet transaction/confirmation testing, public multi-node fork/reorg testing, disconnect/reconnect soak, NAT traversal and longer adversarial operation.

## M8 — Mainnet
Freeze consensus/network specification, generate and independently verify genesis, publish release hashes and documentation, then launch.
