# QUINTUM Android v2 — wallet and CPU mining architecture

Status: implementation branch, NOT production-ready. Existing RandomX Testnet consensus and data remain unchanged.

## Non-negotiable separation
- Wallet: locally generated encrypted keys, local signing, offline create/restore, backup verification. Never send seed/private keys to any backend.
- Light sync: authenticated and verifiable chain-derived data from independently operated full nodes. Never display unverified balance as confirmed or allow unverified spends.
- Mining: native RandomX 2.0.1 CPU hashing on Android; independently obtained block templates and consensus validation on QUINTUM full nodes. Found solutions are submitted and accepted only after full-node consensus checks.
- Full node: optional advanced mode. No requirement to download 4,000+ blocks before opening the wallet.
- Mining must not require wallet seed disclosure to a server. Rewards go to a locally selected payout address.
- Public RPC port 39445 must remain localhost-only. Any remote job endpoint needs separate authentication, rate limiting, TLS and replay protection.
- Preserve package identity, Android app data, existing wallet.dat, keys, network magic, genesis, addresses, RandomX version and testnet ports.

## Release gates
1. Android Gradle and NDK builds green, unit and device smoke tests green on ARM64.
2. Wallet create -> backup phrase -> verify -> app restart -> restore to isolated directory -> same address. Wrong password/phrase and existing wallet protection tested.
3. Light sync tests for malformed, stale, conflicting and fraudulent responses; zero trust in a single VPS.
4. Mining test: real RandomX hash, valid block template, accepted block and propagation across two nodes; thermal limits, user-controlled thread count, background lifecycle.
5. Two physical Android devices (Xiaomi and Huawei), repeated start/stop, network loss, reconnect, and app update preserving data.

No release APK should be described as verified until CI artifacts and on-device checks are available.
