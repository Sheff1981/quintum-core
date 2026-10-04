# QUINTUM Exchange Listing Risk Register

Status: **LIVE DOCUMENT — PRE-MAINNET**  
Last review: **2026-10-04**

Purpose: identify the concrete reasons an exchange may decline, defer or terminate a QUINTUM listing review, and define the evidence required to reduce each risk.

Current project structure:

- one founder;
- no additional human team members currently;
- no company currently;
- no project legal documents currently;
- Mainnet not launched;
- public Testnet active.

This register is not a prediction of acceptance or rejection by any exchange. It is a readiness-control document.

## Risk scale

- **CRITICAL** — likely to block serious production listing review;
- **HIGH** — can materially reduce acceptance probability or delay integration;
- **MEDIUM** — important, but usually remediable during review;
- **LOW** — documentation/operational quality issue rather than a fundamental blocker.

---

## 1. Mainnet not launched

**Severity:** CRITICAL

Why it matters:
- there is no production chain for deposits/withdrawals;
- no Mainnet supply can be independently verified;
- no production hash-rate, node-count or transaction history exists;
- final integration parameters are not yet immutable.

Required remediation:
- freeze Mainnet consensus/network parameters;
- publish final Genesis and release manifest;
- launch Mainnet;
- preserve launch provenance;
- establish independent public nodes and explorer;
- collect dated network evidence.

Current state: **OPEN**

---

## 2. No company / no project legal documents

**Severity:** CRITICAL for venues or jurisdictions that require a company applicant; HIGH otherwise.

Current factual statement:

> QUINTUM is currently a solo-founder project. No company has been formed for the project and no project legal documents currently exist.

Why it matters:
- some exchanges require corporate due diligence;
- some jurisdictions require a legal person for admission-to-trading procedures;
- contracts, NDAs, indemnities, listing agreements and compliance attestations may require an identified contracting party;
- a venue may decide that an individual-only structure does not satisfy its risk framework.

Required remediation if demanded by the target venue:
- obtain jurisdiction-specific legal advice;
- establish the appropriate project/company structure;
- prepare the exact corporate/legal documents requested by that venue;
- keep private documents outside the public repository.

Important rule:
Do not invent a company, registration number, office, directors, advisers or legal opinions in any application.

Current state: **OPEN**

---

## 3. Solo-founder key-person risk

**Severity:** HIGH

Why it matters:
- development, release coordination, incident response and exchange communication currently depend heavily on one person;
- an exchange may assess continuity and operational resilience;
- absence of independent maintainers may increase perceived maintenance risk.

Mitigation without inventing a team:
- strong public Git history;
- reproducible builds;
- documented release/incident procedures;
- deterministic recovery and backups;
- multiple independent network nodes;
- external security review;
- public technical documentation;
- later add real contributors only when they actually exist.

Current state: **OPEN / ACCEPTED PROJECT FACT**

---

## 4. No independent security audit

**Severity:** CRITICAL/HIGH depending on venue.

Why it matters:
- QUINTUM is a new Layer-1;
- consensus, wallet, networking and persistence are all security-sensitive;
- exchanges expose custody and hot-wallet infrastructure to implementation risk.

Required remediation:
- complete internal security checklist;
- commission independent source review/security audit;
- track findings, fixes and retests;
- publish a non-sensitive audit summary/report where permitted;
- never label internal testing as independent audit.

Current state: **OPEN**

---

## 5. Production exchange RPC/API not frozen

**Severity:** CRITICAL

Why it matters:
- a venue cannot safely integrate deposits/withdrawals against an unstable interface;
- node upgrades must preserve documented operational behavior.

Required remediation:
- define stable production node API;
- document versioning;
- implement address validation, block/tx lookup, broadcast, sync status and required deposit-monitoring primitives;
- test exchange-style integration end-to-end;
- publish compatibility policy.

Current state: **OPEN**

---

## 6. No production block explorer

**Severity:** HIGH

Why it matters:
- exchanges and reviewers need independent visibility into blocks, transactions, addresses and supply;
- explorer links are commonly requested in application forms.

Required remediation:
- deploy Mainnet explorer;
- document operator and data source;
- verify explorer against full-node data;
- provide stable public URL.

Current state: **OPEN**

---

## 7. Insufficient network decentralization / bootstrap redundancy

**Severity:** HIGH

Current Testnet has limited bootstrap infrastructure.

Why it matters:
- exchange review may question resilience if the network depends on one server/provider;
- outages or censorship of a single node must not disable discovery.

Required remediation:
- multiple stable public nodes;
- provider and region diversity;
- DNS/bootstrap redundancy;
- peer discovery validation;
- long-duration outage/recovery tests.

Current state: **OPEN**

---

## 8. No long-duration Mainnet operating history

**Severity:** HIGH

Why it matters:
- no empirical Mainnet evidence yet for block intervals, reorg frequency, uptime, peer diversity or hash-rate resilience;
- exchanges often prefer observable maturity over test claims.

Required remediation:
- operate Mainnet before major applications;
- preserve dated metrics;
- document incidents transparently;
- establish a stable release cadence.

Current state: **OPEN**

---

## 9. Low adoption / weak user metrics

