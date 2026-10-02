# QUINTUM persistent blockchain storage

Stage 13 introduces restart-safe durable blockchain state without changing any
consensus parameter, Genesis constant, transaction format, block format, address
rule, monetary rule, network magic, or port.

## Files

Each network data directory contains:

- `blocks.dat` — append-only framed block records.
- `chainstate.dat` — checksummed chainstate snapshot replaced atomically.
- `chainstate.dat.tmp` — temporary file used only while committing a new
  snapshot. It is not authoritative.

## Commit protocol

A persistent state transition is staged in memory first.

1. Validate the block or disconnect operation against a copy of Chainstate.
2. Verify the already committed prefix of `blocks.dat`.
3. Remove any uncommitted crash tail after the last committed block record.
4. Append and flush new block records.
5. Serialize block-index metadata, active-chain metadata, UTXO set and undo data.
6. Append a double-SHA-256 checksum.
7. Flush the temporary snapshot.
8. Atomically replace `chainstate.dat`.
9. Publish the staged in-memory Chainstate only after durable commit succeeds.

If the process stops after step 4 but before step 8, the extra block-log bytes
are not committed because the previous `chainstate.dat` still contains the
older committed block count. Startup ignores that tail; the next commit trims it
before appending new records.

On Windows the snapshot replacement uses `MoveFileExW` with
`MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`. On POSIX systems it uses
`rename()` followed by a directory fsync when available.

## Integrity

`chainstate.dat` stores and checks:

- storage-format version;
- QUINTUM network identity;
- network message magic;
- Genesis enforcement mode and Genesis hash;
- accepted block order;
- block-index hash, parent, height, cumulative work and failed flag;
- active-chain hash, height, cumulative work and block undo;
- complete UTXO set;
- whole-file double-SHA-256 checksum.

Each `blocks.dat` record has its own double-SHA-256 checksum.

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
- safe ignoring of an uncommitted `blocks.dat` crash tail.

This is the first durable storage layer. Future optimization can move large
chainstate/index tables to a key-value database without changing consensus or
the public block/transaction serialization.
