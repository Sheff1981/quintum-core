# QUINTUM Proof of Work

Status: **DECIDED FOR NEXT RANDOMX TESTNET — NOT YET IMPLEMENTED — NOT MAINNET FROZEN**

The current running Testnet still uses double-SHA-256 PoW. The next incompatible Testnet will replace mining PoW with RandomX while retaining SHA-256 where it is useful for transaction IDs, block IDs, signatures and other non-mining hashing.

## Mining algorithm

- Consensus mining algorithm: **RandomX**.
- RandomX is the only mining PoW for the new network.
- QUINTUM will not use a hybrid SHA-256 + RandomX mining schedule.
- A specific RandomX upstream revision/configuration must be vendored or pinned before activation so dependency updates cannot silently change consensus behavior.

## Target block interval

- **120 seconds** per block.

## RandomX seed schedule

Planned QUINTUM RandomX seed policy:

- seed interval: **2,048 blocks**;
- seed lag: **64 blocks**;
- domain separation: **QUINTUM-RX-SEED-V1**;
- seed material is derived deterministically from QUINTUM chain history;
- all nodes must derive the identical seed for the same candidate height.

The exact byte-level seed preimage and genesis/bootstrap behavior must be covered by fixed consensus test vectors before activation.

## Difficulty adjustment

The next RandomX Testnet is planned to use per-block **ASERT** difficulty adjustment rather than the current 2,016-block Bitcoin-style retarget.

Target parameters:

- target spacing: **120 seconds**;
- adjustment: **every block**;
- ASERT half-life: **34,560 seconds / 9 hours 36 minutes**;
- chain selection: greatest cumulative valid chain work.

Exact integer arithmetic, anchor rules and test vectors must be frozen before activation.

## Proof validation

A candidate block is valid only when:

1. its header and consensus fields are structurally valid;
2. the expected difficulty target for that branch is correct;
3. the deterministic RandomX seed for that height is correct;
4. the RandomX PoW hash meets the encoded target;
5. all normal transaction, UTXO, monetary and resource checks pass.

There is no simulated mining success path.

## Non-mining hashes

Changing mining PoW to RandomX does **not** imply replacing every SHA-256 use in QUINTUM. Transaction IDs, signature hashing and other cryptographic identifiers may continue to use the existing SHA-256/double-SHA-256 constructions unless separately changed by an explicit consensus decision.

## Migration rule

RandomX activation defines a new incompatible test network with a new Genesis/network identity. The existing SHA-256 Testnet data is preserved as historical test data and is not silently reinterpreted as RandomX chain data.
