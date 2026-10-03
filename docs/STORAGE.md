# QUINTUM persistent blockchain storage

Stage 13 introduced restart-safe durable blockchain state without changing any
consensus parameter, Genesis constant, transaction format, block format, address
rule, monetary rule, network magic, or port. Stage 31 gives that state a
Bitcoin-Core-style component layout without changing any on-disk record format.

## Current data-directory layout

Each network has its own root directory:

```text
<network>/
├─ blocks/
│  └─ blocks.dat
├─ chainstate/
│  ├─ chainstate.dat
│  └─ chainstate.dat.tmp        # only during an atomic commit
├─ indexes/                     # reserved for real optional indexes
├─ wallets/
│  └─ default/
│     ├─ wallet.dat
│     ├─ wallet_state.dat
│     └─ wallet_meta.dat
├─ peers.dat
└─ .lock                         # OS-level exclusive datadir lock
```

`blocks/blocks.dat` remains the existing append-only framed block store.
`chainstate/chainstate.dat` remains the existing checksummed snapshot.
`peers.dat` stays at the network root. The persistent `.lock` file is only a lock anchor: exclusivity is enforced by the live OS handle (`CreateFileW` sharing rules on Windows, `flock` on POSIX), so a stale file after a clean or unclean shutdown does not block restart. A second live QUINTUM process using the same network datadir fails closed before migration or runtime startup. No placeholder `mempool.dat`, banlist or index database is created until the corresponding feature is real.

## Legacy flat-layout migration

Stage 31 recognizes the earlier flat layout and moves known files into the
component directories before normal node/wallet startup. Migration uses
same-filesystem rename, never overwrites an existing destination and does not
rewrite file contents. If both a legacy source and a new destination exist,
startup fails closed with a conflict instead of guessing which copy is
authoritative. A retry after an interrupted partial migration is safe because
already-moved files are simply left in their destination.

A `--network-only` seed creates blockchain/index directories but does not
create `wallets/` and does not move or inspect legacy wallet material.

## Commit protocol

A persistent state transition is staged in memory first.

1. Validate the block or disconnect operation against a copy of Chainstate.
2. Verify the already committed prefix of `blocks/blocks.dat`.
3. Remove any uncommitted crash tail after the last committed block record.
4. Append and flush new block records.
5. Serialize block-index metadata, active-chain metadata, UTXO set and undo data.
6. Append a double-SHA-256 checksum.
7. Flush the temporary snapshot.
8. Atomically replace `chainstate/chainstate.dat`.
9. Publish the staged in-memory Chainstate only after durable commit succeeds.

If the process stops after step 4 but before step 8, the extra block-log bytes
are not committed because the previous `chainstate/chainstate.dat` still contains the
older committed block count. Startup ignores that tail; the next commit trims it
before appending new records.

On Windows the snapshot replacement uses `MoveFileExW` with
`MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`. On POSIX systems it uses
`rename()` followed by a directory fsync when available.

## Integrity

`chainstate/chainstate.dat` stores and checks:

- storage-format version;
- QUINTUM network identity;
- network message magic;
- Genesis enforcement mode and Genesis hash;
- accepted block order;
- block-index hash, parent, height, cumulative work and failed flag;
- active-chain hash, height, cumulative work and block undo;
- complete UTXO set;
- whole-file double-SHA-256 checksum.

Each `blocks/blocks.dat` record has its own double-SHA-256 checksum.

A snapshot for a different QUINTUM network is rejected before state is loaded.

## Startup reconstruction

Disk state is never trusted as an already-valid in-memory Chainstate.

Startup:

1. verifies the snapshot checksum and format;
2. reads exactly the committed number of block-log records;
3. verifies every block-record checksum;
4. parses blocks with bounded allocations;
5. replays accepted blocks through the normal consensus `Chainstate::connect_block`;
6. verifies stored block-index height, parent and cumulative work;
7. independently rebuilds the stored active chain;
8. recomputes and compares block undo;
9. recomputes and compares the complete UTXO set;
10. restores side-branch index state only after all verification succeeds.

The destination Chainstate is replaced only after the whole reconstruction
succeeds.

## PersistentChainstate

`PersistentChainstate` provides the node-facing transactional wrapper.

A successful consensus mutation is not exposed as committed state unless the
corresponding disk commit also succeeds. Storage failure leaves the previously
committed in-memory state intact.

## Stage-13 QA

The storage test suite covers:

- shutdown/restart with identical tip, height, UTXO set and cumulative work;
- persistence of a side branch;
- heavier-branch reorg after restart;
- persistence and replay of undo data;
- persistent disconnect followed by another restart;
- rejection of wrong-network data;
- rejection of corrupted snapshot checksum;
- rejection of a truncated committed block record;
- safe ignoring of an uncommitted `blocks/blocks.dat` crash tail.

This is the first durable storage layer. Future optimization can move large
chainstate/index tables to a key-value database without changing consensus or
the public block/transaction serialization.
