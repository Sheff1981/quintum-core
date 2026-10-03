# QUINTUM Desktop Wallet

Status: **pre-alpha**

Stage 26 introduces the first Qt 6 Widgets desktop shell. The GUI is deliberately a thin client over `NetworkRuntime`; it does not implement consensus, wallet signing, fee policy or P2P rules itself.

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

The GUI never receives or stores private keys directly. Receive-address generation, signing, transaction creation, balances, history and metadata all go through the existing wallet/runtime APIs.

## Pages

### Overview

Shows available, confirmed, pending and immature balances plus local block height, peer count and mempool size. The view refreshes from `desktop_snapshot()` once per second.

### Send

The user enters destination, amount and an optional label. The GUI calls `preview_send()`, displays the exact automatic fee and total, then calls `confirm_send()` only after explicit confirmation. If chain or mempool state changed between preview and confirmation, the runtime rejects the stale preview and the GUI requires a fresh review.

### Receive

Shows the current receive address, supports clipboard copy, and requests a new address through the existing persistent keypool.

### Transactions

Shows status, txid, received/spent values, fee, confirmations and persistent user labels.

## CI smoke mode

`--smoke-test` is for CI/development only. It creates a disposable encrypted Regtest wallet in a temporary directory, starts a real node runtime and MainWindow, then exits automatically. CI runs this mode headlessly on Linux and Windows.

## Not finished yet

Stage 26 is the first functional shell, not the release UI. Before Windows distribution the desktop layer still needs:

- 24-word recovery display/import workflow;
- richer synchronization progress/state;
- address-book editing UI;
- mining/status controls;
- settings;
- improved user-facing error mapping;
- Qt runtime deployment and Windows installer;
- release signing/update policy.
