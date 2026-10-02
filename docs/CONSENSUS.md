# QUINTUM Consensus Specification — DRAFT

This document becomes normative only when explicitly marked **MAINNET FROZEN**.

## Decisions already made

- Ledger model: UTXO
- Consensus family: Proof of Work
- Implementation language: C++23
- Chain selection: greatest cumulative valid proof of work
- Transaction ownership: cryptographic signatures; no administrator override
- Hidden premine/backdoor/master mint: prohibited
- Genesis: unique QUINTUM genesis block will be generated and permanently recorded

## Candidate cryptographic baseline

- Block/transaction digest family: SHA-256
- Signatures: secp256k1
- Exact serialization and domain-separation rules: to be frozen before genesis

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

## Parameters not frozen yet

- ticker / smallest-unit public name
- block target interval
- difficulty adjustment algorithm
- PoW limit
- maximum block weight/size
- fee policy
- address encoding and prefixes
- network magic
- mainnet/testnet P2P and RPC ports

These values will not be guessed and silently embedded. Each will be documented, tested, then frozen before mainnet genesis.
