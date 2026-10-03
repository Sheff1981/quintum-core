# QUINTUM Core

QUINTUM is an independent proof-of-work cryptocurrency and peer-to-peer network being built from the ground up in modern C++23.

> Status: **pre-alpha / protocol design**. No mainnet exists yet. Coins created in development networks have no monetary value.

## Principles

- Independent network and genesis block
- Proof of Work
- UTXO accounting model
- Fully validating nodes
- Peer-to-peer block and transaction relay
- No premine, hidden mint, master key, or developer backdoor
- Private keys remain under the user's control
- Windows-first desktop experience, with portable core architecture
- Consensus rules are documented and tested before mainnet launch

## Technology

- C++23
- CMake
- secp256k1 for transaction signatures
- SHA-256 family cryptographic hashing
- Durable restart-safe blockchain storage
- Authenticated encrypted wallet storage with Argon2id + XChaCha20-Poly1305
- BIP32 deterministic recovery with 24-word English recovery phrases and atomic restoration
- Qt 6 Widgets desktop wallet shell, kept separate from the consensus/core library

## Repository map

- `src/` — node/core source code
- `docs/` — protocol and architecture documentation
- `docs/ru/START_HERE.md` — plain-language Russian project guide
- `.github/workflows/` — reproducible CI builds

## Development rule

Consensus-critical constants are **DRAFT** until the genesis block and mainnet specification are deliberately frozen. After mainnet launch, incompatible consensus changes require explicit network-upgrade rules.

## Current milestone

**M16 — Public testnet readiness (Stage 29, deployment gate):** the code-side Testnet path is now exercised as the normal desktop network, with hostname-capable seed bootstrap and multi-node adversarial regression coverage.

Implemented in Stage 29:
- a normal `QUINTUM.exe` launch selects **Testnet**; Regtest remains explicit and isolated;
- Testnet/Regtest window titles are clearly labelled to prevent accidental network confusion;
- seed endpoints may be literal IPv4 addresses or DNS hostnames; all resolved IPv4 addresses pass through the normal addrman validation path;
- successful peers keep a short in-process reuse cooldown, but a clean application restart makes known-good persisted peers immediately eligible again;
- the Stage 29 regression suite boots real Testnet parameters and verifies persisted peer reconnect without re-entering an address;
- a three-node partition test mines competing branches, reconnects them, verifies heavier-chain reorg, syncs a third node through an intermediate peer, restarts from `peers.dat`, relays a new block over two hops and reopens all three chainstates on the same tip.

QA at code commit `3495c4901af23319d4e337ccdbd5cc92dcf454c1`:
- Linux core: **27/27 passed**;
- Windows core: **27/27 passed**;
- Linux/Windows GUI + Windows installer/deployment workflow: **success**.

No consensus constants, Genesis blocks, network magic, ports, address formats, PoW, difficulty, subsidy, blockchain format or wallet key derivation were changed.

The remaining Stage 29 deployment gate is intentionally external: provision at least one stable publicly reachable Testnet seed on TCP 38444, pin its real IP/DNS endpoint, then run geographically separate Windows/Linux soak testing. The built-in public seed list remains empty until such an endpoint actually exists.
