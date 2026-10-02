# Architecture Decision Log

## D-001 — Project name
**Decision:** QUINTUM.

Origin: inspired by the classical idea of the fifth essence / quintessence.

## D-002 — Core language
**Decision:** C++23.

Reason: modern C++ with strong native performance and mature systems tooling while remaining suitable for a Bitcoin-class node architecture.

## D-003 — Ledger model
**Decision:** UTXO.

Reason: deterministic spend tracking, explicit double-spend validation and a proven fit for proof-of-work digital cash.

## D-004 — Consensus family
**Decision:** Proof of Work.

## D-005 — No privileged monetary controls
**Decision:** no premine hidden from documentation, master mint key, balance override, or developer-only consensus bypass.

## Freeze states

- **DECIDED**: architectural direction chosen.
- **DRAFT**: value may change during development.
- **TESTNET FROZEN**: fixed for that test network.
- **MAINNET FROZEN**: changing it can split the production network.
