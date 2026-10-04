# QUINTUM Verification Log

Status: **LIVING EVIDENCE LOG**  
Technical state covered: `c5e29dcf45375d227872dd1a21c9127b214b1c8d`

This log records externally meaningful validation. It is not a substitute for the full Git commit history.

## 2026-10-02 — Foundation through full local node architecture

The project progressed from foundation types through transaction/UTXO validation, block structure, real PoW, chainstate, forks/reorgs, monetary rules, secp256k1 authorization, network parameter sets, difficulty/timestamp/resource rules, deterministic Genesis, persistent storage, node runtime/mining, P2P, peer discovery, headers-first synchronization, block/transaction relay, mempool and continuous networking.

Linux and Windows CI were used throughout the build sequence.

## 2026-10-03 — Wallet, recovery, desktop and distribution

Implemented and regression-tested:

- encrypted wallet storage;
- BIP32 deterministic recovery foundation;
- 24-word recovery flow;
- persistent wallet history;
- fee policy;
- desktop wallet API;
- Qt 6 desktop application;
- operational send/receive/history/address-book/mining/settings flows;
- authenticated encrypted wallet metadata;
- complete network-bound `.qtmbackup`;
- Windows per-user installer;
- update/uninstall user-data preservation.

## 2026-10-03 — First public Testnet node

Public node:

`212.193.15.139:38444`

Deployment facts recorded by the project:

- Ubuntu 24.04 LTS VPS;
- persistent `quintumd` systemd service;
- public TCP 38444;
- dedicated service user;
- durable Testnet datadir;
- automatic restart-on-failure.

External mobile-network TCP reachability was verified.

## 2026-10-03 — Fresh Windows bootstrap

A fresh Windows desktop install:

- started on Testnet by default;
- used the hardcoded public seed;
- required no manual peer IP entry;
- completed `version/verack`;
- showed one live peer;
- reported node running.

## 2026-10-03 — Real public-network mining and relay

The Windows reference miner performed real Proof of Work at roughly 37 kH/s and found two valid Testnet blocks.

Observed:

- local height advanced to 2;
- 100 QTM appeared as immature coinbase;
- remote VPS accepted both blocks;
- blocks were durably present after the VPS node was stopped and reopened from the same datadir.

No manual block copying or consensus bypass was used.

## 2026-10-03 — Reconnect verification

After temporary peer loss, the Windows node automatically reconnected through the normal retry/reconnect path without manual peer injection.

Observed after recovery:

- local height 2;
- peer best height 2;
- peers 1;
- synchronization up to date;
- progress 100%.

## 2026-10-03 — Repeated relay through height 7

Mining continued on Windows. The public VPS was reopened against the same Testnet datadir and reported height 7, confirming repeated public P2P relay and durable remote acceptance.

## 2026-10-04 — Overnight persistence, reconnect, synchronization and maturity

A long-running Windows Testnet wallet/miner was left operating overnight without a software update or manual peer configuration.

By the morning the application reported:

- local height: 139;
- peer best height: 139;
- peers: 1;
- synchronization: up to date;
- available: 2,000.00000000 QTM;
- confirmed: 2,000.00000000 QTM;
- immature: 4,950.00000000 QTM.

The accounting matches 139 mined 50-QTM rewards under 100-block maturity:

- 40 matured rewards = 2,000 QTM;
- 99 immature rewards = 4,950 QTM;
- total = 6,950 QTM.

This verifies restart-safe wallet/chain persistence, reconnect, full height agreement and automatic maturity transition in a real Testnet run.

## 2026-10-04 — Bidirectional/equal-height/full-batch sync hardening

Live-network behavior led to additional synchronization hardening:

- initial chain catch-up no longer depends on which peer initiated the TCP connection;
- equal-height fork discovery is preserved;
- full 2,000-header continuation batches no longer terminate prematurely when an intermediate batch is already known on a side branch;
- header anchors are validated against the requested locator/continuation.

Current source head:

`c5e29dcf45375d227872dd1a21c9127b214b1c8d`

GitHub Actions evidence for this head:

- build run: https://github.com/Sheff1981/quintum-core/actions/runs/37181984429 — **success**
- GUI/Windows distribution run: https://github.com/Sheff1981/quintum-core/actions/runs/37181984433 — **success**

## Current Windows artifact recorded in this work session

Filename:

`QUINTUM-Core-Setup-0.0.1-prealpha-x64.exe`

SHA-256:

`de3f38a4c69c0455d4920e633c5b11b9cd7495d6fe3accce15faeb0965c15d9f`

Size:

`18,435,515 bytes`

Status: **unsigned pre-alpha**.

This hash identifies this exact artifact only. It must not be reused for later builds.

## Known verification gaps

Still required before Mainnet/listing readiness:

- additional independent public nodes/seeds;
- DNS seeds;
- long-duration multi-node soak;
- live public fork/reorg test across independent nodes;
- live transaction send/relay/confirmation test between independent wallets as a formal release gate;
- NAT traversal/UPnP/NAT-PMP or finalized documented fallback;
- production peer abuse/reputation hardening;
- final Mainnet parameter/ticker/address freeze;
- production exchange API;
- signed production release;
- release/update rollback rehearsal;
- independent security review/audit status disclosure.
