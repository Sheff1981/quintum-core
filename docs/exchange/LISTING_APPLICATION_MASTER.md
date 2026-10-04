# QUINTUM Master Exchange Listing Application

Status: **DRAFT / PRE-MAINNET — DO NOT SUBMIT AS A PRODUCTION LISTING APPLICATION YET**  
Language: English  
Purpose: one canonical answer sheet from which exchange-specific forms can be completed.

This document contains only public-safe project information and placeholders. Personal KYC data, identity documents, private phone numbers, private Telegram handles, legal opinions under NDA, banking information and confidential counterparty correspondence must never be committed to this public repository.

---

## A. Project identity

**Project name:** QUINTUM  
**Asset type:** Native coin of an independent Layer-1 blockchain  
**Current working ticker:** QTM  
**Final Mainnet ticker:** Not yet frozen  
**Consensus:** Proof of Work  
**Ledger model:** UTXO  
**Primary implementation:** C++23  
**License:** MIT for source code; QUINTUM name/logo/brand rights are separately reserved  
**Official source repository:** https://github.com/Sheff1981/quintum-core  
**Mainnet status:** Not launched  
**Current development network:** Public Testnet / pre-alpha  
**Premine:** None  
**Developer/master mint key:** None  
**Genesis creator allocation:** None; the Genesis subsidy is deliberately unspendable

### One-sentence description

QUINTUM is an independent proof-of-work Layer-1 digital-cash network focused on fully validating nodes, UTXO accounting, permissionless mining, transparent monetary rules and a Bitcoin-like operational model without a hidden premine or privileged mint authority.

### Short project overview

QUINTUM is being built as a standalone decentralized cryptocurrency rather than as a token on an existing smart-contract chain. The implementation includes its own Genesis blocks, network identifiers, PoW consensus, difficulty adjustment, UTXO transaction model, secp256k1 signatures, mempool, persistent blockchain storage, headers-first P2P synchronization, transaction/block relay, fork/reorganization handling, wallet encryption, mining and a Windows desktop wallet. The project remains pre-Mainnet. Mainnet parameters and production exchange interfaces will be frozen only after public-network testing and security gates are completed.

---

## B. Official links

**Repository:** https://github.com/Sheff1981/quintum-core  
**Website:** [TO BE ADDED]  
**Block explorer:** [TO BE ADDED — production Mainnet explorer required]  
**Whitepaper:** [TO BE ADDED — final Mainnet whitepaper required]  
**Technical specification:** Repository documentation; final immutable Mainnet specification pending  
**X / Twitter:** [TO BE ADDED]  
**Instagram:** [TO BE ADDED]  
**Telegram:** [TO BE ADDED]  
**Discord:** [TO BE ADDED / N/A]  
**CoinMarketCap:** N/A — not yet listed  
**CoinGecko:** N/A — not yet listed

All social links must be verified before any application. Never invent or infer a handle.

---

## C. Technology and consensus

**Blockchain type:** Independent Layer-1  
**Consensus mechanism:** Proof of Work  
**Fork-choice rule:** Greatest cumulative valid proof of work  
**Hash family:** SHA-256; transaction/block identifiers use double-SHA-256  
**Transaction signatures:** ECDSA/secp256k1  
**Address format:** Network-separated Bech32m candidate format  
**Mempool:** Implemented  
**Persistent chain storage:** Implemented  
**Reorganizations:** Supported by cumulative-chainwork selection and staged rollback/connect  
**P2P:** Inbound/outbound peers, handshake, headers-first sync, block/transaction relay, persistent peers and seed bootstrap  
**Mining:** Real nonce search and consensus-validated coinbase blocks  
**Wallet:** Encrypted local wallet with deterministic recovery foundation  
**Smart contract:** Not applicable; QTM is the native coin of the QUINTUM chain

### Mainnet technical values

**Mainnet status:** Not frozen / not launched.  
Do not submit candidate values as final exchange integration parameters until the Mainnet release manifest is published.

The current candidate/testnet technical details are maintained in:
- `TECHNICAL_DUE_DILIGENCE.md`
- `EXCHANGE_INTEGRATION.md`
- source-level consensus constants and tests.

---

## D. Monetary policy

Current Mainnet candidate policy:

**Precision:** 8 decimal places  
**Initial block subsidy:** 50 QTM candidate  
**Halving interval:** 210,000 blocks candidate  
**Money-range ceiling:** 21,000,000 QTM candidate  
**Coinbase maturity:** 100 blocks candidate  
**Genesis spendable allocation:** 0  
**Premine:** 0  
**Private sale allocation:** 0 unless a future public disclosure explicitly changes project policy before Mainnet  
**Foundation allocation:** 0 unless a future consensus/public launch plan explicitly changes this before freeze  
**Developer mint authority:** None

