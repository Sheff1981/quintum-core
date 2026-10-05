# QUINTUM Exchange Integration Guide

Status: **PRE-INTEGRATION DRAFT — MAINNET NOT LAUNCHED**

Technical state covered: `c5e29dcf45375d227872dd1a21c9127b214b1c8d`

## 1. Do not enable production deposits yet

QUINTUM Mainnet is not launched. Exchanges must not enable production deposits, withdrawals or trading based on the public Testnet or on the current Mainnet candidate constants.

The current purpose of this document is to define what an exchange will need and to keep the integration surface aligned with the node as it matures.

## 2. Asset identity

- blockchain type: independent Layer-1;
- ledger: UTXO;
- consensus: PoW;
- precision candidate: 8 decimals;
- current working GUI symbol: QMU;
- final Mainnet exchange ticker: not yet frozen;
- Mainnet address HRP candidate: `qmu`;
- Testnet address HRP: `tqtm`;
- Regtest address HRP: `rqmu`.

## 3. Network endpoints

Candidate Mainnet:

- P2P: TCP 28444
- RPC reservation/candidate: 28445

Public Testnet:

- P2P: TCP 38444
- RPC reservation/candidate: 38445
- current bootstrap: `212.193.15.139:38444`

A production exchange must run its own fully validating node. Seed nodes are discovery aids, not trusted consensus authorities.

## 4. Node trust model

An exchange node must independently verify:

- exact network Genesis;
- block PoW;
- contextual difficulty;
- timestamps and resource limits;
- transaction authorization;
- UTXO availability;
- double-spend rules;
- coinbase reward/maturity;
- cumulative chain work;
- fork/reorg rules.

An exchange must never infer deposits merely from a third-party seed or wallet UI.

## 5. Deposit model

QUINTUM uses UTXO accounting.

A future production integration will need to:

1. derive or import exchange-controlled deposit addresses;
2. detect relevant outputs in validated active-chain transactions;
3. track block hash and height of each credit;
4. track confirmations against the active chain;
5. invalidate or reclassify credits if a reorg removes the containing block;
6. credit only after the exchange's configured confirmation threshold;
7. retain enough block/transaction identity to audit each credit later.

The project will publish a recommended confirmation threshold only after public-Testnet reorg/soak data and final Mainnet security assumptions are available.

## 6. Withdrawal model

A future production withdrawal path must:

1. select exchange-controlled UTXOs;
2. create a transaction under current fee policy;
3. sign locally using exchange-controlled keys;
4. submit to a fully validating node;
5. verify mempool acceptance;
6. track relay and eventual block confirmation;
7. handle replacement/conflict policy only as explicitly supported by QUINTUM consensus/policy.

No privileged withdrawal or balance-adjustment RPC exists by design.

## 7. Coinbase handling

Coinbase outputs mature after 100 blocks.

This normally affects miners rather than exchange customer deposits, but exchange indexers must recognize coinbase transactions and must not treat immature coinbase outputs as spendable.

The Genesis subsidy is permanently unspendable and must never be included in circulating/spendable supply.

## 8. Reorganization handling

Fork choice uses greatest cumulative valid work, not height alone.

A production deposit system must bind each credited transaction to the active-chain block hash and confirmation depth, not just a height.

Equal work does not trigger a reorg. A strictly heavier valid branch can replace the active branch. Exchange accounting must therefore support credit rollback/reclassification before deposits reach the exchange's finality threshold.

## 9. Address validation

Current address format is network-separated Bech32m.

An integration must reject an address belonging to the wrong network. Mainnet/Testnet/Regtest HRPs are intentionally distinct.

Final exchange validators must be built from the frozen Mainnet specification, not from screenshots or hard-coded assumptions taken during Testnet.

## 10. Amount handling

Amounts are integer atomic units internally.

Candidate conversion:

`1 QUINTUM = 100,000,000 atomic units`

Exchange systems must not use floating-point arithmetic for ledger-critical amount calculations.

## 11. Fees

Transaction fees are the difference between valid input value and output value. A block's coinbase may claim subsidy plus total fees.

Wallet fee policy exists in the current client, but final production exchange fee estimation/relay policy is not yet frozen. The Mainnet integration package will specify minimum relay and recommended fee behavior.

## 12. Required production API

A stable production exchange-facing JSON-RPC/API contract is **not yet frozen or documented in the current repository**. The project will not ask an exchange to integrate against unstable internal C++ interfaces.

Before Mainnet listing outreach, the following minimum operations will be specified, versioned and regression-tested:

- node/network info;
- current height/tip/chain work;
- block lookup by hash and height;
- transaction lookup;
- raw transaction submission;
- mempool query;
- peer/network health;
- fee policy/estimate;
- address validation;
- wallet-disabled full-node mode suitable for custodians/exchanges;
- deterministic error codes;
- reorg-safe transaction/block notifications or polling semantics.

Exact method names are intentionally not invented in this pre-integration document.

## 13. Exchange node deployment requirements

The final integration pack will include:

- supported Linux distribution(s);
- binary/source build instructions;
- service configuration;
- datadir layout;
- firewall ports;
- bootstrap/DNS seed configuration;
- RPC bind/authentication configuration;
- log rotation;
- disk/RAM sizing;
- backup rules;
- upgrade/rollback procedure;
- chain rescan/reindex procedure;
- monitoring checks;
- safe shutdown;
- emergency response procedure.

## 14. Security requirements for exchange deployment

Production guidance will require:

- RPC bound to localhost/private management network only;
- authentication and firewalling;
- no wallet private keys on public seed nodes;
- least-privilege service account;
- isolated hot-wallet key management;
- cold-wallet procedures controlled by the exchange;
- checksum/signature verification for node binaries;
- explicit network selection;
- alerting on stalled sync, peer loss, reorg and unexpected chain work.

## 15. Mainnet listing gate

The exchange integration document becomes production-ready only when all of these are true:

- Mainnet launched;
- final Genesis/network parameters frozen;
- production API version frozen;
- at least a resilient bootstrap/DNS topology exists;
- current source/binary release identified by commit/tag;
- signed release checksums are published;
- block explorer is available;
- transaction/deposit/withdrawal integration tests pass on release candidate;
- reorg handling is tested;
- recommended confirmations are published;
- security/release disclosures are current.

Until then this file is a due-diligence preview, not an instruction to list the asset.
