# QUINTUM MiCA / EEA Listing Preparation

Status: **PREPARATION CHECKLIST — NOT LEGAL ADVICE**  
Last source review: **2026-10-04**

Purpose: keep QUINTUM technically prepared for exchange requests involving the EU Markets in Crypto-Assets Regulation (MiCA), while leaving legal conclusions and formal filings to qualified counsel and the competent authority process.

## Current exchange signal

Kraken's current EEA asset-listing guidance states that crypto-assets listed in the EEA must have a MiCA-compliant whitepaper and accompanying explanatory information. Kraken further states that, under the current ESMA implementation timeline, whitepapers must be produced in XBRL format and submitted to a National Competent Authority in advance of the intended listing date.

Relevant Kraken source reviewed:  
https://support.kraken.com/in/articles/kraken-asset-listing-whitepapers

OKX's current listing guidance also asks projects to keep regulatory materials, including a MiCA whitepaper where relevant, current during review:

https://www.okx.com/en-eu/help/how-can-i-get-my-project-listed-on-okx

This document does not decide whether a particular QUINTUM offering/admission scenario falls into a specific MiCA category. That determination must be made for the actual launch/listing structure by qualified counsel.

---

## 1. Public technical source material already available

The following public materials are intended to become source evidence for a future regulatory whitepaper:

- project identity and repository provenance;
- Genesis provenance;
- consensus algorithm;
- UTXO model;
- PoW and difficulty rules;
- monetary policy;
- transaction authorization;
- address format;
- chain selection/reorganization;
- storage/recovery;
- P2P/networking;
- wallet architecture;
- security controls;
- release/build evidence;
- known limitations;
- development history;
- no-premine/no-master-mint disclosure.

Canonical technical documents:

- `TECHNICAL_DUE_DILIGENCE.md`
- `EXCHANGE_INTEGRATION.md`
- `VERIFICATION_LOG.md`
- `DEVELOPMENT_HISTORY.md`

---

## 2. Information that must be frozen before drafting a final regulatory whitepaper

- final Mainnet ticker;
- final Mainnet Genesis;
- final network parameters;
- final monetary policy;
- final address format;
- final project/operator/legal-entity structure;
- final launch date and distribution mechanics;
- final website and official communications channels;
- final risk disclosures;
- final use/utility description;
- final governance/upgrade policy;
- any treasury/foundation/developer allocation, if one ever exists;
- any fundraising terms, if any fundraising ever occurs;
- intended jurisdictions and trading venues.

Until these are frozen, any MiCA document is a draft only.

---

## 3. Regulatory data room to prepare privately

Do not store personal/KYC/confidential legal records in the public repository.

Private data-room sections should include, where applicable:

### Project/operator identity
- company name, if one exists/is required;
- registration number, if applicable;
- registered office;
- directors/authorized representatives;
- beneficial ownership information where requested;
- responsible contact;
- legal counsel contact.

### Asset description
- native Layer-1 status;
- technology and protocol description;
- rights/obligations attached to the coin;
- utility and intended uses;
- transferability;
- custody characteristics;
- consensus and security model.

### Offering/admission details
- whether there is any offer to the public;
- admission-to-trading plan;
- launch schedule;
- distribution method;
- allocation schedule;
- pricing method if applicable;
- fundraising, if any;
- target markets/jurisdictions.

### Financial/funding information
- project funding sources;
- capital allocation;
- treasury arrangements;
- conflicts of interest;
- material commercial relationships.

### Technology/security
- source repository;
- audit reports;
- dependency inventory;
- vulnerability-management process;
- incident-response plan;
- business-continuity/recovery process;
- node/release verification.

### Environmental/consensus information
Because QUINTUM uses Proof of Work, any sustainability/environmental disclosure required by the applicable MiCA/ESMA framework must be prepared from measured, supportable data rather than estimates presented as facts.

---

## 4. Whitepaper production workflow

1. Freeze the Mainnet technical specification.
2. Determine the relevant MiCA classification and responsible legal person with counsel.
3. Map the required regulatory fields to verified project evidence.
4. Draft the human-readable whitepaper.
5. Perform technical fact-check against source code and Mainnet.
6. Perform legal review.
7. Prepare the machine-readable/XBRL version required by the applicable taxonomy and ESMA guidance.
8. Validate the XBRL package technically.
9. Submit/notify the appropriate National Competent Authority according to the legally applicable procedure/timeline.
10. Preserve submission evidence and document version.
11. Provide the exact accepted/published version to the exchange.
12. Keep the regulatory document synchronized with material protocol/project changes.

Do not use an exchange-targeted marketing whitepaper as a substitute for the legally required regulatory document.

---

## 5. Technical fact-check gates

Before counsel receives a final technical data pack, verify:

- [ ] Genesis hash matches running Mainnet nodes
- [ ] chain ID/network magic match source
- [ ] P2P/RPC ports match official releases
- [ ] target block interval matches source
- [ ] difficulty rules match source
- [ ] subsidy/halving rules match source
- [ ] max supply figures are mathematically reproducible
- [ ] Genesis allocation statement is correct
- [ ] premine/distribution statement is correct
- [ ] address format matches official wallet/node
- [ ] signatures/hash algorithms match source
- [ ] coinbase maturity matches source
- [ ] reorg/fork-choice description matches source
- [ ] upgrade/governance description matches actual policy
- [ ] audit status is current
- [ ] all known material security issues are properly handled/disclosed
- [ ] Mainnet launch date/history is accurate
- [ ] circulating-supply methodology matches chain data

---

## 6. Risk topics to cover accurately

A final regulatory/legal whitepaper may need counsel-approved language covering risks such as:

- Proof-of-Work probabilistic finality;
- chain reorganizations;
- majority-hash-power attacks;
- software defects;
- P2P partition/eclipse/DoS risks;
- wallet/private-key loss;
- exchange/custody operational risks;
- mining centralization risks;
- insufficient early-network hash rate;
- protocol upgrade/fork risk;
- market/liquidity/volatility risk;
- regulatory change;
- cybersecurity incidents;
- dependency/supply-chain risk;
- loss of seed/explorer infrastructure;
- project-maintenance/key-person risk before governance decentralizes.

Risk language must not be minimized for marketing purposes.

---

## 7. Version-control rule

Every regulatory draft should carry:

- document version;
- UTC date;
- corresponding QUINTUM Core release;
- Git commit SHA;
- Mainnet Genesis hash;
- legal-review status;
- filing/submission status;
- hash of the final document package.

A regulatory whitepaper may not silently drift away from the implementation.

---

## 8. Current gaps

At 2026-10-04 the following are not yet available as final regulatory inputs:

- Mainnet launch;
- final ticker freeze;
- final Mainnet parameter freeze;
- final company/operator structure, if required;
- final jurisdictional legal analysis;
- final whitepaper;
- XBRL conversion/validation;
- NCA filing/submission;
- final environmental/sustainability metrics;
- Mainnet market/adoption data.

Therefore QUINTUM must currently be described as **MiCA preparation in progress**, not MiCA compliant/approved/notified.