### Distribution statement

The intended distribution model is permissionless proof-of-work issuance through valid coinbase transactions according to consensus rules. No hidden developer balance, master key or out-of-consensus issuance path exists in the current architecture.

### Circulating-supply methodology

Before Mainnet launch: **0 Mainnet QTM circulating.**  
Testnet balances are not Mainnet supply and have no monetary value.

After Mainnet launch, circulating supply must be calculated from the validated Mainnet chain, excluding permanently unspendable Genesis output and any later provably burned outputs according to the final published methodology.

---

## E. Genesis and launch

**Mainnet Genesis:** Candidate exists, but Mainnet is not launched and final release status is not yet frozen.  
**Public Testnet:** Active.  
**Mainnet launch date:** [TBD]  
**Token Generation Event:** Not a smart-contract TGE. Native issuance begins from the Layer-1 Mainnet Genesis and subsequent PoW blocks.  
**Airdrop:** None currently planned.  
**ICO / presale:** None currently disclosed.  
**Public sale:** None currently disclosed.

A future application must attach:

- final Genesis hash;
- timestamp;
- nonce;
- Merkle root;
- network magic;
- P2P/RPC ports;
- final address HRP/prefix;
- final subsidy/halving schedule;
- release manifest;
- SHA256 hashes and signatures for official binaries.

---

## F. Team and governance

**Current team structure:** Solo founder / single-person project  
**Employees:** None currently disclosed  
**Co-founders:** None  
**Project founder / responsible applicant:** [PRIVATE DUE-DILIGENCE FIELD — DO NOT COMMIT PERSONAL DATA]  
**Role/title:** Founder / project owner; currently responsible for project direction, development coordination, releases and exchange-facing preparation  
**Legal entity:** [TBD / PRIVATE WHEN CREATED]  
**Jurisdiction:** [TBD]  
**Technical contact:** Founder unless a dedicated technical contact is appointed later  
**Security contact:** Founder until a dedicated public security alias/process is created  
**Business/listing contact:** Founder unless formally delegated

QUINTUM must not invent a team for listing purposes. Until additional contributors, employees, contractors or advisers actually exist and are disclosure-appropriate, exchange applications must describe the project as a solo-founder project. Use of development tools, automation or AI-assisted engineering does not create fictitious human team members and must not be represented as such.

### Governance model

The current network architecture does not contain a protocol-level administrator who can arbitrarily rewrite balances or mint coins. Consensus-validating nodes enforce the same rules. Development before Mainnet remains controlled through the public software repository; final Mainnet upgrade/governance policy must be documented before launch.

### Team disclosure rule

Where an exchange asks for "team members", "core team" or "founders", answer truthfully that QUINTUM currently has one founder and no additional human team members. Do not pad the application with fictitious CTO/CMO/adviser roles. If an exchange requires identity/KYC, provide the founder's accurate information privately through the official exchange portal. Do not publish passports, addresses, private telephone numbers or similar KYC information in GitHub.

---

## G. Funding and financial transparency

**Venture funding:** [TBD / disclose truthfully]  
**Private investors:** [TBD / disclose truthfully]  
**Treasury:** [TBD; no hidden on-chain allocation]  
**Fundraising history:** [TBD / N/A]  
**Capital allocation:** [TBD]

If the project remains self-funded, state that plainly. Do not invent investors, partnerships, valuations or funding rounds.

---

## H. Product status and milestones

Completed/observed development areas include:

- independent blockchain and Genesis construction;
- Proof of Work;
- UTXO transaction validation;
- secp256k1 signing;
- difficulty rules;
- persistent blockchain/chainstate;
- mempool;
- headers-first P2P synchronization;
- transaction and block relay;
- chainwork-based fork/reorg handling;
- encrypted wallet storage;
- deterministic recovery foundation;
- Qt desktop wallet;
- Windows packaging/installer;
- public Testnet bootstrap;
- live multi-node relay/reconnect/synchronization testing.

Current state is pre-alpha/public Testnet. The complete development history is retained in Git and summarized in `DEVELOPMENT_HISTORY.md`.

---

## I. Security

**Independent third-party audit:** Not yet completed  
**Bug bounty:** [TO BE DEFINED]  
**Security disclosure policy:** [TO BE PUBLISHED]  
**Code signing:** Current pre-alpha Windows installer is unsigned; production signing is required before release maturity  
**Private key custody:** User-controlled; wallet private keys are not sent through P2P  
**Wallet encryption:** Implemented  
**Consensus privileged mint path:** None

Known limitations and unresolved production gates must be disclosed exactly as listed in `TECHNICAL_DUE_DILIGENCE.md` and `VERIFICATION_LOG.md`.

