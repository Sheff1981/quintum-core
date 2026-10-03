# QUINTUM Desktop Wallet

Status: **pre-alpha**

Stage 26 introduced the first Qt 6 Widgets desktop shell. Stage 27 makes it operational. The GUI remains a thin client over `NetworkRuntime`; it does not implement consensus, wallet signing, fee policy, recovery derivation, mining validation or P2P rules itself.

## Build

The normal node/core build does not require Qt.

To build the desktop wallet, provide Qt 6.8+ and configure:

```text
cmake -S . -B build-gui -DQUINTUM_BUILD_GUI=ON -DCMAKE_PREFIX_PATH=<Qt-prefix>
cmake --build build-gui --config Release --target quintum_qt
```

The desktop executable is named `QUINTUM` / `QUINTUM.exe`.

CI pins Qt **6.8.0** for reproducibility and builds the GUI separately on Linux and Windows.

## Networks and data directories

The GUI defaults to **Regtest** while QUINTUM is pre-mainnet.

Supported switches:

- `--regtest` — local regression network;
- `--testnet` — QUINTUM test network;
- `--mainnet` — pre-mainnet candidate parameters;
- `--datadir PATH` — explicit data directory.

Without `--datadir`, Qt's application-data directory is used with a separate subdirectory for each network.

## Wallet startup

A newly created desktop wallet requires a password and uses the existing encrypted `wallet.dat` implementation. Existing legacy unencrypted development wallets may still be opened with an empty password.

When no `wallet.dat` exists, the startup UI offers **Create new wallet** or **Recover from 24 words**. Recovery is executed by `NetworkRuntime::start()` before normal startup, uses the existing gap-aware deterministic recovery path, and refuses to overwrite an existing wallet file. After a successful recovery, orphaned metadata from a previously removed wallet is atomically replaced by an empty metadata store bound to the recovered wallet. Invalid word count, unknown words and checksum/order errors are reported separately.

The GUI never receives or stores private keys directly. Receive-address generation, signing, transaction creation, balances, history, recovery derivation, mining and metadata all go through existing core/runtime APIs. Password and mnemonic handoff strings are best-effort erased after use.

## Pages

### Overview

Shows available, confirmed, pending and immature balances plus local block height, peer best height, peer count, mempool size and synchronization progress. With no peer target the UI shows **Waiting for peers** rather than a false 100%; once connected, progress is derived from the local height versus the best height reported/observed from live peers. The view refreshes from `desktop_snapshot()` once per second.

### Send

The user enters destination, amount and an optional label. The GUI calls `preview_send()`, displays the exact automatic fee and total, then calls `confirm_send()` only after explicit confirmation. If chain or mempool state changed between preview and confirmation, the runtime rejects the stale preview and the GUI requires a fresh review.

### Receive

Shows the current receive address, supports clipboard copy, and requests a new address through the existing persistent keypool.

### Transactions

Shows status, txid, received/spent values, fee, confirmations and persistent user labels.

### Address Book

Lists persistent address labels from `wallet_meta.dat`. Entries can be added, updated and removed. Address/network validation remains in wallet core.

### Mining

The desktop mining control calls `NetworkRuntime::mine_wallet_block()` in bounded PoW batches. Payout is constructed from a wallet-owned receive key. The displayed hash rate is measured from real attempted hashes. A found block must pass normal local chain validation/storage before it counts and is then announced through the existing P2P block relay path.

The current desktop miner is intentionally simple and single-process; it is a correctness/reference miner, not yet an optimized multi-threaded production miner.

### Settings

Shows network, P2P/listen ports and recommended fee rate. It also exposes:

- **Show 24 recovery words** with an explicit secret warning and mandatory wallet-password re-verification;
- **Backup wallet.dat** for spend-key backup.

The current single-file backup does **not** include `wallet_meta.dat`; the UI states this explicitly. Before seed words are revealed, the password is re-derived with the wallet's real Argon2id parameters and compared against the active encryption key without early exit. Wrong passwords never request the mnemonic from the wallet. Stage 28 will replace the backup limitation with a complete backup bundle.

## CI smoke mode

`--smoke-test` is for CI/development only. It creates a disposable encrypted Regtest wallet in a temporary directory, starts a real node runtime and MainWindow, then exits automatically. CI runs this mode headlessly on Linux and Windows.

## Not finished yet

Stage 27 completes the functional desktop loop, but this is still pre-alpha. Before Windows distribution/release:

- encrypt privacy-sensitive `wallet_meta.dat` at rest;
- create one complete backup/restore bundle covering keys plus user metadata;
- finish explicit Windows wallet/metadata ACL hardening;
- strengthen clean-shutdown/restart and desktop soak tests;
- deploy Qt runtime files with the Windows package;
- build the Windows installer with safe upgrades and preserved user data;
- define release signing/update policy;
- later replace the reference desktop miner with optimized mining infrastructure only if needed.
