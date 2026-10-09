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
