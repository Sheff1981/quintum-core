# QUINTUM Exchange Listing & Due-Diligence Pack

Status: **LIVE DOCUMENT — PRE-ALPHA / PUBLIC TESTNET**

Last technical state covered: `c5e29dcf45375d227872dd1a21c9127b214b1c8d`  
Last exchange-requirements review: **2026-10-04**  
Repository: https://github.com/Sheff1981/quintum-core  
Project inception: **2026-10-02**  
Mainnet status: **NOT LAUNCHED**  
Public Testnet status: **ACTIVE**

This directory is the canonical exchange-facing documentation package for QUINTUM. It is intended for exchanges, custodians, infrastructure providers, security reviewers, market-data providers, wallet providers and other counterparties that need a complete technical, historical and listing-integration record before any future Mainnet listing.

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
- [DEVELOPMENT_HISTORY.md](DEVELOPMENT_HISTORY.md) — public development commit ledger and provenance.
- [EXCHANGE_LISTING_REQUIREMENTS.md](EXCHANGE_LISTING_REQUIREMENTS.md) — live requirements register for Binance, Coinbase, Kraken, OKX, Bybit, KuCoin, Gate, MEXC and Bitget, based on official public sources.
- [LISTING_APPLICATION_MASTER.md](LISTING_APPLICATION_MASTER.md) — canonical English answer sheet used to populate exchange-specific applications without inventing data.
- [LISTING_REQUEST_TEMPLATES.md](LISTING_REQUEST_TEMPLATES.md) — initial listing requests, technical cover notes, due-diligence responses, upgrade/security notices and anti-fraud reply templates.
- [EXCHANGE_READINESS_MATRIX.md](EXCHANGE_READINESS_MATRIX.md) — concrete readiness gates and current blockers per exchange.
- [MICA_EEA_PREPARATION.md](MICA_EEA_PREPARATION.md) — MiCA/EEA preparation workflow, regulatory data-room checklist and XBRL/fact-check gates.
- [DUE_DILIGENCE_DOCUMENT_CHECKLIST.md](DUE_DILIGENCE_DOCUMENT_CHECKLIST.md) — complete founder/KYC, company/legal, technical, security, integration, supply, market and compliance document checklist.
- [LISTING_RISK_REGISTER.md](LISTING_RISK_REGISTER.md) — explicit reasons a venue may decline/defer QUINTUM and the evidence required to reduce each risk.

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
- Independent third-party audit: **not yet**
- Production Mainnet explorer: **not yet**
- MiCA/XBRL package: **not yet**
- Current human team: **one founder**
- Company: **none currently**
- Project legal documents: **none currently**

## Exchange-requirements policy

Exchange listing requirements are treated as moving external requirements. The project maintains a dated register from official exchange sources and must re-check the target venue immediately before any application is submitted.

No paid broker, unofficial Telegram contact, unsolicited "listing agent" or unverifiable message is accepted as authoritative. Private KYC/legal/contact information must not be committed to this public repository.

An exchange application may only contain claims that can be supported by the current source code, Mainnet release manifest, public-chain data, legal documentation or dated project metrics.

## Documentation maintenance rule

Every consensus change, networking change, wallet change, security fix, release artifact change, public-network verification and production-readiness decision must be reflected in this pack. Source-level change provenance is preserved by Git history; exchange-facing implications and verification results are recorded in the relevant files here.

Every material change should be checked for effects on:

- technical due diligence;
- exchange integration;
- listing application answers;
- supply/distribution;
- security/audit statements;
- regulatory/MiCA disclosures;
- node/RPC instructions;
- confirmation/reorg policy;
- release hashes/signatures;
- current exchange readiness.

No consensus-critical parameter may be represented to an exchange as final while the repository marks it as a candidate or pre-mainnet value.

## Submission status

**Do not submit QUINTUM as a production Mainnet listing yet.**

The readiness matrix must move to READY only after Mainnet launch, final parameter/ticker freeze, exchange-grade integration, explorer, release signing, security review, legal/regulatory preparation and reproducible Mainnet supply evidence are in place.
