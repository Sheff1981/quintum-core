# QUINTUM Timestamp Consensus and Resource Limits

Status: **DRAFT — pre-mainnet**

## Public-network block interval correction

Before genesis freeze, the Mainnet/Testnet target spacing was changed from the earlier 150-second draft to:

**600 seconds / 10 minutes**

The retarget interval remains 2016 blocks, therefore one target period is now:

**1,209,600 seconds / 14 days**

This supersedes the earlier 150-second draft. No genesis block or production network existed when the change was made.

## Median Time Past

For every non-genesis candidate block, QUINTUM collects timestamps from its parent and up to the previous 10 ancestors on that same branch.

Window:

**11 blocks**

The values are sorted and the median is selected.

A new block must satisfy:

`block_timestamp > MedianTimePast(parent)`

Equality is invalid.

This rule is branch-specific. A side branch uses its own ancestors, not timestamps from the currently active competing branch.

A block timestamp is not required to be greater than the immediate parent's timestamp; it only has to be strictly greater than the branch Median Time Past.

## Future-time limit

At block admission:

`block_timestamp <= adjusted_time + 7200 seconds`

Therefore the maximum future offset is:

**2 hours**

The API accepts an explicit adjusted time for deterministic testing. The normal node-facing overload uses the operating system wall clock until peer-adjusted network time is implemented.

A too-far-future block is rejected before it enters the block index.

## Block serialized-size limit

Maximum serialized block size:

**1,000,000 bytes**

Size is calculated from the actual deterministic QUINTUM serialization layout:

- 88-byte block header
- CompactSize transaction count
- exact serialized size of every transaction

The size calculator performs overflow checks and does not need to allocate a complete serialized block just to measure it.

## Transaction-count limit

Maximum transactions per block:

**10,000**

This provides an additional resource bound before later P2P parsing and mempool logic are exposed to untrusted peers.

## Script-size limits

Maximum individual script byte length:

**10,000 bytes**

Maximum coinbase unlocking script:

**100 bytes**

The current spendable P2PK v1 scripts are much smaller than these ceilings. The limits exist to bound malformed, unspendable or future script payloads.

## Validation order

Before a block is inserted into the block index, QUINTUM checks:

1. structural block validity;
2. block resource limits;
3. maximum future timestamp;
4. parent existence / ancestry;
5. Median Time Past;
6. contextual expected difficulty;
7. Proof of Work;
8. chain-work arithmetic;
9. transaction/UTXO validity when the branch is activated.

Resource or timestamp-invalid blocks never become future reorg candidates.

## Reorg interaction

MTP is calculated against the candidate block's own parent branch at admission time.

A side-branch block that passed contextual timestamp, resource, difficulty and PoW checks can remain indexed while weaker. If it later becomes the most-work branch, activation still uses staged UTXO reorganization.

## Current limits

These values are consensus candidates and are not MAINNET FROZEN until genesis is frozen:

- MTP window: 11
- max future time: 7,200 seconds
- max serialized block: 1,000,000 bytes
- max transactions: 10,000
- max script: 10,000 bytes
- max coinbase unlocking script: 100 bytes

## Remaining pre-genesis work

Before production genesis, QUINTUM still needs:

- exact genesis construction and verification;
- persistent block/chainstate storage;
- P2P message parsing that enforces size bounds before large allocations;
- network-adjusted time source;
- public testnet soak and adversarial validation.
