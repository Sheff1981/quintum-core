# QUINTUM Exchange Readiness Matrix

Status: **PRE-MAINNET / NOT READY FOR PRODUCTION LISTING**  
Last review: **2026-10-04**

This matrix translates public exchange requirements into concrete QUINTUM deliverables. It is a project-control document, not a claim of exchange approval.

Legend:

- **DONE** — implemented/documented and evidenced at current pre-Mainnet level;
- **PARTIAL** — exists, but not yet at production/Mainnet quality;
- **MISSING** — not yet delivered;
- **N/A NOW** — not applicable before Mainnet/market launch.

| Deliverable | State | Evidence / next action |
|---|---|---|
| Public source repository | DONE | https://github.com/Sheff1981/quintum-core |
| Open-source license | DONE | MIT |
| Brand/trademark policy | DONE | TRADEMARKS.md |
| Independent native Layer-1 | DONE | Core implementation and technical due-diligence |
| PoW consensus | DONE | Implemented/tested |
| UTXO validation | DONE | Implemented/tested |
| Chainwork fork selection | DONE | Implemented/tested |
| Reorg support | DONE | Implemented/tested |
| Persistent chainstate | DONE | Implemented/tested |
| Mempool | DONE | Implemented/tested |
| P2P headers/block/tx relay | DONE | Implemented; live Testnet evidence |
| Automatic public bootstrap | PARTIAL | One documented public Testnet seed; production redundancy pending |
| Multiple independent seed regions/providers | MISSING | Required before Mainnet maturity |
| DNS seeds | MISSING | Required before production network maturity |
| Production peer-abuse hardening | PARTIAL | More soak/adversarial testing required |
| Windows desktop wallet | DONE | Pre-alpha implementation |
| Encrypted wallet storage | DONE | Implemented |
| Reproducible release/build evidence | PARTIAL | CI exists; production release process not frozen |
| Signed production binaries | MISSING | Code-signing/release-signing required |
| Final Mainnet Genesis | PARTIAL | Candidate exists; not frozen/launched |
| Final Mainnet ticker | MISSING | QMU is working ticker only |
| Final monetary-policy freeze | PARTIAL | Candidate documented; must freeze before launch |
| No-premine evidence | DONE | Current architecture/Genesis has no spendable creator allocation |
| Final circulating-supply methodology | PARTIAL | Method defined conceptually; Mainnet evidence pending |
| Production Mainnet launch | MISSING | Required |
| Production block explorer | MISSING | Required |
| Stable exchange RPC/API | MISSING | Required |
| Deposit scanner/reference integration | MISSING | Required |
| Withdrawal reference integration | MISSING | Required |
| Address validation API | PARTIAL | Core format exists; production interface pending |
| Confirmation policy | PARTIAL | Coinbase maturity exists; exchange deposit depth requires network-risk testing |
| Reorg operational runbook | PARTIAL | Technical behavior documented; exchange runbook still to finalize |
| Node upgrade procedure | PARTIAL | Installer/release path exists; production exchange procedure pending |
| Emergency/security contact | MISSING | Public security alias/process required |
| Vulnerability disclosure policy | MISSING | Required before production maturity |
| Bug bounty | MISSING | Recommended |
| Independent third-party security audit | MISSING | High-priority listing gate |
| Final whitepaper | MISSING | Mainnet freeze required first |
| Legal classification analysis | MISSING | Qualified counsel required per jurisdiction |
| MiCA whitepaper/XBRL package | MISSING | Required for relevant EEA listing routes |
| Public website | MISSING / VERIFY | Add only after official URL is established |
| Official social link register | PARTIAL | GitHub public; other verified official links must be registered |
| Community/adoption metrics | N/A NOW | Collect dated Mainnet evidence later |
| Mainnet hash-rate history | N/A NOW | Collect after launch |
| Mainnet node-count history | N/A NOW | Collect after launch |
| Mainnet transaction/activity history | N/A NOW | Collect after launch |
| Existing market/liquidity evidence | N/A NOW | No production market yet |
| CoinMarketCap / CoinGecko | N/A NOW | Apply when eligibility criteria are met |
| Market-maker/liquidity plan | MISSING | Needed for many practical CEX launches, if used |
| Private founder/team due-diligence pack | MISSING / PRIVATE | Prepare founder-only KYC/due-diligence pack off-repo; do not invent additional team members |
| Company / legal documents | MISSING / PRIVATE | No company and no project legal documents currently exist; this is a blocker wherever the venue or jurisdiction requires them |
| Exchange master application | DONE | Public-safe draft exists; private fields pending |
| Listing request templates | DONE | Draft templates exist |
| Exchange-specific requirements register | DONE | Current major CEX requirements recorded |

## Exchange-specific status

| Exchange | Public process researched | QUINTUM submission status | Principal blockers |
|---|---|---|---|
| Binance | YES | DO NOT SUBMIT YET | Mainnet, adoption data, audit, final legal/technical pack |
| Coinbase | YES | DO NOT SUBMIT YET | Mainnet, production L1 integration API, explorer, audit, legal/compliance |
| Kraken | YES | DO NOT SUBMIT YET | Mainnet, metrics, legal/compliance, EEA MiCA where applicable |
| OKX | YES | DO NOT SUBMIT YET | Mainnet, final tokenomics, team/funding disclosures, ecosystem metrics |
| Bybit | YES | DO NOT SUBMIT YET | Mainnet and complete private/project application package |
| KuCoin | YES | DO NOT SUBMIT YET | Mainnet, full documentation, private review package |
| Gate | YES | DO NOT SUBMIT YET | Mainnet, whitepaper, technical/legal/market due diligence |
| MEXC | YES | DO NOT SUBMIT YET | Mainnet explorer/site/socials/market references and private team data |
| Bitget | YES | DO NOT SUBMIT YET | Mainnet, security/compliance, ecosystem/network-effect evidence |

## Hard listing gate

No document may change QUINTUM's status to **READY TO SUBMIT** until all of these are true:

1. Mainnet consensus and network parameters are frozen.
2. Mainnet is actually live.
3. The final ticker is confirmed.
4. At least several independently hosted public full/seed nodes are operating.
5. A production explorer is live.
6. Exchange-grade node/RPC integration is stable and documented.
7. Deposit/withdraw and reorg tests pass.
8. Official release binaries are reproducibly built, hashed and signed.
9. An independent security review has been completed or its absence is explicitly accepted by the target venue.
10. Legal/regulatory review for the target venue's jurisdiction is complete.
11. Supply/distribution/circulating-supply figures can be reproduced from Mainnet.
12. All claims in the listing application are supported by current evidence.

## After Mainnet launch: evidence collection

The project must begin preserving dated snapshots for:

- chain height and tip hash;
- network hash rate;
- reachable full-node count and methodology;
- geographic/provider diversity where measurable;
- transaction count and active-address metrics;
- funded-address count;
- block interval distribution;
- orphan/reorg events;
- software version distribution where measurable without invasive telemetry;
- exchange deposits/withdrawals incident history;
- security advisories;
- release adoption;
- community metrics from official accounts.

No hidden telemetry is to be added simply to generate listing statistics. Metrics should come from public-chain data, voluntary/public node observations and official public channels.
