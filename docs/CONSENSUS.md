# QUINTUM Consensus Specification — DRAFT

This document becomes normative only when explicitly marked **MAINNET FROZEN**.

## Decisions already made

- Ledger model: UTXO
- Consensus family: Proof of Work
- Implementation language: C++23
- Chain selection: greatest cumulative valid proof of work
- Transaction ownership: cryptographic signatures; no administrator override
- Hidden premine/backdoor/master mint: prohibited
- Genesis: unique Mainnet/Testnet/Regtest Genesis blocks are code-pinned and reproducible

## Candidate cryptographic baseline

- block/transaction digest family: SHA-256
- signatures: ECDSA over secp256k1 via pinned Bitcoin Core libsecp256k1 v0.8.0
- public keys: compressed 33-byte secp256k1 keys
- initial locking model: versioned P2PK
- current sighash: QUINTUM-domain-separated SIGHASH_ALL
- high-S signatures: rejected
- exact rules remain DRAFT until genesis freeze

See `docs/TRANSACTION_AUTHORIZATION.md`.

## Monetary consensus candidate — implemented, not mainnet-frozen

- atomic precision: 100,000,000 units per QUINTUM
- money range ceiling: 21,000,000 QUINTUM
- initial subsidy: 50 QUINTUM
- halving interval: 210,000 blocks
- exact scheduled subsidy maximum: 20,999,999.9769 QUINTUM
- coinbase maturity: 100 blocks
- coinbase reward ceiling: subsidy + transaction fees
- no privileged issuance path

See `docs/MONETARY_POLICY.md`.

## Network and difficulty candidate — implemented, not mainnet-frozen

Mainnet candidate:

- target block spacing: 600 seconds
- retarget interval: 2016 blocks
- retarget timespan: 1209600 seconds
- per-period target change clamp: 1/4x through 4x
- PoW limit bits: `0x1e0ffff0`
- contextual `bits` validation is branch-specific
- arbitrary miner-selected easier difficulty is rejected

Testnet uses the same base schedule with the documented delayed-block minimum-difficulty exception. Regtest keeps a fixed easy target.

Network-specific message-start bytes and draft P2P/RPC ports are also present in ChainParams.

See `docs/CHAIN_PARAMS.md` and `docs/DIFFICULTY.md`.

## Timestamp and resource candidate — implemented, not mainnet-frozen

- Median Time Past window: 11 blocks
- candidate timestamp must be strictly greater than MTP
- maximum future timestamp: adjusted time + 2 hours
- maximum serialized block size: 1,000,000 bytes
- maximum transactions per block: 10,000
- maximum script size: 10,000 bytes
- maximum coinbase unlocking script: 100 bytes

See `docs/TIMESTAMP_AND_LIMITS.md`.

## Genesis identity — code-pinned

Mainnet, Testnet and Regtest Genesis blocks have been deterministically constructed, mined where required, inserted into `ChainParams`, and covered by independent reconstruction tests.

See `docs/GENESIS.md`.

Changing any Genesis field now creates a different network identity. These constants are not to be edited casually.

## Parameters not frozen yet

- ticker / smallest-unit public name
- fee policy
- address encoding and prefixes
- final review of network magic and ports
- public-testnet validation before final release freeze

These values will not be guessed and silently embedded. Each will be documented, tested, then frozen before mainnet genesis.
