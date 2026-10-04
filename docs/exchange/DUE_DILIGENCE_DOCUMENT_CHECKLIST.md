# QUINTUM Exchange Due-Diligence Document Checklist

Status: **LIVE DOCUMENT — PRE-MAINNET**  
Last review: **2026-10-04**

Purpose: maintain the exact document set that may be requested by centralized exchanges, custodians, market-data providers, banking/compliance counterparties and regulators.

Current factual status:

- one founder;
- no additional human team members currently;
- no company currently;
- no project legal documents currently;
- Mainnet not launched;
- public Testnet active.

Private identity/KYC material must not be committed to the public repository.

---

## A. Founder / applicant identity pack — PRIVATE

Potentially requested items:

- government-issued identity document;
- proof of residential address;
- date/place of birth;
- nationality/citizenship information;
- CV / professional biography;
- source-of-funds/source-of-wealth information where required;
- sanctions/PEP screening information;
- selfie/liveness/video verification where required;
- signed applicant declarations;
- official email and verified communication handle;
- authorization to represent QUINTUM.

Current status: **NOT STORED IN PUBLIC REPOSITORY / PREPARE ONLY FOR VERIFIED OFFICIAL REQUESTS**

Rules:
- never publish passport/ID scans in GitHub;
- never send KYC to unofficial agents;
- store encrypted/private copies only;
- log the official recipient and submission date.

---

## B. Company / legal documents — PRIVATE, CURRENTLY ABSENT

Current QUINTUM statement:

> No company currently exists for the project and no project legal documents currently exist.

If a target exchange or jurisdiction requires a company, the exact requested package may include:

- company registration/incorporation certificate;
- constitutional/formation documents;
- registered address evidence;
- directors/officers register;
- ownership / ultimate beneficial owner declaration;
- authorized-signatory evidence;
- tax registration information;
- good-standing/incumbency documents;
- board/shareholder authorization for listing or contracting;
- legal opinions;
- signed exchange agreement/NDA;
- compliance declarations.

Current status: **MISSING — create only if/when required by actual project structure and target venue**

Do not fabricate or backdate any document.

---

## C. Project identity package — PUBLIC

Required/expected:

- project name: QUINTUM;
- final ticker;
- official website;
- official repository;
- whitepaper;
- technical specification;
- brand/logo assets;
- official social links;
- official contact channels;
- project history and launch chronology;
- founder/project overview;
- license and trademark policy.

Current status: **PARTIAL**

Already available:
- public GitHub repository;
- MIT license;
- trademark/brand policy;
- technical due-diligence pack;
- complete development provenance.

Still required:
- final website;
- final whitepaper;
- final verified social-link register;
- final ticker.

---

## D. Mainnet / technical integration package — PUBLIC

Required:

- final Mainnet release version;
- source commit SHA;
- Genesis hash;
- Genesis timestamp/nonce/Merkle root;
- message magic;
- P2P port;
- RPC/API port;
- address format/HRP;
- block time;
- difficulty rules;
- max block/transaction limits;
- node installation guide;
- system requirements;
- bootstrap/seed/DNS information;
- synchronization guide;
- shutdown/upgrade procedure;
- API/RPC reference;
- address validation;
- block lookup;
- transaction lookup;
- mempool lookup;
- broadcast endpoint;
- balance/UTXO integration primitives;
- health/sync status;
- error model;
- version compatibility policy;
- signed release manifest;
- hashes/signatures for binaries.

Current status: **PARTIAL — final Mainnet package pending**

---

## E. Deposit / withdrawal integration package — PUBLIC/EXCHANGE

Required:

- deposit-address generation/validation;
- address network separation;
- minimum deposit amount recommendation;
- transaction fee behavior;
- confirmation counting;
- recommended confirmation threshold;
- unusually large deposit policy;
- reorg detection;
- orphan/replaced transaction handling;
- node desynchronization behavior;
- withdrawal creation/signing/broadcast;
- stuck transaction handling;
- hot/cold wallet recommendations;
- key-backup/recovery guidance;
- maintenance window procedure;
- chain halt procedure;
- emergency pause guidance;
- test vectors/examples.

Current status: **MISSING/PARTIAL — production integration must be implemented and tested**

