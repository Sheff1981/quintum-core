# QUINTUM Genesis Blocks

Status: **GENESIS CANDIDATES — code-pinned and reproducible**

Date of construction: **2026-10-02**

The three QUINTUM networks use separate Genesis blocks. Each Genesis is reconstructed from deterministic consensus code and compared against immutable constants stored in `ChainParams`.

## Public anchor

The Genesis coinbase message includes the public pre-Genesis repository milestone:

`1bac54a3f46c`

This is the prefix of the final Stage 11 commit that existed publicly before Genesis construction.

The common message prefix is:

`QUINTUM 02/Oct/2026 1bac54a3f46c Independent PoW digital cash`

Each network appends its own network name.

This acts as a public development anchor: the committed Genesis construction follows the already-published Stage 11 state.

## Timestamp

All three Genesis headers use:

- Unix timestamp: `1790960400`
- UTC: **2026-10-02 17:00:00 UTC**

## Coinbase

The Genesis coinbase:

- transaction version: 1
- one null previous outpoint
- one output
- output value: the full height-0 subsidy, **50 QUINTUM**
- output locking script: permanently reserved unspendable version `0x00`
- marker payload: `QUINTUM-GENESIS`

The locking-script version `0x00` is permanently reserved as **provably unspendable** by consensus.

Therefore the Genesis subsidy has no owner and no private key. It cannot be spent now or by a future wallet/script extension without changing consensus and creating an incompatible network.

This mirrors the principle that Genesis must not secretly allocate funds to the creator.

## Mainnet Genesis

Coinbase message:

`QUINTUM 02/Oct/2026 1bac54a3f46c Independent PoW digital cash | mainnet`

Parameters:

- version: `1`
- previous block: all zeroes
- timestamp: `1790960400`
- bits: `0x1e0ffff0`
- nonce: `591080`

Merkle root:

`b1d9e1aedbe90d5b88148d4af6b0a64d6fb20c286d72192713147869e69580c5`

Genesis hash:

`0000008b82073109dc079e6c5b7eac0c2fba8a5633822f87fc33719633ebab8c`

## Testnet Genesis

Coinbase message:

`QUINTUM 02/Oct/2026 1bac54a3f46c Independent PoW digital cash | testnet`

Parameters:

- version: `1`
- previous block: all zeroes
- timestamp: `1790960400`
- bits: `0x1e0ffff0`
- nonce: `969294`

Merkle root:

`d5be95629c8ce60e22e817bc54accc3ed75983d5267b89724cd808f422a8c179`

Genesis hash:

`0000039bf09b49dfa4c9bcb924c0c38257fdeef9952021baabceb86fa099e872`

## Regtest Genesis

Coinbase message:

`QUINTUM 02/Oct/2026 1bac54a3f46c Independent PoW digital cash | regtest`

Parameters:

- version: `1`
- previous block: all zeroes
- timestamp: `1790960400`
- bits: `0x2100ffff`
- nonce: `0`

Merkle root:

`01b9f141fff566d6d50e700ff0c59f07ff0a89123921340bd946f30386c09d89`

Genesis hash:

`211c0cdb97dfb8bc2f0190e40132d1eb3be5ab9a03c9717aa3b2e90a98b3fcd6`

## Consensus enforcement

For built-in Mainnet, Testnet and Regtest:

- the first block must exactly hash to the configured Genesis hash;
- an alternative first block is rejected with `wrong_genesis`;
- the configured Genesis is reconstructed from its message, subsidy, locking script, timestamp, bits and nonce;
- reconstructed Merkle root must equal the pinned Merkle root;
- reconstructed block hash must equal the pinned Genesis hash;
- Genesis Proof of Work must independently validate against the network PoW parameters.

Synthetic unit-test ChainParams may explicitly set `genesis.enforce = false` so tests can create small arbitrary chains without pretending they are a real network.

## Reproducibility

The Genesis test suite independently checks:

- all three exact hashes;
- all three exact Merkle roots;
- all three nonces;
- exact coinbase message;
- exact height-0 subsidy;
- unspendable Genesis output;
- Proof of Work;
- one-transaction Merkle equality;
- rejection of a mutated alternative first block;
- successful Chainstate activation of the exact configured Genesis.

## Compatibility rule

Once a public network is launched using these Genesis constants, changing any Genesis field creates a different incompatible network.

The following are therefore identity-defining:

- coinbase message;
- output script/value;
- timestamp;
- bits;
- nonce;
- Merkle root;
- Genesis hash.

No production release may silently alter them.
