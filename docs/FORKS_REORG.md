# QUINTUM Forks and Reorganization

Status: **DRAFT — pre-mainnet**

## Purpose

A decentralized proof-of-work network must tolerate temporary forks. Two miners can legitimately produce competing blocks before learning about each other.

QUINTUM therefore keeps a block index that can contain:

- the active chain;
- weaker side branches;
- blocks that later become known to be contextually invalid.

The active chain is selected by **greatest cumulative valid proof of work**, not by block count alone.

## Block index

Each indexed block stores:

- full block data;
- block hash;
- parent hash;
- height;
- cumulative chain work from development genesis to that block;
- failed status.

A block with an unknown parent is not accepted into the index yet. Orphan handling belongs to the future P2P layer.

## Fork-choice rule

A side branch does not replace the active chain merely because it exists.

Activation is attempted only when:

`candidate_chain_work > active_chain_work`

Equal cumulative work does **not** trigger a reorg. This gives deterministic "keep current tip" behavior until one branch proves strictly more work.

## Reorganization procedure

When a heavier candidate appears:

1. locate the common ancestor of active and candidate chains;
2. build the candidate path from the common ancestor to the new tip;
3. copy the current UTXO and active-chain state;
4. disconnect the old active branch on the staged state using block undo;
5. connect candidate blocks forward on the staged state;
6. validate every transaction while connecting;
7. commit the staged state only if the complete reorg succeeds.

The live chain is never partially modified during an attempted reorg.

## Invalid heavier branch

A side branch may have valid headers and PoW but still contain a transaction that is invalid in that branch context.

If such a branch becomes heavier:

- activation is attempted on staged state;
- the invalid block is marked failed;
- the reorg is abandoned;
- the original active chain and UTXO remain unchanged;
- future descendants of the known-invalid branch are rejected.

## Tested scenario

The automated test constructs:

- one development genesis;
- active branch A;
- competing branch B;
- branch B reaching equal work without replacing A;
- branch B gaining one more PoW block and replacing A;
- UTXO state switching from A's spend to B's spend;
- competing branch C containing a missing-input spend;
- branch C becoming heavier;
- failed activation of C;
- exact preservation of branch B as the active chain;
- early rejection of descendants of the failed branch.

## Current limitations

Not implemented yet:

- persistent block index;
- orphan block pool;
- headers-first synchronization;
- difficulty adjustment validation;
- exact mainnet genesis enforcement;
- invalidity propagation optimization for all already-indexed descendants;
- pruning;
- disk-backed undo data.
