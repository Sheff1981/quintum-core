# QUINTUM Technical Due Diligence

Status: **PRE-ALPHA / PUBLIC TESTNET — NOT MAINNET FROZEN**  
Technical state covered: `c5e29dcf45375d227872dd1a21c9127b214b1c8d`

## 1. Project identity

QUINTUM is an independent Layer-1 proof-of-work cryptocurrency implemented from the ground up in modern C++23. It is not an ERC-20, BEP-20, wrapped asset or token hosted by another chain.

The public repository was created on 2026-10-02. The first source commit was `fc0ec6001f51fc918904aad32bd080bfe6b9835a` ("foundation: add README.md"). The repository has remained public while the chain has been built.

Current license: **MIT**.

The project explicitly prohibits hidden premine paths, developer mint keys, master keys, consensus bypasses and private-key exfiltration.

## 2. Current lifecycle state

QUINTUM is in public-Testnet pre-alpha.

A Mainnet candidate Genesis has been constructed and pinned for reproducibility, but Mainnet has **not** been launched and the complete Mainnet parameter set is not yet declared frozen.

Development/Testnet balances have no monetary value and must not be treated as circulating Mainnet supply.

## 3. Core architecture

- implementation language: C++23;
- build system: CMake;
- ledger: UTXO;
- consensus: Proof of Work;
- fork choice: greatest cumulative valid proof of work;
- signatures: ECDSA over secp256k1;
- secp256k1 dependency: Bitcoin Core libsecp256k1 v0.8.0, pinned;
- block/transaction digest family: SHA-256;
- transaction ID: double-SHA-256 of deterministic transaction serialization;
- proof-of-work header hash: double-SHA-256;
- durable local blockchain/chainstate storage;
- P2P networking with Bitcoin-like framing, handshake, headers-first synchronization, inventory relay and peer discovery;
- Qt 6 desktop wallet kept above the consensus/core layer.

The GUI does not implement an alternative consensus path. Wallet, mining and network objects are submitted through the same core validation used by the node.

## 4. Network profiles

| Parameter | Mainnet candidate | Public Testnet | Regtest |
|---|---:|---:|---:|
| Network name | mainnet | testnet | regtest |
| Message magic | `51 b7 4c a3` | `b7 d7 16 5a` | `33 20 e2 ee` |
| P2P port | 28444 | 38444 | 48444 |
| RPC port reserved/candidate | 28445 | 38445 | 48445 |
| Target spacing | 600 s | 600 s | 1 s |
| Retarget interval | 2016 | 2016 | nominal 144 |
| PoW limit bits | `0x1e0ffff0` | `0x1e0ffff0` | `0x2100ffff` |
| Min-difficulty exception | no | yes | fixed easy target |
| Retargeting | yes | yes | no |

Mainnet/Testnet target timespan is 1,209,600 seconds (14 days).

Testnet permits a minimum-difficulty block when its timestamp is more than two target spacings after its parent. The following normally timed block restores the most recent non-minimum difficulty except at a normal retarget boundary.

## 5. Genesis provenance

All built-in network Genesis blocks were constructed on 2026-10-02 and are reconstructed by deterministic consensus code and checked against pinned constants.

The public development anchor embedded in the Genesis coinbase message is the pre-Genesis repository milestone prefix `1bac54a3f46c`.

Common message prefix:

`QUINTUM 02/Oct/2026 1bac54a3f46c Independent PoW digital cash`

Timestamp for all three built-in Genesis headers:

- Unix: `1790960400`
- UTC: **2026-10-02 17:00:00 UTC**

### Mainnet candidate Genesis

- version: 1
- bits: `0x1e0ffff0`
- nonce: `591080`
- Merkle root: `b1d9e1aedbe90d5b88148d4af6b0a64d6fb20c286d72192713147869e69580c5`
- Genesis hash: `0000008b82073109dc079e6c5b7eac0c2fba8a5633822f87fc33719633ebab8c`

### Public Testnet Genesis

- version: 1
- bits: `0x1e0ffff0`
- nonce: `969294`
- Merkle root: `d5be95629c8ce60e22e817bc54accc3ed75983d5267b89724cd808f422a8c179`
- Genesis hash: `0000039bf09b49dfa4c9bcb924c0c38257fdeef9952021baabceb86fa099e872`