---

## J. Network and market metrics

These fields must be filled with dated, reproducible evidence at application time.

**Mainnet active nodes:** [TBD]  
**Mainnet reachable public nodes:** [TBD]  
**Independent infrastructure providers/regions:** [TBD]  
**Unique funded addresses:** [TBD]  
**Daily active addresses:** [TBD]  
**Daily transactions:** [TBD]  
**Hash rate:** [TBD]  
**Chain height:** [TBD]  
**Market capitalization:** N/A before trading  
**24h volume:** N/A before trading  
**Liquidity:** N/A before trading  
**Number of holders:** N/A before Mainnet distribution  
**TVL:** N/A unless QUINTUM later has an applicable on-chain ecosystem metric  
**Community size:** [TBD with dated platform-specific evidence]

Never substitute Testnet statistics for Mainnet adoption statistics without clearly labeling them as Testnet.

---

## K. Competitive positioning

### Category

Independent PoW Layer-1 / digital cash.

### Comparable networks

Bitcoin and Litecoin are architectural reference points for conservative PoW/UTXO/P2P design principles. QUINTUM is an independent implementation and network; it is not affiliated with either project.

### Intended differentiators

Only factual, implemented differences may be stated. Marketing claims must be revised against the final Mainnet implementation before submission. Avoid unverifiable claims such as "fastest", "safest", "best", guaranteed appreciation or investment-return language.

---

## L. Exchange technical integration

The final integration package must include:

- supported OS/platforms;
- official release download and signature verification;
- system requirements;
- node startup and shutdown;
- data directory;
- network ports;
- firewall guidance;
- seed/bootstrap behavior;
- node health endpoint;
- blockchain sync status;
- address generation/validation;
- transaction creation/signing/broadcast;
- transaction lookup;
- block lookup;
- mempool lookup;
- balance/UTXO queries required by exchange architecture;
- fee selection;
- confirmation counting;
- reorg handling;
- wallet hot/cold separation recommendations;
- deposit scanning;
- withdrawal construction;
- backup/recovery;
- version upgrade procedure;
- emergency/security contact;
- chain halt/reorg incident procedure.

Current production exchange RPC/API is **not yet frozen**.

---

## M. Listing market proposal

**Requested market pair(s):** [TBD — e.g. QTM/USDT only after ticker freeze]  
**Preferred listing date:** [TBD]  
**Deposit opening date:** [TBD]  
**Withdrawal opening date:** [TBD]  
**Market maker:** [TBD / disclose accurately]  
**Initial external liquidity:** [TBD]  
**Existing exchanges:** N/A unless listed later

No artificial volume, wash trading or fabricated liquidity metrics may be represented as organic market demand.

---

## N. Legal and regulatory

Required before serious production listing applications:

- jurisdiction-by-jurisdiction legal review;
- classification analysis for the native coin;
- sanctions/AML risk analysis requested by the exchange;
- privacy/data-processing review where relevant;
- MiCA whitepaper and XBRL package for applicable EEA listing paths;
- risk disclosures;
- legal entity/issuer/offering-person information where required.

This repository does not provide a legal opinion. Final legal documents must be prepared/reviewed by qualified counsel for the target jurisdictions.

---

## O. Standard risk disclosure

QUINTUM is experimental pre-Mainnet software. Mainnet has not launched. Candidate parameters may still change before consensus freeze. Testnet coins have no monetary value. Software defects, chain reorganizations, networking failures, key loss, market volatility, regulatory changes and other risks may affect a future Mainnet asset. No statement in project documentation is a promise of profit or price appreciation.

---

## P. Attachments checklist for a future submission

- [ ] Final whitepaper
- [ ] Mainnet technical specification
- [ ] Exchange integration guide
- [ ] Final Genesis/network manifest
- [ ] Source repository
- [ ] Reproducible release instructions
- [ ] Signed release + checksums
- [ ] Mainnet block explorer
- [ ] Third-party security audit
- [ ] Security disclosure / bug-bounty policy
- [ ] Legal opinion(s), where required
- [ ] MiCA/XBRL package, where required
- [ ] Team/KYC pack, private
- [ ] Corporate/legal-entity documents, private
- [ ] Tokenomics/supply workbook
- [ ] Circulating-supply methodology
- [ ] Network statistics evidence
- [ ] Community statistics evidence
- [ ] Existing-market/liquidity evidence
- [ ] Brand kit/logo assets
- [ ] Technical emergency contact
- [ ] Business/listing contact

## Accuracy rule

Every answer copied from this master into an exchange form must be revalidated against the current source code, Mainnet release manifest, public explorer and legal documentation on the day of submission.
