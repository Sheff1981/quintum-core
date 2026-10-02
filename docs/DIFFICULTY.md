# QUINTUM Difficulty Adjustment

Status: **DRAFT — pre-mainnet**

## Principle

A miner is not allowed to choose an arbitrary easy target.

For every candidate block, QUINTUM independently calculates the exact `bits` value expected for that block's parent branch. The block is rejected when its header does not contain that exact value.

Only after contextual difficulty is correct does normal Proof-of-Work validation check:

`block_hash <= target`

and:

`target <= network_pow_limit`

## Mainnet algorithm

Current Mainnet candidate:

- target spacing: 150 seconds
- interval: 2016 blocks
- target timespan: 302400 seconds

At non-retarget heights:

`next_bits = previous_bits`

At each retarget boundary:

1. locate the first block of the completed difficulty period on the same branch;
2. measure:
   `actual_timespan = last_timestamp - first_timestamp`
3. clamp actual timespan to:
   - minimum: target timespan / 4
   - maximum: target timespan * 4
4. calculate:
   `new_target = old_target * actual_timespan / target_timespan`
5. cap the result at the network PoW limit;
6. encode the result canonically into compact `bits`.

This limits a single retarget to at most a fourfold increase or fourfold decrease in target.

## Exact integer arithmetic

Difficulty calculations use deterministic unsigned 256-bit target arithmetic.

No floating-point math is used.

The implementation performs exact integer scaling and canonical CompactTarget encoding so Windows and Linux derive the same result.

Overflow conditions are explicitly checked and cannot silently wrap into a different difficulty.

## Branch-specific validation

Retarget history is resolved by following the candidate block's own ancestors in the block index.

This is important during forks: the expected target belongs to the candidate branch, not necessarily the currently active tip.

## Testnet minimum-difficulty rule

When enabled, a Testnet block may use the network PoW limit if:

`candidate_timestamp > parent_timestamp + 2 * target_spacing`

For a normally timed block after such an emergency block, QUINTUM walks backward on that branch to the most recent non-minimum difficulty block, stopping at a retarget boundary.

## Regtest

Regtest has `no_retargeting = true`.

Its target remains fixed at the easy Regtest PoW limit. This is a testing rule only.

## Miner interface

`Chainstate::next_work_required(candidate_timestamp)` returns the exact compact `bits` required for a new block extending the active tip.

Mining code therefore consumes the consensus result rather than duplicating difficulty logic.

## Tested properties

Automated tests cover:

- distinct Mainnet/Testnet/Regtest parameter sets;
- unchanged difficulty at the target timespan;
- 4x clamp on very fast periods;
- PoW-limit clamp on slow periods;
- canonical compact target encoding;
- rejection of targets above network PoW limit;
- Regtest no-retarget behavior;
- rejection of a valid-hash block carrying the wrong contextual `bits`;
- acceptance of the correct retarget block;
- Testnet minimum-difficulty escape;
- Testnet restoration of the previous non-minimum target;
- miner-facing next-work calculation.

## Timestamp dependency

Difficulty uses block timestamps, so Mainnet genesis will **not** be frozen until QUINTUM also has explicit timestamp-consensus rules such as Median Time Past and future-time limits.
