# QMU protocol DNA and implementation gates

Status: **accepted project architecture — pre-Mainnet**

This document records the features explicitly accepted for QUINTUM/QMU so they
are implemented deliberately rather than by copying whole foreign protocols.

## Non-negotiable base

- independent UTXO Layer-1;
- Nakamoto-style heaviest-valid-chain selection by cumulative PoW;
- RandomX v2 mining PoW;
- per-block ASERT difficulty adjustment;
- 120-second target spacing on the RandomX network;
- deterministic RandomX seed schedule;
- persistent chainstate, mempool, reorg handling and wallet recovery;
- 95% miner / 5% transparent founder primary subsidy;
- 100% transaction fees to miners;
- 1 QMU permanent tail subsidy after the six primary eras;
- no admin mint, master balance key, hidden premine or private-key telemetry.

## Accepted ideas to implement

### Bitcoin family

- stronger addrman/peer diversity and eclipse resistance;
- peer abuse scoring/eviction and bounded resource use;
- compact block relay;
- encrypted authenticated P2P transport;
- Tor/I2P proxy support;
- version-bits consensus deployment states;
- versioned output/script types;
- hashed-key payment outputs;
- multisig and offline/partially signed transaction workflow;
- stable authenticated JSON-RPC including mining methods;
- pruning and independently verifiable fast-sync snapshots;
- compact-filter/light-client support;
- reproducible and signed release pipeline.

### Monero family

- RandomX CPU-accessible PoW — already active on RandomX Testnet;
- permanent tail emission — already implemented;
- Dandelion++-style transaction origin privacy;
- optional full-memory RandomX mining with shared Dataset.

### Bitcoin Cash

- ASERT difficulty adjustment — already active on RandomX Testnet.

### Ravencoin (post-Mainnet extension)

- bounded native UTXO assets without embedding an EVM or arbitrary
  consensus-executed application VM.

### Ergo / Bitcoin light-client direction (post-Mainnet extension)

- independently verifiable light/mobile synchronization. Prefer compact
  filters first; more advanced proofs require a separate security review.

## Explicitly not merged into QMU v1

These designs were reviewed but are intentionally not part of the QMU Mainnet
v1 architecture:

- Ethereum EVM/account model;
- Kaspa blockDAG/GHOSTDAG;
- Decred hybrid PoW/PoS voting;
- Dash masternodes/ChainLocks;
- DigiByte multi-algorithm PoW;
- Dogecoin AuxPoW/merged mining;
- Zcash shielded consensus;
- Litecoin MWEB consensus;
- full Mimblewimble transaction model.

Adding one of these would replace or greatly enlarge QMU's trust/consensus
surface rather than merely improve the current system.

## Safety gate

A feature does not become an active consensus rule merely because its code
exists.

Consensus-sensitive additions must:

1. preserve existing wallet/private-key data;
2. have deterministic vectors and negative tests;
3. pass Windows/Linux/macOS builds;
4. pass multi-node relay/reorg/restart tests;
5. be activated only through an explicitly published network upgrade or a new
   incompatible test network when required;
6. never silently change the existing RandomX Genesis or current chain identity.

Non-consensus networking, packaging and RPC hardening can ship independently
when their regression tests pass.

## Current implementation progress

Already implemented or now in-tree:

- RandomX v2 PoW, ASERT, tail emission and monetary split;
- persistent P2P/addrman, headers-first sync and tx/block relay;
- /16-aware public peer diversity and per-peer message-rate limiting;
- cross-platform Windows/Linux/macOS packaging work;
- multi-thread RandomX mining context with shared light cache and optional
  shared full-memory Dataset;
- deterministic version-bits deployment state machine;
- bounded canonical multisig primitives;
- P2PKH256 hashed-key authorization primitives;
- optional consensus-pinned founder multisig custody representation;
- sanitizer CI mode;
- loopback-only cookie-authenticated production JSON-RPC/mining API;
- negotiated compact block relay with mempool reconstruction and missing-transaction fallback.

Still required before Mainnet freeze:

- encrypted P2P;
- proxy/Tor/I2P and automatic NAT mapping;
- Dandelion++ transaction relay;
- pruning/fast-sync;
- light-client filters;
- offline/partially signed transaction UX;
- explicit activation of the new payment script types;
- fuzzing/adversarial soak tests;
- multiple independent public seed nodes and deployed DNS seeds;
- signed/notarized production releases;
- independent security review.
