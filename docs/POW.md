# QUINTUM Proof of Work

Status: **RANDOMX v2 TESTNET CONSENSUS + GENESIS IMPLEMENTED — SERVER DEPLOYMENT PENDING — NOT MAINNET FROZEN**

The current running Testnet still uses double-SHA-256 PoW. The next incompatible Testnet will replace mining PoW with RandomX while retaining SHA-256 where it is useful for transaction IDs, block IDs, signatures and other non-mining hashing.

RandomX v2.0.1 is now pinned in the build at upstream commit `aaafe71322df6602c21a5c72937ac284724ae561`. QUINTUM has a light-mode RandomX v2 hashing wrapper plus fixed upstream consensus vectors on Linux and Windows CI. The legacy SHA-256 Testnet remains available under its original identity. A separate `randomx-testnet` parameter set now activates RandomX v2, the fixed seed schedule, ASERT, the RandomX monetary rules and the pinned RandomX Genesis without reinterpreting old chain data.

## Mining algorithm

- Consensus mining algorithm: **RandomX**.
- RandomX is the only mining PoW for the new network.
- QUINTUM will not use a hybrid SHA-256 + RandomX mining schedule.
- A specific RandomX upstream revision/configuration must be vendored or pinned before activation so dependency updates cannot silently change consensus behavior.

## Target block interval

- **120 seconds** per block.

## RandomX seed schedule

The QUINTUM RandomX seed schedule is now implemented and covered by fixed vectors:

- seed interval: **2,048 blocks**;
- seed lag: **64 blocks**;
- domain separation: **QUINTUM-RX-SEED-V1**;
- for candidate height `H`, seed height is `0` while `H <= 2112`; afterwards it is `(H - 64 - 1) & ~(2048 - 1)`;
- seed key is exactly `double-SHA-256(ASCII("QUINTUM-RX-SEED-V1") || uint64_le(seed_height) || seed_block_hash[32])`;
- the seed block hash is taken from the candidate block's own ancestor branch, so forks derive their seed from their own history;
- Genesis alone uses the bootstrap seed key with seed height 0 and an all-zero 32-byte seed-block hash, avoiding a circular dependency on the Genesis hash;
- blocks after Genesis use the real Genesis block ID while seed height remains 0.

All nodes therefore derive the same 32-byte RandomX key for the same branch and candidate height.

## Difficulty adjustment

The next RandomX Testnet is planned to use per-block **ASERT** difficulty adjustment rather than the current 2,016-block Bitcoin-style retarget.

Target parameters:

- target spacing: **120 seconds**;
- adjustment: **every block**;
- ASERT half-life: **34,560 seconds / 9 hours 36 minutes**;
- chain selection: greatest cumulative valid chain work.

Exact integer arithmetic, anchor rules and test vectors must be frozen before activation.

## RandomX block input

For RandomX mining and validation, the input is exactly the existing **88-byte serialized QUINTUM block header**:

`version || previous_block || merkle_root || timestamp || bits || nonce`

using the existing field serialization rules. The 32-byte RandomX output is compared to the compact target using QUINTUM's existing byte-order/target comparison convention. SHA-256 remains the block-ID hash; RandomX is the mining PoW only.

The implementation includes real RandomX nonce search, independent RandomX verification, branch-derived seed selection and a per-thread seed cache so the ~256 MiB light cache is not rebuilt for every block.

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
