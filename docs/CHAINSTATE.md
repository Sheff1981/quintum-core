# QUINTUM Chainstate

Status: **DRAFT — pre-mainnet**

## Purpose

Chainstate is the authoritative active-chain state that combines:

- ordered block headers;
- cumulative proof of work;
- the current UTXO set;
- per-block undo data.

The implementation is deliberately conservative: a block either connects completely or leaves the live state unchanged.

## Active chain rules currently implemented

A candidate block must:

1. pass structural block validation;
2. reference the current active tip, or use a zero previous hash when the chain is empty;
3. satisfy Proof of Work;
4. contribute valid 256-bit chain work without overflow;
5. have a height representable by the current height type;
6. have every transaction apply successfully to a staged UTXO view;
7. have a fee sum that does not overflow.

Only after every check succeeds is the staged UTXO state committed.

## Atomic block connection

Block connection uses a staged copy of the UTXO set.

This is intentionally simple and correctness-first during the pre-mainnet phase:

- live state is not touched while validation is running;
- a failure in transaction N cannot leave transactions 0..N-1 applied;
- successful transaction undo data is collected for the entire block;
- commit happens only after the full block is valid.

The final persistent database layer may use database transactions or write batches, but it must preserve the same all-or-nothing behavior.

## Block undo

Every connected block stores one UTXO undo record per transaction.

Disconnecting the tip:

1. creates a staged copy of the current UTXO set;
2. applies transaction undo records in reverse order;
3. commits the restored state only if all undo operations succeed;
4. removes the active tip entry.

This is the mechanical foundation required for future reorganization handling.

## Cumulative work

Each active-chain entry stores cumulative work:

`chain_work(height) = chain_work(height - 1) + block_work`

Fork choice is not implemented yet, but future selection will compare cumulative valid work rather than block count alone.

## Development-first-block rule

While no QUINTUM genesis is frozen, an empty development chain accepts a structurally valid PoW block whose previous hash is all zeroes.

This is **not** the final mainnet genesis rule.

Before mainnet, network parameters will contain the exact expected genesis hash, and chainstate initialization will reject any alternative first block.

## Not implemented yet

- block index for side branches;
- automatic best-chain selection;
- multi-block reorganization;
- persistent block/chainstate storage;
- exact genesis enforcement;
- signature/script consensus;
- subsidy and coinbase maturity rules;
- difficulty adjustment rules.