---

## F. Tokenomics / monetary-policy package — PUBLIC

For a native PoW coin, prepare:

- maximum monetary range;
- initial subsidy;
- halving interval;
- emission formula;
- atomic-unit precision;
- coinbase maturity;
- fee treatment;
- Genesis allocation;
- premine statement;
- founder/team/foundation allocation;
- private/public sale allocation;
- treasury allocation;
- mining distribution model;
- circulating-supply methodology;
- total-supply methodology;
- reference-height/hash supply proof;
- burn rules, if any;
- supply reconciliation workbook.

Current QUINTUM status:
- candidate monetary policy documented;
- no hidden premine;
- no developer master mint;
- Genesis creator allocation: none;
- final Mainnet evidence pending.

---

## G. Genesis / provenance package — PUBLIC

Required:

- Genesis construction description;
- timestamp;
- message/anchor;
- nonce;
- bits/target;
- Merkle root;
- block hash;
- coinbase output details;
- proof that Genesis creator allocation is unspendable/zero;
- source location;
- deterministic reconstruction test;
- project inception chronology;
- Git commit history.

Current status: **PARTIAL/STRONG — candidate/testnet provenance documented; final Mainnet freeze pending**

---

## H. Security package — PUBLIC + PRIVATE AS NEEDED

Potentially requested:

- independent audit report;
- remediation report;
- dependency inventory;
- cryptographic design summary;
- wallet security model;
- private-key custody model;
- key-generation entropy source;
- storage encryption;
- file permissions;
- consensus attack-surface analysis;
- P2P abuse/DoS controls;
- reorg handling;
- release-signing procedure;
- vulnerability disclosure policy;
- security contact;
- bug-bounty program;
- incident-response plan;
- previous security incidents;
- CVE/advisory history;
- penetration/adversarial test evidence.

Current status: **PARTIAL**
Major missing item: independent third-party audit.

---

## I. Network-resilience package — PUBLIC

Prepare dated evidence for:

- public full-node count;
- reachable seed/bootstrap nodes;
- independent providers;
- geographic diversity;
- DNS seed availability;
- peer discovery;
- inbound/outbound behavior;
- restart/reconnect behavior;
- long-offline catch-up;
- multi-node convergence;
- equal-height fork handling;
- longer/heavier fork reorg;
- node data corruption/recovery behavior;
- block propagation;
- transaction propagation;
- network hash rate;
- observed block-interval distribution;
- chain-stall history;
- reorg/orphan history.

Current status: **Testnet evidence exists; production Mainnet evidence pending**

---

## J. Build / release package — PUBLIC

Required:

- source tag/version;
- release commit SHA;
- reproducible build instructions;
- build dependencies/versions;
- CI evidence;
- Windows binary;
- Linux binary;
- checksums;
- digital signatures;
- release manifest;
- SBOM where appropriate;
- installer behavior;
- upgrade compatibility;
- wallet/blockchain preservation tests;
- previous release support policy;
- security hotfix procedure.

Current status: **PARTIAL**
Current pre-alpha Windows installer is unsigned.

---

## K. Block explorer / public verification package — PUBLIC

Required:

- production explorer URL;
- operator information;
- blocks page;
- transaction page;
- address/UTXO visibility as applicable;
- supply statistics;
- network status;
- API if available;
- synchronization status/health;
- consistency checks against independent full nodes.

Current status: **MISSING for production Mainnet**

---

## L. Legal / regulatory package — PRIVATE/PUBLIC DEPENDING ON DOCUMENT

Potentially requested:

- project legal classification memorandum/opinion;
- jurisdiction analysis;
- offering/admission-to-trading analysis;
- AML/sanctions risk analysis;
- consumer-risk disclosures;
- privacy/data-processing documentation;
- marketing/financial-promotion review;
- conflict-of-interest disclosure;
- fundraising facts;
- investor/financing disclosure;
- MiCA whitepaper where applicable;
- XBRL package where applicable;
- proof of filing/notification/publication where applicable;
- sustainability/environmental disclosures where applicable.

Current status: **MISSING**
No project legal documents currently exist.

---

## M. Funding / treasury package — PUBLIC/PRIVATE

