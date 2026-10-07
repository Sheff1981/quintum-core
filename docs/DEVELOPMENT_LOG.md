# QUINTUM Development Log

This is the canonical English index for QUINTUM development history.

The former Russian narrative log duplicated information that is already preserved in the repository's immutable Git history and in the English technical documentation. To avoid maintaining two divergent histories, QUINTUM documentation is English-only from this point forward.

## Authoritative history sources

- [Exchange Development History](exchange/DEVELOPMENT_HISTORY.md) — commit-level chronological development history.
- [Roadmap](ROADMAP.md) — milestone and Stage status.
- [Architecture](ARCHITECTURE.md) — component boundaries and architecture.
- [Consensus](CONSENSUS.md) — consensus rules and validation boundaries.
- [Chain Parameters](CHAIN_PARAMS.md) — network-specific parameters.
- [Proof of Work](POW.md) and [Difficulty](DIFFICULTY.md) — mining PoW and difficulty.
- [Chainstate](CHAINSTATE.md), [Storage](STORAGE.md), and [Forks/Reorg](FORKS_REORG.md) — durable blockchain state and reorganization behavior.
- [P2P](P2P.md) and [Node Runtime](NODE_RUNTIME.md) — networking, synchronization, relay and runtime behavior.
- [Wallet](WALLET.md) — wallet, recovery, encryption, backup and metadata.
- [Desktop](DESKTOP.md) and [Windows](WINDOWS.md) — GUI and Windows distribution.
- [Testnet Testing](TESTNET_TESTING.md) — public Testnet procedures and evidence.

## Development chronology

### 2026-10-02 — Foundation and local blockchain

Stages 0 onward established the C++23 repository and CI foundation, deterministic serialization and SHA-256 primitives, transactions, UTXO accounting, double-spend protection, blocks and Merkle validation, Proof of Work, chainwork, chainstate, block undo, forks/reorganizations, network parameters, Genesis construction, persistent blockchain storage, node/mining integration and the initial P2P stack.

The detailed implementation history and exact commits are preserved in [Exchange Development History](exchange/DEVELOPMENT_HISTORY.md). Current normative behavior is documented by the technical documents linked above.

### Wallet and desktop stages

Stage 20 implemented wallet core: OS-CSPRNG keys, network-specific addresses, receive/send, explicit fees, coin selection, signing, balances and keypool backup/recovery.

Stage 21 added backward-compatible encrypted wallet storage using Argon2id and XChaCha20-Poly1305 plus deterministic BIP32 recovery foundations.

Stage 22 added persistent wallet transaction state/indexing and restart-safe rebuild behavior.

Stage 23 added 24-word English recovery representation, checksum validation and gap-aware recovery.

Stage 24 unified wallet/node fee policy and guarded automatic fee selection.

Stage 25 added the desktop wallet API, persistent labels/address book metadata and preview/confirm send protection.

Stage 26 introduced the Qt 6 desktop shell.

Stage 27 added first-launch create/recover UX, synchronization status, mining status, address-book operations and runtime recovery protections.

Stage 28 hardened encrypted metadata, complete wallet backup/restore, Windows ACL handling, Qt deployment and the per-user Windows installer/update pipeline.

### Public Testnet and network hardening

Stage 29 established the public Testnet deployment and verified automatic Windows-to-seed connectivity, real PoW mining, Internet block relay and durable restart.

Stage 31 added compact block relay with bounded reconstruction and legacy full-block compatibility.

Stage 32 added negotiated encrypted transport using ephemeral secp256k1 ElligatorSwift ECDH, HKDF-SHA256 and ratcheted ChaCha20-Poly1305.

Stage 33 added typed peer addresses, addrv2, Tor/I2P SOCKS5 dialing and best-effort NAT-PMP/UPnP reachability.

Stage 34 added Dandelion-style private transaction stem relay with bounded fallback behavior.

Stages 39–41 hardened consensus-valid headers-first synchronization, bounded block/transaction in-flight requests and reconnect behavior.

Stage 42 implemented validated fast RandomX restart.

Stage 43 added encrypted-wallet spend reauthentication.

Stage 44 made complete wallet backup creation verify the final atomic write by reading it back before reporting success.

Stage 45 hardened restore behavior, including fail-closed handling of existing/orphan wallet metadata and rollback-safe verification.

Stage 46 added a bounded runtime-only negative cache for exact context-independent invalid blocks; announced hashes cannot poison the cache.

Stage 47 verified validation-before-relay behavior and added live malformed-message peer-disconnect regression coverage.

Stage 48 added a dedicated bounded per-peer compact-block reconstruction budget while preserving existing custom block-request configurations.

Stage 49 made CompactSize parsing transactional: malformed, truncated or non-canonical encodings fail without partially advancing the caller's cursor.

## Stage 49 verification

PR #39 passed Build, Security and GUI CI before merge. It was merged into `main` as `cf960e6165f0d73434a2f8dbf766a2e0ee7fd87d`.

Successful serialization formats, Genesis, RandomX, difficulty, monetary policy, wallet/backup formats, chain data, network magic, addresses and ports were unchanged.

## Current hardening sequence

The remaining M7 safety sequence is:

1. Stage 50 — fuzz/adversarial parser and malformed-input coverage.
2. Stage 51 — crash/restart consistency.
3. Stage 52 — multi-node partition/reorg soak.

Each stage must pass the applicable Build, Security and GUI CI gates before merge.

## Stage 50 — Adversarial parser corpus

Stage 50 adds deterministic malformed-input coverage to the existing CI suite without introducing a production dependency or changing serialization.

The P2P wire tests now exercise every strict prefix of a valid frame, concatenated-frame consumption, and independent network-magic corruption. Compact-block tests exercise every strict prefix of valid compact-block, getblocktxn and blocktxn payloads, trailing garbage rejection, and non-canonical CompactSize rejection before allocation.

These tests are intended to prove fail-closed behavior at network-controlled parser boundaries under the normal Build/Security/GUI CI matrix. Consensus rules, successful serialization, RandomX, Genesis, monetary policy, wallet formats, network identity, addresses and ports are unchanged.

## Stage 51 — Crash/restart consistency

Stage 51 extends persistent-chainstate recovery coverage with an explicit interrupted-snapshot crash point. The test writes a partial `chainstate.dat.tmp` beside a valid committed snapshot, restarts the node, verifies that the committed height/tip/UTXO state remains authoritative, continues by committing another block, and verifies a second clean restart.

This complements the existing coverage for uncommitted block tails, truncated committed blocks, side-branch/undo restart, pruning restart and corruption rejection. The storage format and consensus rules are unchanged.

## Documentation policy

QUINTUM technical documentation is maintained in English only. New implementation stages must update the relevant English normative document and this development index when the change is historically significant. Git history remains the source of truth for exact code changes and commit chronology.
