# QUINTUM Chain Parameters

Status: **DRAFT — pre-mainnet**

QUINTUM uses explicit parameter sets so Mainnet, Testnet and Regtest cannot silently share network identity or Proof-of-Work policy.

These values are implemented and tested. Genesis constants are now code-pinned; changing them creates a different network identity. Other pre-launch parameters remain subject to explicit review.

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

## Genesis identity

Exact Genesis hashes, Merkle roots, timestamps, bits, nonces and messages are recorded in `docs/GENESIS.md` and stored directly in `ChainParams`.

Built-in networks set `genesis.enforce = true`: the first block must equal the configured Genesis hash.

## Still subject to pre-launch review

- network magic bytes;
- P2P/RPC ports;
- address prefixes/encoding;
- fee policy;
- public-testnet operational behavior.

Changing Genesis itself is no longer a parameter tweak; it defines another network.


## Wallet address encoding candidate

Stage 20 implements network-separated Bech32m wallet addresses:

- Mainnet HRP: `qtm`
- Testnet HRP: `tqtm`
- Regtest HRP: `rqtm`

The payload currently contains address type `0x01` followed by the 33-byte compressed secp256k1 public key and maps directly to the existing P2PK v1 locking model.

These prefixes and encoding rules are implemented and regression-tested, but remain **pre-mainnet candidates** until the public network specification is deliberately frozen.
