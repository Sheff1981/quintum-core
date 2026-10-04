# Architecture Decision Log

## D-001 — Project name
**Decision:** QUINTUM.

Origin: inspired by the classical idea of the fifth essence / quintessence.

## D-002 — Core language
**Decision:** C++23.

## D-003 — Ledger model
**Decision:** UTXO.

## D-004 — Consensus family
**Decision:** Proof of Work.

## D-005 — No hidden monetary controls
**Decision:** no hidden premine, master mint key, balance override or developer-only consensus bypass.

A transparent founder subsidy is permitted only because it is explicitly specified below, enforced identically by every node, publicly visible, and cannot be increased without an incompatible consensus fork.

## D-006 — Monetary policy for next RandomX Testnet
**State:** DECIDED FOR IMPLEMENTATION; not Mainnet frozen.

- 8 decimals;
- Genesis spendable issuance: 0 QTM;
- target spacing: 120 seconds;
- six primary eras of 1,000,000 mineable blocks each;
- starting subsidy: 50 QTM/block;
- subsidy halves at each 1,000,000-block era boundary for six eras;
- primary issuance through height 6,000,000: exactly 98,437,500 QTM;
- founder subsidy: exactly 5% of scheduled primary subsidy;
- founder primary total: exactly 4,921,875 QTM;
- miner primary total: exactly 93,515,625 QTM;
- transaction fees: 100% to miner;
- from height 6,000,001: permanent 1 QTM/block tail subsidy, 100% to miner;
- founder subsidy from height 6,000,001 onward: 0 QTM;
- coinbase maturity: 500 blocks, about 16 h 40 min at target spacing.

Detailed arithmetic is normative in `MONETARY_POLICY.md`.

## D-007 — Initial ownership primitive
**State:** DRAFT until mainnet freeze.

Use the existing secp256k1-based ownership model unless changed by a separate reviewed decision.

## D-008 — PoW and difficulty for next RandomX Testnet
**State:** DECIDED FOR IMPLEMENTATION; not Mainnet frozen.

- mining PoW changes from double-SHA-256 to RandomX;
- RandomX is the sole mining PoW, not a hybrid;
- target spacing: 120 seconds;
- seed interval: 2,048 blocks;
- seed lag: 64 blocks;
- planned difficulty algorithm: per-block ASERT;
- planned ASERT half-life: 34,560 seconds / 9 h 36 min;
- active chain remains the valid chain with greatest cumulative work.

A specific RandomX revision and exact byte-level test vectors must be pinned before network activation.

## D-009 — Timestamp and resource limits
**State:** DRAFT until mainnet freeze.

Current resource limits remain in force unless separately changed and tested.

## D-010 — Legacy SHA-256 Genesis identity
**State:** HISTORICAL / CURRENT TESTNET IDENTITY.

The existing pinned Genesis values identify the current SHA-256 Testnet candidate. They are not reused for the next RandomX Testnet. Existing chain and wallet data must be preserved rather than silently reinterpreted.

## D-011 — RandomX Testnet is a new incompatible chain
**State:** DECIDED.

The RandomX migration requires a new Genesis/network identity. The old SHA-256 Testnet is retained as an archive/test history. No production Mainnet exists yet, so this change does not fork live production funds.

## D-012 — Founder payout transparency
**State:** DECIDED FOR IMPLEMENTATION.

For primary subsidy heights 1..6,000,000, 5% of scheduled subsidy is paid to one publicly documented founder payout script/address and 95% to the miner. The exact payout script/address must be published and pinned before activation. It receives no transaction fees and no tail subsidy.

## D-013 — Transaction fee model
**State:** DECIDED FOR IMPLEMENTATION.

QUINTUM keeps the existing Bitcoin/Litecoin-style size-based fee model:

- fees are based on serialized transaction size and fee rate, not on the amount being transferred;
- default wallet/minimum relay candidate: 1,000 atomic units per 1,000 serialized bytes;
- the wallet Auto rate is the maximum of its default rate, the node relay floor and the median current mempool fee rate;
- 100% of transaction fees go to the miner that includes the transaction;
- the founder receives 0% of transaction fees;
- fee policy remains separate from consensus: an otherwise valid lower-fee transaction is not made consensus-invalid merely because a node would normally refuse to relay it.

At the current P2PK transaction shape, a common 1-input/2-output transaction is about 202 bytes and therefore costs 202 atomic units = 0.00000202 QTM at the default rate.

## Freeze states

- **DECIDED**: architectural direction chosen.
- **DRAFT**: value may change during development.
- **DECIDED FOR IMPLEMENTATION**: approved target for the next test network but not yet present in running consensus code.
- **TESTNET FROZEN**: fixed for that test network.
- **MAINNET FROZEN**: changing it can split the production network.