### Regtest Genesis

- version: 1
- bits: `0x2100ffff`
- nonce: 0
- Merkle root: `01b9f141fff566d6d50e700ff0c59f07ff0a89123921340bd946f30386c09d89`
- Genesis hash: `211c0cdb97dfb8bc2f0190e40132d1eb3be5ab9a03c9717aa3b2e90a98b3fcd6`

The height-0 subsidy is 50 QUINTUM but is paid to permanently reserved unspendable locking-script version `0x00`. It has no private key and cannot be recovered without creating an incompatible consensus network.

## 6. Monetary policy candidate

- precision: **8 decimal places**;
- 1 QUINTUM = **100,000,000 atomic units**;
- money-range ceiling: **21,000,000 QUINTUM**;
- initial subsidy: **50 QUINTUM**;
- halving interval: **210,000 blocks**;
- non-zero subsidy eras: **33**;
- exact scheduled subsidy total: **20,999,999.9769 QUINTUM**;
- Genesis subsidy: **50 QUINTUM, permanently unspendable**;
- maximum theoretically spendable scheduled subsidy: **20,999,949.9769 QUINTUM**, before later burns or under-claimed rewards;
- coinbase maturity: **100 blocks**;
- maximum valid coinbase claim: **subsidy + transaction fees**.

There is no developer mint, administrator balance override or privileged coin-generation path.

The working GUI abbreviation is **QTM**. The final Mainnet exchange ticker remains a pre-launch decision and must not be treated as formally frozen yet.

## 7. Proof of Work and difficulty

The block header carries a compact `bits` target. Invalid zero, negative, overflowing or non-canonical targets are rejected.

PoW validity requires:

`block_hash <= target`

and:

`target <= network_pow_limit`.

Per-block work is:

`floor(2^256 / (target + 1))`.

Cumulative chain work is the sum of valid per-block work. Height alone does not select the active chain.

Mainnet candidate retarget:

1. determine the completed 2016-block period on the candidate branch;
2. measure actual timespan;
3. clamp to target-timespan/4 through target-timespan*4;
4. scale the old target using exact integer arithmetic;
5. cap at the network PoW limit;
6. require canonical compact encoding.

A miner cannot choose an easier arbitrary target. Contextual `bits` are calculated from the candidate's branch and checked before PoW acceptance.

Mining performs real nonce search. The desktop miner is currently a correctness/reference miner rather than an optimized production miner.

## 8. Timestamp and block resource rules

Current consensus candidate:

- Median Time Past window: 11 blocks;
- candidate timestamp must be strictly greater than branch MTP;
- maximum future time: adjusted time + 7,200 seconds;
- maximum serialized block size: 1,000,000 bytes;
- maximum transactions per block: 10,000;
- maximum script size: 10,000 bytes;
- maximum coinbase unlocking script: 100 bytes.

## 9. Transactions and authorization

Base transaction fields:

- version;
- inputs;
- outputs;
- lock_time.

Inputs identify a previous transaction hash/output index and carry unlocking data plus sequence. Outputs carry an unsigned 64-bit atomic amount and locking data.

Initial spendable locking model: versioned P2PK v1.

P2PK v1 lock:

- `0x01`;
- 33-byte compressed secp256k1 public key.

Unlock:

- `0x01`;
- 64-byte compact ECDSA signature.

Reserved lock version `0x00` is permanently unspendable and is used by Genesis.

Current signature mode is domain-separated SIGHASH_ALL. The signed preimage commits to all transaction inputs/outpoints and sequences, all outputs, lock_time, current input index, exact previous-output amount and locking script, and sighash type.

High-S signatures are rejected.

UTXO validation covers ownership authorization, missing inputs, monetary bounds, coinbase maturity and double-spend prevention.

## 10. Address format

Stage 20 implements network-separated Bech32m wallet addresses carrying address type `0x01` plus the 33-byte compressed secp256k1 public key.

Current HRPs:

- Mainnet candidate: `qtm`
- Testnet: `tqtm`
- Regtest: `rqtm`

