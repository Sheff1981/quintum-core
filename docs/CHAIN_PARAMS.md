# QUINTUM Chain Parameters

Status: **DRAFT — pre-mainnet**

QUINTUM uses explicit parameter sets so Mainnet, Testnet and Regtest cannot silently share network identity or Proof-of-Work policy.

These values are implemented and tested, but remain changeable until the corresponding genesis blocks are frozen.

## Mainnet candidate

- network name: `mainnet`
- message start / network magic: `51 b7 4c a3`
- P2P port: `28444`
- RPC port: `28445`
- target block spacing: **600 seconds**
- retarget interval: **2016 blocks**
- target retarget timespan: **1209600 seconds / 14 days**
- PoW limit compact bits: `0x1e0ffff0`
- special minimum-difficulty blocks: **disabled**
- difficulty retargeting: **enabled**

## Testnet candidate

- network name: `testnet`
- message start / network magic: `b7 d7 16 5a`
- P2P port: `38444`
- RPC port: `38445`
- target block spacing: **600 seconds**
- retarget interval: **2016 blocks**
- PoW limit compact bits: `0x1e0ffff0`
- special minimum-difficulty blocks: **enabled**
- difficulty retargeting: **enabled**

Testnet permits a minimum-difficulty block when its timestamp is more than **2 target spacings (1200 seconds)** after its parent. A normally timed following block restores the most recent non-minimum difficulty, except at a normal retarget boundary.

## Regtest candidate

- network name: `regtest`
- message start / network magic: `33 20 e2 ee`
- P2P port: `48444`
- RPC port: `48445`
- target block spacing: **1 second**
- nominal retarget interval: **144 blocks**
- PoW limit compact bits: `0x2100ffff`
- difficulty retargeting: **disabled**

Regtest intentionally keeps a fixed easy target so automated tests and local developers can mine blocks immediately.

## Network magic derivation

The current draft message-start bytes were deterministically selected from the first four SHA-256 bytes of:

- `QUINTUM-mainnet`
- `QUINTUM-testnet`
- `QUINTUM-regtest`

They exist to reduce accidental cross-network message interpretation.

## Separation rule

A node must be created with exactly one ChainParams set. Consensus validation reads its PoW policy from that set.

A Mainnet node must never accept a Testnet or Regtest difficulty policy merely because a block's raw hash satisfies some target.

## Not frozen yet

Before Mainnet genesis:

- all message-start bytes will be reviewed once more;
- ports will be checked as part of P2P deployment;
- the Mainnet/Testnet genesis blocks will be uniquely generated;
- final genesis hashes will be inserted into ChainParams;
- the parameter documents will change from DRAFT to the appropriate frozen state.
