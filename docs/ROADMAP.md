# QUINTUM Roadmap

## M0 — Foundation
C++23 build, repository structure, CI, specification, deterministic test harness.

## M1 — Local blockchain
Serialization, hashes, transactions, UTXO, blocks, PoW validation, chainstate, disk persistence, reorg tests.

## M2 — Development networks
Regtest and testnet parameters, unique genesis blocks, mining and restart/recovery testing.

## M3 — P2P
Handshake, peer database, seeds, address relay, headers-first synchronization, block and transaction propagation.

**Compact block relay implemented in Stage 31:** capable peers negotiate a dedicated service bit, keep normal block inventory announcements for backward compatibility, reconstruct announced blocks from 48-bit SipHash short transaction IDs plus a prefilled coinbase, and request only missing transactions through bounded `getblocktxn/blocktxn`. Legacy peers continue to receive full blocks, and every reconstructed block still enters the normal persistent consensus-validation path.

**Encrypted transport implemented in Stage 32:** capable peers upgrade after `version/verack` using ephemeral secp256k1 ElligatorSwift ECDH, network-domain-separated HKDF-SHA256 directional keys, encrypted key confirmation and ratcheted ChaCha20-Poly1305 AEAD packets. Legacy peers remain compatible; authentication protects the negotiated session and packet integrity rather than asserting a permanent peer identity.

**Network reachability implemented in Stage 33:** addrman now migrates legacy `peers.dat` v1 to typed v2 records, capable peers exchange IPv4/Tor v3/I2P endpoints over negotiated `addrv2`, Tor/I2P dial through SOCKS5 without local DNS resolution, and ordinary public runtimes attempt best-effort NAT-PMP/UPnP inbound port mapping with clean shutdown and non-fatal fallback. Legacy IPv4 `addr` peers remain compatible.

**Private transaction relay implemented in Stage 34:** locally originated transactions use a negotiated Dandelion-style stem phase through one capable outbound route before ordinary inventory diffusion. Epoch route rotation, loop-to-fluff handling, randomized embargo fallback, stem-phase mempool/getdata suppression and a dedicated stem message-rate ceiling preserve liveness and bound abuse. Legacy peers keep the existing `inv/getdata/tx` path.

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

Remaining Testnet work: additional independent/geographically separate nodes, DNS seeds, live wallet transaction/confirmation testing, public multi-node fork/reorg testing, disconnect/reconnect soak and longer adversarial operation.

**Live hardening Stages 39–43:** consensus-valid headers-first synchronization, bounded block/transaction in-flight requests, exponential reconnect backoff, validated fast RandomX restart and encrypted-wallet spend reauthentication are implemented on staged branches and covered by Build/Security/GUI CI before integration.

**Stage 44 backup durability hardening:** complete `.qtmbackup` creation now verifies the finished atomic write by reading the destination back and requiring an exact byte-for-byte match before reporting success. No consensus, wallet-format or network-identity changes.

## M8 — Mainnet
Freeze consensus/network specification, generate and independently verify genesis, publish release hashes and documentation, then launch.


## M7 hardening backlog — external mature-node audit (2026-10-07)

The following pre-Mainnet work is adopted from concrete failure classes observed in mature PoW node implementations. These are engineering patterns, not consensus imports; QUINTUM network identity, RandomX rules, monetary policy and existing serialization remain unchanged unless a separately reviewed consensus proposal explicitly requires otherwise.

- **Invalid-object cache/body-poisoning safety:** cache only exact validated objects and only context-independent failures; never let an announced/header hash suppress a later valid body. Stage 46 is implementing this conservatively.
- **Validate before fast relay/persistence side effects:** a block must complete the appropriate validation/connection boundary before relay/cache state can make it authoritative.
- **Malformed-message peer accountability:** malformed consensus/P2P payloads must consume bounded resources and result in deterministic disconnect/penalty rather than unlimited retry.
- **Per-peer admission/resource caps:** inventory, mempool admission, block/transaction requests and gossip queues need independent bounded limits so one connection cannot monopolize global work.
- **Parser/serialization fallibility:** no network-controlled parse or re-serialization path may be assumed infallible; malformed inputs must return errors rather than terminate the process.
- **Fuzz/adversarial coverage:** add fuzz targets for P2P framing/messages, transaction/block parsing and serialization, compact-block reconstruction, wallet backup/restore parsing and persistent-state decoding.
- **Crash/restart consistency:** exercise forced termination around block/chainstate/wallet writes and prove restart yields a valid old or new state, never a partially committed hybrid.
- **Multi-node partition/reorg soak:** repeatedly partition/reconnect miners and wallets and verify greatest-cumulative-work convergence, UTXO rollback, mempool reconciliation, wallet confirmations and durable restart recovery.

These items are release gates for production maturity, not justification to delay normal Testnet UX work once the current safety stage is green.