The format is implemented and regression-tested but remains a pre-Mainnet candidate until final freeze.

## 11. Chainstate, forks and reorganizations

The block index can retain the active branch and side branches.

A side branch activates only when:

`candidate_chain_work > active_chain_work`.

Equal cumulative work does not force a reorg.

Reorgs are staged:

1. locate common ancestor;
2. disconnect old branch using undo state;
3. connect candidate branch forward;
4. validate every contextual/transaction/UTXO rule;
5. commit only if the complete transition is valid.

An invalid heavier branch does not partially mutate live state. Failed branch state is marked so descendants can be rejected.

## 12. Persistent blockchain storage

The current persistent storage uses:

- `blocks.dat` — append-only framed block records;
- `chainstate.dat` — checksummed chainstate snapshot;
- `chainstate.dat.tmp` — non-authoritative temporary commit file.

State transitions are staged in memory and published only after durable commit succeeds.

The commit process validates the block, verifies the committed block-log prefix, trims uncommitted crash tails, appends/flushed records, writes chain metadata/UTXO/undo state, checksums the snapshot and atomically replaces the authoritative chainstate.

Windows uses write-through replacement semantics; POSIX uses rename plus directory fsync where available.

Wrong-network or corrupt persistent state is not silently replaced with a new chain.

## 13. P2P protocol

QUINTUM uses a fixed 24-byte P2P header:

- 4-byte network magic;
- 12-byte zero-padded ASCII command;
- 4-byte little-endian payload size;
- 4-byte checksum: first four bytes of double-SHA-256(payload).

Current P2P envelope ceiling: 2,000,000 bytes.

Handshake is Bitcoin-like `version/verack` and includes protocol version, service bits, timestamp, random connection nonce and reported blockchain height. Self-connections are rejected by nonce.

Implemented network functions include:

- inbound/outbound TCP peers;
- ping/pong;
- reconnect;
- `getaddr/addr`;
- persistent `peers.dat`;
- retry/backoff;
- hardcoded numeric seeds;
- headers-first synchronization;
- Bitcoin-style block locator;
- batches up to 2,000 headers;
- bounded `getdata` block transfer;
- transaction mempool;
- `inv/getdata/tx` transaction relay;
- `inv/getdata/block` block relay;
- mempool catch-up;
- continuous network runtime;
- heavier-chain activation through normal chainstate validation.

The public Testnet currently has one pinned bootstrap endpoint:

`212.193.15.139:38444`

Mainnet/Regtest seed lists remain empty.

Still pending before production Mainnet maturity:

- additional independent seed nodes;
- DNS seeds;
- UPnP/NAT-PMP automatic inbound mapping;
- production-grade peer reputation/eviction hardening;
- longer multi-node soak/adversarial testing.

## 14. Headers-first synchronization

A syncing node builds a locator from recent active-chain hashes and exponentially older hashes back to Genesis. The remote side returns up to 2,000 headers after the first common locator point.

Before requesting bodies, header linkage and PoW are checked. Full blocks are then submitted through the normal persistent node path, where contextual difficulty, timestamp, Merkle root, transaction, UTXO, coinbase and chain-work rules are enforced again.

The current head includes two important live-network-derived hardenings:

- initial synchronization is bidirectional rather than dependent on which side initiated the TCP connection;
- equal-height competing tips remain discoverable so a same-height fork can later resolve by greater cumulative work;
- full 2,000-header continuation batches continue correctly even when an intermediate batch is already known on a side branch, and returned header anchors are checked against the requested locator/continuation.

## 15. Wallet

Current wallet architecture includes:

- OS cryptographic randomness for private-key creation;
- secp256k1 keys;
- network-separated Bech32m addresses;
- encrypted `wallet.dat`;
- Argon2id password derivation;
- XChaCha20-Poly1305 authenticated encryption;
- deterministic BIP32 recovery foundation;
- 24-word English recovery phrases;
- gap-aware restoration;
- persistent wallet transaction history/index;
- coinbase maturity accounting;
- available/confirmed/pending/immature balance separation;
- persistent labels/address-book metadata;
- complete network-bound `.qtmbackup` bundle.

