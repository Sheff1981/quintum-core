# QUINTUM Chain Parameters

Status: **LEGACY SHA-256 TESTNET PRESERVED; RANDOMX TESTNET IDENTITY AND GENESIS IMPLEMENTED**

QUINTUM uses explicit parameter sets so Mainnet, Testnet and Regtest cannot silently share network identity or Proof-of-Work policy.

## Current implemented SHA-256 test network

The repository currently contains the original pre-mainnet SHA-256 candidate:

- target block spacing: 600 seconds;
- periodic 2,016-block retarget;
- SHA-256-based PoW;
- existing pinned Genesis values.

Those values remain necessary to interpret the existing public Testnet and its stored chain data.

## Next RandomX Testnet

The next incompatible public Testnet is specified to use:

- mining PoW: **RandomX**;
- target block spacing: **120 seconds**;
- difficulty adjustment: **per-block ASERT**;
- ASERT half-life: **34,560 seconds / 9 hours 36 minutes**;
- ASERT arithmetic and Chainstate integration: **implemented and covered by deterministic vectors**;
- ASERT anchor: **Genesis at height 0**, with a virtual parent timestamp one 120-second target interval before Genesis;
- coinbase maturity: **500 blocks**;
- RandomX seed interval: **2,048 blocks**;
- RandomX seed lag: **64 blocks**;
- primary monetary schedule and 5% founder subsidy split as defined in `MONETARY_POLICY.md`;
- tail subsidy of 1 QMU/block from height 6,000,001.

The RandomX Testnet now has a **new Genesis and distinct network identity**. Existing SHA-256 Testnet chain/wallet data remains separate and is not deleted or silently migrated.

## Existing network identities

Current implemented identities remain documented for the legacy SHA-256 Testnet until the RandomX network is implemented:

### Mainnet candidate

- message start: `51 b7 4c a3`
- P2P port: `28444`
- RPC port: `28445`

### Testnet candidate

- message start: `b7 d7 16 5a`
- P2P port: `38444`
- RPC port: `38445`

### Regtest candidate

- message start: `33 20 e2 ee`
- P2P port: `48444`
- RPC port: `48445`

### RandomX Testnet

- network/data directory: `randomx-testnet`
- message start: `3b bf b9 f0`
- P2P port: `39444`
- RPC port: `39445`
- address HRP: `xqmu`
- PoW limit bits: `0x1f7fffff`
- Genesis timestamp: `1791158400`
- Genesis nonce: `80`
- Genesis Merkle root: `1e11ac64fba90f543acd87018ec0d9da7ce11d56892026037728688b8c15cd0a`
- Genesis block ID: `89477dab8594e000e155b2a1e020ce8a73a3b03fa32568ac8773900794dca360`
- Genesis RandomX PoW hash: `00248b3ed59b5fd59e60d06dc425deecd029d449854b674fcac5651420e115af`

These values are pinned and intentionally incompatible with the legacy SHA-256 Testnet.

## Wallet address encoding candidate

Current address HRPs remain:

- Mainnet: `qmu`
- Testnet: `tqtm`
- Regtest: `rqmu`
- RandomX Testnet: `xqmu`

Address encoding is independent from the PoW migration unless a separate decision changes it.

## Freeze rule

No RandomX parameter is **MAINNET FROZEN** yet. The next step is implementation and destructive-incompatibility-aware public Testnet validation. Mainnet parameters are frozen only after multi-node, reorg, persistence, wallet and mining tests pass.
