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

## D-006 — Monetary policy candidate
**State:** DRAFT until mainnet genesis freeze.

Current candidate:

- 8 decimal places;
- 50 QUINTUM initial block subsidy;
- halving every 210,000 blocks;
- 100-block coinbase maturity;
- 21,000,000 QUINTUM money-range ceiling;
- exact scheduled subsidy maximum 20,999,999.9769 QUINTUM;
- coinbase may claim at most subsidy + transaction fees.

Reason: conservative, audit-friendly fixed issuance with no privileged mint path.

## D-007 — Initial ownership primitive
**State:** DRAFT until mainnet genesis freeze.

Use Bitcoin Core's pinned `libsecp256k1` for ECDSA. Initial spend authorization is a minimal versioned P2PK construction with compressed public keys, compact low-S signatures and a QUINTUM-specific domain-separated SIGHASH_ALL preimage.

Reason: minimize consensus surface area while establishing real cryptographic ownership before addresses, wallet UX or a broader script system.

## D-008 — Network and difficulty candidate
**State:** DRAFT until genesis freeze.

Current Mainnet candidate uses 600-second target spacing, 2016-block Bitcoin-style periodic retargeting, 1/4x to 4x timespan clamps and PoW limit bits `0x1e0ffff0`.

Testnet uses the same base policy with a delayed-block minimum-difficulty rule. Regtest has fixed easy difficulty.

Every block's expected `bits` is derived from its own parent branch before PoW acceptance; miners cannot lower difficulty by placing an arbitrary target in the header.

Reason: retain a small, deterministic, well-understood retarget design while keeping development and public testing networks operationally distinct.

## D-009 — Timestamp and resource limits
**State:** DRAFT until genesis freeze.

Use an 11-block Median Time Past rule, a 2-hour maximum future timestamp window, a 1,000,000-byte serialized block ceiling, 10,000-transaction block ceiling, 10,000-byte script ceiling and 100-byte coinbase unlocking-script ceiling.

Reason: keep block validation deterministic and resource-bounded before network/P2P parsing is exposed to untrusted peers.

## Freeze states

- **DECIDED**: architectural direction chosen.
- **DRAFT**: value may change during development.
- **TESTNET FROZEN**: fixed for that test network.
- **MAINNET FROZEN**: changing it can split the production network.