Private keys are not sent over P2P.

The desktop GUI never implements signing rules independently; it calls wallet/runtime core APIs.

## 16. Desktop and Windows distribution

The desktop client is Qt 6 Widgets, with CI pinned to Qt 6.8.0.

Normal pre-alpha desktop startup defaults to **Testnet**, preventing ordinary users from accidentally treating the Mainnet candidate as a launched network.

Current GUI functions include:

- Overview;
- Send;
- Receive;
- Transactions;
- Address Book;
- Mining;
- Settings;
- peer count/best height/sync state;
- real measured mining hash rate;
- recovery and backup controls.

The Windows installer is per-user, uses Inno Setup, places program files under LocalAppData Programs and keeps wallet/blockchain data outside the installation directory.

Update/uninstall is tested not to delete the user's AppData wallet/blockchain data.

Current installer is **unsigned pre-alpha**. Production signing requires a real code-signing certificate.

## 17. Security design statements

The current codebase is designed so that:

- remote peers cannot directly assign balances, heights, UTXO or active tips;
- incoming blocks and transactions pass normal consensus validation;
- block reward is bounded by consensus;
- Genesis does not allocate spendable creator funds;
- wallet private keys remain local;
- wallet storage uses authenticated encryption;
- Windows sensitive temporary wallet/metadata files receive current-user-only ACL protection;
- wrong-network storage/peer databases are rejected;
- network magic separates Mainnet/Testnet/Regtest traffic;
- corrupted persistent state is surfaced rather than silently reset.

No claim of a completed independent third-party security audit is made at this stage.

## 18. Current public-Testnet evidence

On 2026-10-03 a fresh Windows install connected automatically to the public Testnet seed without manual peer/IP configuration.

Observed end-to-end path:

Windows installer -> encrypted wallet -> automatic Testnet bootstrap -> version/verack -> real PoW -> valid 50-QTM coinbase -> block inventory relay -> VPS validation -> durable VPS blockchain storage -> restart recovery.

Two initial blocks were independently confirmed on the remote VPS. Later live runs confirmed relay through height 7, automatic reconnect, and an overnight synchronized run through height 139 with coinbase maturity accounting.

At height 139, the observed wallet accounting was:

- Available/Confirmed: 2,000 QTM;
- Immature: 4,950 QTM;
- 40 matured 50-QTM rewards + 99 immature 50-QTM rewards = 6,950 QTM total.

This is Testnet evidence only.

## 19. Build and verification state

At technical state `c5e29dcf45375d227872dd1a21c9127b214b1c8d`, both repository workflows completed successfully:

- `gui`: GitHub Actions run 37181984433 — success;
- `build`: GitHub Actions run 37181984429 — success.

The current source head is a verified GitHub commit.

## 20. Known pre-Mainnet gaps

The following must not be concealed from an exchange:

- Mainnet has not launched;
- final ticker is not frozen;
- final Mainnet parameter freeze has not occurred;
- only one public hardcoded Testnet seed is currently documented;
- DNS seeds are not yet deployed;
- UPnP/NAT-PMP is not yet implemented;
- production peer abuse/reputation hardening is incomplete;
- Windows release is unsigned;
- no independent third-party security audit is claimed;
- a production exchange-facing JSON-RPC/API contract is not yet frozen/documented;
- public Mainnet explorer/release-signing/update policy remains a launch requirement;
- long-duration adversarial multi-node testing remains in progress.

## 21. Mainnet freeze requirements

Before an exchange is asked to list QUINTUM Mainnet, the project must publish an immutable release package containing at minimum:

- final ticker;
- final Mainnet address format;
- final network magic and ports;
- final Mainnet Genesis hash/parameters;
- final monetary policy;
- final difficulty rules;
- final node version and source commit;
- production bootstrap/DNS infrastructure;
- production exchange integration API;
- explorer endpoints;
- signed checksums/release manifests;
- Windows/Linux node installation instructions;
- recommended confirmation policy;
- reorg/deposit handling guidance;
- security review status;
- disclosure of any known consensus/network risks.

No pre-alpha/Testnet artifact should be substituted for those Mainnet freeze materials.
