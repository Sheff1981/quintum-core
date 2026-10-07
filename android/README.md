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
