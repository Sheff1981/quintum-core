# QUINTUM Exchange Listing & Due-Diligence Pack

Status: **LIVE DOCUMENT — PRE-ALPHA / PUBLIC TESTNET**

Last technical state covered: `c5e29dcf45375d227872dd1a21c9127b214b1c8d`  
Repository: https://github.com/Sheff1981/quintum-core  
Project inception: **2026-10-02**  
Mainnet status: **NOT LAUNCHED**  
Public Testnet status: **ACTIVE**

This directory is the canonical exchange-facing documentation package for QUINTUM. It is intended for exchanges, custodians, infrastructure providers, security reviewers, market-data providers, wallet providers and other counterparties that need a complete technical and historical record before any future Mainnet listing.

## Mandatory interpretation rule

QUINTUM is not currently offered as a Mainnet asset. Development/Testnet coins have no monetary value. No exchange should enable production deposits or withdrawals until QUINTUM Mainnet is explicitly launched, final network parameters are frozen, the production integration interface is published and the Mainnet release manifest is signed and released.

Where historical documentation and current implementation differ, the following precedence applies:

1. consensus implementation and pinned constants in source;
2. automated tests;
3. current English exchange/due-diligence pack;
4. other protocol documentation;
5. historical development notes.

Historical documents are retained because they provide provenance; they are not allowed to silently redefine current consensus.

## Package contents

- [TECHNICAL_DUE_DILIGENCE.md](TECHNICAL_DUE_DILIGENCE.md) — architecture, consensus, monetary policy, Genesis, networking, wallet, security, storage and distribution.
- [EXCHANGE_INTEGRATION.md](EXCHANGE_INTEGRATION.md) — future exchange node/deposit/withdrawal integration requirements and current readiness state.
- [VERIFICATION_LOG.md](VERIFICATION_LOG.md) — real-network validation, CI evidence, release checks and known limitations.
- [DEVELOPMENT_HISTORY.md](DEVELOPMENT_HISTORY.md) — complete one-line Git commit ledger from the first repository commit through the technical state covered by this pack.

## Disclosure summary

- Independent Layer-1 blockchain: **yes**
- Token on another chain: **no**
- Consensus: **Proof of Work**
- Ledger: **UTXO**
- Chain selection: **greatest cumulative valid proof of work**
- Hidden premine: **none**
- Developer mint/master mint key: **none**
- Genesis allocation to creator: **none**
- Genesis subsidy: **50 QUINTUM, provably unspendable**
- Transaction signatures: **ECDSA/secp256k1**
- Block/transaction hashing: **SHA-256 family; transaction/block identifiers use double-SHA-256**
- Current working desktop symbol: **QTM**
- Final Mainnet ticker: **not yet frozen**
- Public Testnet P2P port: **38444**
- Current public Testnet bootstrap node: **212.193.15.139:38444**
- Public Testnet ordinary desktop default: **Testnet**
- Windows installer: **available as unsigned pre-alpha**
- Mainnet listing readiness: **not yet**
- Exchange RPC/API freeze: **not yet**

## Documentation maintenance rule

Every consensus change, networking change, wallet change, security fix, release artifact change, public-network verification and production-readiness decision must be reflected in this pack. Source-level change provenance is preserved by Git history; exchange-facing implications and verification results are recorded in the relevant files here.

No consensus-critical parameter may be represented to an exchange as final while the repository marks it as a candidate or pre-mainnet value.
