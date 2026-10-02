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

## Parameters not frozen yet

- ticker / smallest-unit name
- block target interval
- initial block subsidy
- halving interval and maximum issuance
- difficulty adjustment algorithm
- coinbase maturity
- maximum block weight/size
- fee policy
- address encoding and prefixes
- network magic
- mainnet/testnet P2P and RPC ports

These values will not be guessed and silently embedded. Each will be documented, tested, then frozen before mainnet genesis.
