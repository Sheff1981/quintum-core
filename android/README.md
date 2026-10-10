# QUINTUM Android

This directory is the Android application boundary for QUINTUM Core.

## Architecture

The Android application is intentionally a controller for the existing QUINTUM Core rather than a second blockchain implementation.

```
Jetpack Compose UI
        |
Kotlin application/services
        |
loopback JSON-RPC + per-run cookie authentication
        |
QUINTUM C++ Core process
        |
wallet / chainstate / P2P / RandomX
```

Consensus, transaction validation, wallet formats, P2P rules and RandomX remain owned by the C++ core.

## Security invariants

- RPC remains loopback-only.
- Authentication secrets are never compiled into the APK.
- Android UI code must not implement alternate consensus or transaction-validation rules.
- Private keys and recovery material must not be logged.
- Application upgrades must preserve the app identity and private data.
- Mining must run only through an explicit foreground service and user-visible controls.
- Android builds must not change Genesis, network magic, address prefixes, ports or monetary policy.

## Initial scope

The first Android milestone establishes a modern Kotlin/Compose shell and a typed boundary for the native node. Packaging the ARM64 QUINTUM daemon is a separate build step and is deliberately not faked with a placeholder binary.

The target product is a normal wallet/node when mining is disabled and a wallet/full-node/RandomX miner when mining is explicitly enabled.


## Updates

Android application identity is stable across releases. Wallet, node and settings data are private persistent application state and must survive normal package upgrades.

Direct Testnet APK distribution will use the authenticated release contract in [UPDATE_SECURITY.md](UPDATE_SECURITY.md). Store builds use the platform store update path. Consensus-critical code is never hot-swapped separately from a complete reviewed release.

## Mobile light-wallet direction (design gate; not implemented)

Default Android operation must not download or retain the full QUINTUM chain. Preserve the current full-node mode as an explicit developer option until a verified light-client protocol is implemented.

- Show **Wallet connections** (actual authenticated light-service connections) separately from **Network peers** (remote node-reported P2P count). Never label a syncing or initializing peer as an active peer.
- Show the source and verification status of chain height and synchronization. Never report 100% merely because one server says it is synced.
- Keep wallet secrets and transaction signing on device. Remote services must never receive seed phrases, private keys, or unrestricted spending RPC credentials.
- Specify authenticated chain identity, bounded responses, proof of transaction inclusion, reorg handling, server disagreement and malicious-server behavior before implementing balance/history APIs.
- Do not claim Bitcoin SPV-equivalent trust guarantees without independently verifying QUINTUM RandomX chainwork. Clearly disclose any server trust assumptions.
- Prefer multiple independently operated servers, with automatic discovery and reconnect; the project VPS must not be a mandatory central authority.
- Preserve existing app ID, wallet storage and recovery paths. Light-mode migration must be reversible and must not delete existing full-node chain data automatically.
- Gate light-wallet activation on integration tests for send/receive, invalid proofs, reorgs, disconnected servers, and app restart. Until then keep existing full-node behavior unchanged.

## Existing native wallet integration: implementation contract

Core already implements `wallet::Wallet::start(passphrase)`, `new_receive_address()`, `encrypt_wallet(passphrase)`, `recovery_mnemonic()`, `recover_from_seed()`, `recover_from_mnemonic(...)`, `backup_bundle()` and `restore_bundle()` in `src/wallet/wallet.hpp`. Android's `NativeCore.kt` currently exposes **none** of these methods, and `MainActivity.kt` has disabled setup buttons. Do not duplicate key derivation or cryptography in Kotlin.

Before enabling **Create wallet**, implement and test a JNI boundary that (1) detects existing `wallet.dat` without overwriting it; (2) obtains a user-chosen password through a non-logging UI; (3) creates and encrypts a wallet using native Core and fails closed if encryption or durable persistence fails; (4) returns a real testnet receive address only after successful persistence; (5) provides authenticated, user-confirmed recovery-phrase reveal and verification, without logging or exporting the phrase to diagnostics; (6) proves restart and backup/restore recovery in tests. No default hardcoded password or plaintext seed in preferences.

Before enabling **Restore wallet**, ensure that seed/key recovery and storage are independent of full-chain sync. `recover_from_mnemonic()` currently requires `Chainstate` and `Mempool`; separate key recovery from optional balance/history discovery with no change to the wallet format or derivation. Test restoring to an empty light client, a wrong network, malformed phrases, pre-existing wallets and interrupted writes. Only then connect to verified light-client network data.

Network and wallet creation must be independent: a user may create and back up an encrypted wallet while offline; balances remain unverified until the light-client protocol is ready. Do not enable UI buttons before the JNI and recovery tests pass.
