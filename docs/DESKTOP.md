# QUINTUM Desktop Wallet

Status: **pre-alpha**

Stage 26 introduced the first Qt 6 Widgets desktop shell. Stage 27 made it operational. Stage 28 hardens wallet privacy/backup and produces the first tested Windows distribution. The GUI remains a thin client over `NetworkRuntime`; it does not implement consensus, wallet signing, fee policy, recovery derivation, mining validation or P2P rules itself.

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

When no `wallet.dat` exists, the startup UI offers **Create new wallet**, **Recover from 24 words**, or **Restore backup** from a complete `.qtmbackup`. Recovery is executed by `NetworkRuntime::start()` before normal startup, uses the existing gap-aware deterministic recovery path, and refuses to overwrite an existing wallet file. After a successful recovery, orphaned metadata from a previously removed wallet is atomically replaced by an empty metadata store bound to the recovered wallet. Invalid word count, unknown words and checksum/order errors are reported separately.

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
- **Backup complete wallet** creates one `.qtmbackup` containing `wallet.dat` and wallet metadata.

The backup bundle is checksummed and network-bound. It intentionally excludes blockchain data and the rebuildable `wallet_state.dat`; both can be reconstructed by synchronization/rescan. For normal encrypted wallets, the bundle contains encrypted wallet secrets plus encrypted metadata. Before seed words are revealed, the password is re-derived with the wallet's real Argon2id parameters and compared against the active encryption key without early exit. Wrong passwords never request the mnemonic from the wallet. Stage 28 will replace the backup limitation with a complete backup bundle.

## CI smoke mode

`--smoke-test` is for CI/development only. It creates a disposable encrypted Regtest wallet in a temporary directory, starts a real node runtime and MainWindow, then exits automatically. CI runs this mode headlessly on Linux and Windows.

## Windows distribution

Stage 28 CI creates a portable deployment with Qt runtime DLLs/plugins using `windeployqt`, then compiles a per-user Inno Setup installer.

The installer:
- installs under the current user's LocalAppData Programs directory and does not require administrator privileges;
- creates a Start Menu shortcut and offers an optional desktop shortcut;
- uses a stable application id for upgrades;
- closes a running `QUINTUM.exe` during update through the Windows Restart Manager path;
- never installs blockchain/wallet data under `{app}`;
- never deletes the user's AppData wallet/blockchain directory on update or uninstall.

CI performs a real silent first install, launches the installed executable, starts a persistent wallet process, performs an update, verifies the old process was closed, uninstalls, and verifies a user-data sentinel survived both update and uninstall.

The Windows artifact contains:
- `QUINTUM-Core-Setup-0.0.1-prealpha-x64.exe`;
- the portable `windows/` deployment directory;
- `SHA256SUMS.txt`.

The pre-alpha installer is currently unsigned. Production signing is deferred until a real code-signing certificate is provisioned.

## Remaining pre-mainnet work

- public seed/bootstrap infrastructure and geographically separate nodes;
- long-duration network/reorg/disconnect soak tests;
- confirmation-target fee estimator;
- external Windows installation testing across supported machines;
- release signing/update policy and signing certificate;
- public testnet release candidate before any mainnet parameter freeze.


## Stage 30 — Bitcoin Core interaction parity

The desktop shell now follows the proven Bitcoin Core window structure more closely without copying Bitcoin branding or network identity:

- top-level **File / Settings / Window / Help** menus;
- horizontal wallet navigation for **Overview / Send / Receive / Transactions**, with QUINTUM's **Mining** action retained;
- Bitcoin-style Window shortcuts for **Information**, **Console**, **Network Traffic** and **Peers**;
- a tabbed **Debug window** backed by current QUINTUM runtime state; unsupported RPC/byte-counter functions are explicitly identified rather than simulated;
- first-run **Welcome to QUINTUM Core** data-directory chooser with default/custom locations persisted per network.

This is presentation and operator UX only. Consensus, Genesis, network magic, ports, address encoding, wallet formats and chain data remain unchanged.