Prepare truthful answers for:

- self-funded / externally funded;
- funding rounds;
- investor names where disclosure is required/permitted;
- amount raised;
- use of funds;
- treasury holdings;
- treasury addresses where applicable;
- founder allocation;
- vesting;
- locked allocations;
- market-making funding;
- conflicts of interest.

Current status: **TBD — answer only with verified facts**

No investor, partner or funding relationship may be invented.

---

## N. Market / liquidity package — PUBLIC/PRIVATE

Potential exchange requests:

- requested trading pair(s);
- initial liquidity plan;
- market maker, if any;
- market-maker agreement/details where requested;
- liquidity source;
- existing exchange listings;
- price history;
- 24h/30d volume;
- order-book depth;
- OTC activity;
- CoinMarketCap/CoinGecko links;
- anti-manipulation controls;
- market-launch plan.

Current status: **NOT YET AVAILABLE**

Rules:
- no wash trading;
- no fake volume;
- no fictitious market maker;
- no guaranteed-price language.

---

## O. Adoption / community package — PUBLIC

Prepare dated evidence for:

- official social accounts;
- follower counts;
- engagement metrics;
- repository contributors/stars/forks;
- release downloads where accurately measurable;
- node count;
- wallet users where measurable without hidden telemetry;
- active/funded addresses;
- transaction activity;
- mining participation;
- community channels;
- partnerships only when verifiable.

Current status: **EARLY / PRE-MAINNET**

Do not buy fake followers or present promotional reach as active blockchain users.

---

## P. Governance / maintenance package — PUBLIC

Required:

- who maintains the reference implementation;
- how releases are approved;
- upgrade process;
- consensus-change process;
- emergency-fix process;
- how users are notified;
- fork policy;
- deprecation/support policy;
- continuity plan;
- key-person risk disclosure.

Current status:
- solo-founder project;
- public source history;
- final Mainnet governance/upgrade policy still required.

---

## Q. Exchange operational contacts — PRIVATE/PUBLIC ALIAS

Prepare:

- responsible applicant;
- technical integration contact;
- 24/7 emergency/security contact;
- business/listing contact;
- backup contact when one actually exists;
- PGP/signing keys where used;
- official email domain once website/domain is established.

Current status: **PARTIAL**
Do not invent backup staff.

---

## R. Regulatory / EEA-specific package

Where applicable:

- MiCA-compliant whitepaper;
- XBRL version under applicable taxonomy;
- explanatory notes;
- NCA notification/submission evidence;
- responsible legal/person/company details as legally required;
- sustainability disclosures;
- conflicts/risk disclosure;
- version/hash of final regulatory package.

Current status: **PREPARATION ONLY — no claim of MiCA compliance/approval**

---

## S. Listing application archive — PRIVATE

For every exchange application preserve:

- exchange name;
- official application URL;
- submission date/time UTC;
- applicant;
- submitted form export/screenshots;
- document versions/hashes;
- NDA reference;
- exchange case/reference number;
- reviewer contacts after independent verification;
- additional questions and responses;
- decision/status;
- technical integration requirements;
- listing agreement;
- launch/maintenance obligations;
- post-listing incident contacts.

Current status: **NO PRODUCTION APPLICATION YET**

---

## T. Current high-priority missing documents

Before serious major-exchange submission, create/obtain:

1. final Mainnet whitepaper;
2. Mainnet technical specification;
3. final Genesis/network manifest;
4. exchange-grade RPC/API specification;
5. deposit/withdrawal integration guide;
6. Mainnet explorer;
7. independent security audit;
8. vulnerability disclosure policy;
9. incident-response policy and security contact;
10. signed/reproducible production release package;
11. circulating-supply methodology + evidence;
12. legal/regulatory review for target venues;
13. company/legal documents only where the actual listing route requires them;
14. dated Mainnet network/adoption metrics;
15. liquidity/market plan if required.

## Accuracy rule

A document is not considered "ready" merely because a template exists. It becomes ready only when:
- its facts are final/current;
- it matches the running Mainnet implementation;
- supporting evidence exists;
- private/legal material has been reviewed by the appropriate professional where required;
- the exact version submitted to an exchange is archived.