**Severity:** HIGH for major exchanges.

Why it matters:
- major venues evaluate actual demand and community traction;
- a technically correct chain can still be commercially unattractive to a venue.

Evidence to collect after Mainnet:
- active/funded addresses;
- transaction activity;
- reachable nodes;
- hash rate;
- wallet downloads where measurable without invasive telemetry;
- public community metrics;
- organic ecosystem activity.

Rules:
- do not buy fake followers;
- do not fabricate holders;
- do not wash trade;
- do not present Testnet usage as Mainnet adoption.

Current state: **NOT YET MEASURABLE**

---

## 10. No established market/liquidity

**Severity:** HIGH

Why it matters:
- an exchange needs credible price discovery and order-book liquidity;
- some venues will reject assets with no realistic liquidity path.

Required remediation:
- determine lawful launch/liquidity strategy;
- disclose market-maker relationships if any;
- document funding source for liquidity;
- avoid guaranteed-price or return claims;
- never fabricate volume.

Current state: **OPEN**

---

## 11. Legal/regulatory classification unresolved

**Severity:** CRITICAL/HIGH depending on jurisdiction.

Why it matters:
- venues need to understand whether listing the asset creates securities, financial-instrument, consumer, AML or other regulatory exposure;
- EEA routes may require MiCA-specific documentation and processes.

Required remediation:
- target-jurisdiction legal review;
- counsel-reviewed project facts;
- prepare required whitepaper/regulatory filings;
- document sanctions/AML and offering/distribution facts accurately.

Current state: **OPEN**

---

## 12. Final ticker not frozen

**Severity:** HIGH until freeze.

Why it matters:
- exchanges require a stable symbol;
- ticker conflicts can prevent or complicate listing;
- address/explorer/market data must use consistent identity.

Required remediation:
- perform ticker collision review;
- freeze final ticker before Mainnet/listing package;
- update all releases/documentation atomically.

Current state: **OPEN**

---

## 13. Unsigned production binaries

**Severity:** HIGH

Why it matters:
- exchanges need provenance and integrity for node software;
- unsigned desktop/node distributions increase supply-chain risk.

Required remediation:
- production release signing process;
- published SHA256 hashes;
- signed release manifest;
- reproducible build evidence;
- key custody/revocation procedure.

Current state: **OPEN**

---

## 14. Weak incident-response process

**Severity:** HIGH

Why it matters:
- exchanges require rapid notification for consensus bugs, chain stalls, reorgs and security incidents.

Required remediation:
- public security contact;
- private exchange emergency contacts;
- severity classification;
- pause/upgrade/recovery templates;
- canonical-chain verification procedure;
- post-incident reporting.

Current state: **PARTIAL — templates exist, operational contact process pending**

---

## 15. Incomplete supply/distribution evidence

**Severity:** CRITICAL for production listing.

Why it matters:
- an exchange must understand max supply, current supply, premine, allocations and circulating supply;
- inconsistencies are a major trust/compliance failure.

Required remediation:
- freeze monetary policy;
- publish deterministic supply formula;
- prove Genesis allocation;
- publish circulating-supply calculation methodology;
- verify figures against Mainnet at a reference height/hash.

Current state: **PARTIAL — candidate policy documented; Mainnet evidence pending**

---

## 16. Incomplete public identity / communications package

**Severity:** MEDIUM/HIGH.

Required production items:
- official website;
- official repository;
- verified official social accounts;
- security contact;
- listing/business contact;
- anti-phishing guidance.

Why it matters:
- reviewers must distinguish official communications from scams;
- users need a canonical source for releases and incidents.

Current state: **PARTIAL**

---

## 17. Documentation inconsistency

**Severity:** HIGH if consensus/financial claims conflict.

Why it matters:
- conflicting values for supply, Genesis, ticker, ports or consensus undermine due diligence.

Control:
- code/tests are authoritative;
- exchange pack updated with every material change;
- final application revalidated on submission date;
- release-specific manifest binds documentation to commit and Genesis.

Current state: **CONTROL ACTIVE**

---

## 18. Paid broker / impersonation risk

**Severity:** HIGH operational/fraud risk.

Controls:
- use official exchange application portals;
- verify representatives independently;
- do not transfer funds to unverifiable agents;
- do not share confidential KYC/audit material before verification;
- preserve private submission records.

Current state: **CONTROL ACTIVE**

---

## Overall current listing posture

### Major global CEX
**Not ready for production application.**

Primary blockers:
1. Mainnet not launched;
2. no company/legal package where required;
3. no independent audit;
4. no production exchange API;
5. no production explorer;
6. insufficient production network/adoption history;
7. legal/regulatory analysis incomplete;
8. liquidity/market evidence absent.

### Smaller or early-stage venues
Potentially lower entry barriers do not remove technical, security or legal risk. QUINTUM must not trade away core safeguards merely to obtain an early listing.

## Decision rule

A listing opportunity is rejected or deferred if it requires:
- false team information;
- fabricated company/legal documents;
- fake volume/followers/holders;
- hidden premine or privileged allocation;
- undisclosed paid promotion represented as organic adoption;
- private-key disclosure;
- consensus backdoor;
- misleading regulatory claims.
