# QUINTUM Wallet Core

Status: **DRAFT — pre-mainnet**

Stage 20 introduced the first real QUINTUM wallet core. Stage 21 hardened key storage and deterministic recovery. Stage 22 added persistent transaction history, a restart-safe incremental wallet index, reorg-safe cache rebuilding and a local fee-policy foundation. Stage 23 adds a 24-word human recovery representation, gap-aware restoration and atomic recovery commit semantics.

The wallet does not bypass consensus. A transaction produced by the wallet must still pass the same mempool/UTXO/signature validation as a transaction received from any peer.

## Private-key generation

New private keys are generated only from the operating-system cryptographic random source:

- Windows: `BCryptGenRandom(..., BCRYPT_USE_SYSTEM_PREFERRED_RNG)`;
- Linux: `getrandom()`;
- other POSIX builds: `/dev/urandom` fallback.

Generated 32-byte candidates must additionally pass libsecp256k1 secret-key validation.

Sensitive temporary byte buffers are explicitly overwritten after use where the current design controls their lifetime.

## Address format

The current development address format is Bech32m.

Human-readable prefixes:

- Mainnet candidate: `qtm`;
- Testnet: `tqtm`;
- Regtest: `rqtm`.

The address payload is:

1. one byte address type `0x01`;
2. the 33-byte compressed secp256k1 public key.

The payload is converted from 8-bit bytes to 5-bit Bech32 data and protected by the Bech32m checksum.

Address type `0x01` maps directly to the already-existing QUINTUM P2PK v1 locking script. Stage 20 therefore does **not** add a new spend authorization rule or alter existing UTXO consensus.

Pinned private-key-1 address vectors:

- Mainnet: `qtm1qyp8n0nx0muaewav2ksx99wwsu9swq5mlndjmn3gm9vl9q2mzmup0xqq3mxt2`
- Testnet: `tqtm1qyp8n0nx0muaewav2ksx99wwsu9swq5mlndjmn3gm9vl9q2mzmup0xqfj98rp`
- Regtest: `rqtm1qyp8n0nx0muaewav2ksx99wwsu9swq5mlndjmn3gm9vl9q2mzmup0xqc3rzan`

The address format is implemented and tested but remains a **pre-mainnet candidate** until the public network specification is frozen.

## wallet.dat

Each network directory has its own `wallet.dat`.

Two formats are recognized:

- **v1 (legacy):** Stage 20 checksummed key records, still readable for backward compatibility;
- **v2 (encrypted):** password-derived authenticated encryption for seed, private keys and key metadata.

The v2 open header contains only the data required to select and authenticate the decryption parameters: wallet magic/version, network identity, Argon2 parameters, random salt, random nonce and encrypted-payload length.

Sensitive payload protection:

- password KDF: **Argon2id**, 64 MiB memory, 3 passes, one lane;
- authenticated encryption: **XChaCha20-Poly1305**;
- 16-byte random salt;
- fresh 24-byte nonce on every wallet rewrite;
- the entire open header is authenticated as AEAD associated data;
- recovery seed, private keys and keypool metadata are encrypted;
- wrong passwords and modified ciphertext fail authentication before any key is accepted.

The cryptographic primitives come from pinned Monocypher 4.0.3; transaction signatures remain on the existing pinned libsecp256k1 path.

Wallet replacement remains crash-safe:

1. write a temporary file;
2. flush and force it to durable storage;
3. atomically replace the old wallet.

On POSIX systems the wallet and backup files use mode `0600`.

A v1 wallet is never silently rewritten merely because a password was supplied. Migration is explicit through `Wallet::encrypt_wallet()` or the development CLI `--encrypt-wallet --wallet-passphrase-file PATH`.

### Deterministic recovery

A password-created v2 wallet generates one 256-bit recovery seed from the operating-system CSPRNG.

Private keys use standard **BIP32 CKDpriv mechanics**: HMAC-SHA512 master/child derivation plus libsecp256k1 scalar tweak-add. QUINTUM reserves this path:

`m/5329997'/network'/branch/index`

where `network` is Mainnet/Testnet/Regtest and `branch` is 0 for receive keys or 1 for internal change keys.

The path is versioned by the v2 wallet format and covered by a pinned derivation test vector.

A seed-native v2 wallet can reconstruct its deterministic receive/change keypool from the seed. A migrated v1 wallet, or a v2 wallet containing imported random private keys, must still be backed up as `wallet.dat`; the API deliberately refuses to advertise seed-only recovery as complete in that case.

### 24-word recovery phrase

Stage 23 gives seed-native wallets a **24-word English recovery phrase**.

Compatibility is intentionally strict: QUINTUM's existing 256-bit `RecoverySeed` remains the exact input to the existing BIP32 derivation. The phrase is a reversible human representation of those same 32 bytes using the standard 2048-word BIP39 English list and the BIP39 256-bit entropy checksum rule:

- 256 entropy bits;
- 8 SHA-256 checksum bits;
- 264 bits split into 24 groups of 11 bits;
- one English word per 11-bit index.

QUINTUM does **not** run the phrase through BIP39 PBKDF2 to create a different 512-bit wallet seed. Doing that now would change already-defined QUINTUM BIP32 keys and addresses. A generic wallet that interprets these words through the full BIP39 mnemonic-to-seed PBKDF2 step therefore will not derive QUINTUM's keys unless it explicitly supports the QUINTUM recovery scheme.

The decoder requires exactly 24 known words and verifies the checksum before any wallet file is created. Unknown words, wrong word counts and invalid checksums are rejected.

There is currently no optional BIP39-style mnemonic passphrase/"25th word". The password protecting encrypted `wallet.dat` is separate from the 24-word recovery phrase.

### Gap-aware recovery

`Wallet::recover_from_mnemonic()` restores both QUINTUM deterministic branches:

- branch 0 — receive;
- branch 1 — internal/change.

The default recovery gap limit is **100**, matching the existing keypool policy. Recovery starts with at least 101 receive keys and 100 internal keys. If an active-chain output is found near or beyond the current lookahead, derivation extends until there are 100 unused indices beyond the highest discovered index. Both active-chain outputs and current mempool outputs are considered when marking recovered keys used.

The recovery scan is correctness-first and may rescan the active blockchain multiple times as the lookahead expands. This is acceptable for the current pre-mainnet foundation; a later performance pass can add a dedicated descriptor/filter index without changing recovery semantics.

Recovery is also commit-safe:

1. validate and decode the 24 words;
2. discover deterministic keys from the authoritative active chain;
3. build/synchronize the derivable wallet index;
4. only after all of that succeeds, atomically commit encrypted `wallet.dat`.

An existing `wallet.dat` is never overwritten by mnemonic recovery. If discovery, index persistence or synchronization fails, the in-memory recovery state is wiped and no new `wallet.dat` is committed.

`Wallet::recovery_mnemonic()` and the explicit `NetworkRuntime::wallet_recovery_mnemonic()` bridge return a phrase only when the wallet is fully seed-recoverable. If legacy/random imported private keys are present, seed-only recovery remains disabled and `wallet.dat` backup is required. The phrase is not included in ordinary runtime status, transaction history, P2P messages or wallet-state cache.

## Keypool and backup safety

A new wallet pre-generates a reserve keypool before it can receive funds:

- one active primary receive key;
- 100 additional receive keys;
- 100 internal change keys.

When a new receive address or change address is needed, the wallet first consumes a key that was already persisted in the existing wallet file.

This gives an old backup recovery coverage for the pre-generated future keypool, similar in purpose to the historical Bitcoin keypool model.

If a restored older backup sees one of its reserved keys used by an active-chain or mempool output, the wallet automatically marks that key as used and persists the recovered metadata. This avoids silently reissuing that key as a fresh address.

If a keypool is exhausted, a new batch is generated. The result explicitly marks that a fresh backup is recommended.

Imported private keys also require a fresh backup.

For seed-native v2 wallets, keypool refill uses deterministic BIP32 branch/index derivation, so future deterministic keys remain recoverable from the seed. Legacy/imported keys retain the explicit backup requirement described above.

## Balance model

The wallet owns a persistent derivable index in `wallet_state.dat`. On the first scan it walks the active chain and records wallet-owned UTXOs plus confirmed wallet transaction history. Normal later synchronization processes only blocks after the indexed tip.

The state file is bound to:

- the QUINTUM network and message-start bytes;
- the complete wallet public-key set;
- the indexed active-chain height and tip hash;
- a double-SHA-256 checksum.

It contains **no private keys or recovery seed**. The file is checksummed but intentionally not encrypted because it is a rebuildable index; it does contain privacy-sensitive public-key/transaction metadata, so it should still be treated as private user data. If it is missing, corrupt, belongs to another wallet, or its indexed tip is no longer on the active chain after a reorg, the wallet discards the cache and rebuilds it from the authoritative blockchain. Importing a private key also invalidates the cache so historical funds for that key cannot be missed, including both immediate in-process rescan and the crash-before-rescan case.

It exposes four amounts:

- **confirmed** — mature active-chain wallet outputs;
- **available** — mature confirmed outputs not already spent by a mempool transaction;
- **pending** — wallet outputs created by current unconfirmed mempool transactions;
- **immature** — wallet coinbase outputs that have not reached 100-block maturity.

Mempool spends subtract from available balance immediately. Unconfirmed change appears as pending.

## Transaction history

Stage 22 persists wallet transaction records together with the derivable wallet index.

A history record contains:

- txid;
- status: `confirmed`, `unconfirmed`, or `inactive`;
- wallet value received;
- wallet value spent;
- fee when it can be determined exactly;
- coinbase flag;
- block height/hash for confirmed transactions;
- current confirmation count.

Because the node mempool is intentionally memory-only, a transaction that was unconfirmed before restart is loaded as **inactive** until the transaction is observed again in the current mempool or confirmed in a block. A transaction that was confirmed on a branch later removed by reorg is also retained as **inactive**, with its old block association and confirmation count cleared. This avoids both falsely presenting stale transactions as currently broadcast and silently deleting user-visible history during a reorg.

For wallet-created transactions all inputs belong to the wallet, so the exact fee is retained. For arbitrary transactions involving external inputs, a fee is recorded only when it can be proven from the wallet-visible inputs.

Deleting or rebuilding `wallet_state.dat` cannot lose private keys or confirmed funds. Confirmed history is reconstructed from the blockchain. Inactive history is a wallet convenience record and is not a substitute for backing up `wallet.dat`.

## Fee policy foundation

Stage 22 introduces a **wallet policy**, not a consensus rule:

- default rate: **1,000 atomic units per 1,000 serialized bytes**;
- size fee uses ceiling arithmetic and overflow/range checks;
- when the mempool contains fee-paying transactions, the wallet exposes a recommended rate based on the median observed fee rate, never below the default;
- `NetworkRuntimeStatus` exposes the current recommended fee rate for future GUI/RPC use.

This is intentionally not yet a confirmation-target estimator and does not change mempool consensus validity, block validity, monetary policy, or a network-wide minimum relay fee.

## Transaction creation

The Stage 20 wallet send path:

1. decodes and network-checks the destination address;
2. refreshes wallet ownership against active chain + mempool;
3. rejects zero/out-of-range amounts;
4. adds the explicit requested fee;
5. selects mature available wallet UTXOs deterministically, oldest first;
6. creates the recipient output;
7. sends change to an internal wallet key;
8. signs every selected input through the existing QUINTUM P2PK sighash/signing path;
9. rebuilds a staged UTXO view including the current mempool;
10. applies the completed transaction to that staged view;
11. verifies that the actual fee exactly equals the requested fee.

Only after this does `NetworkRuntime::send_to_address()` submit the transaction to the normal mempool and announce its txid through the existing P2P relay path.

## Continuous-node integration

`NetworkRuntime` owns the wallet together with the node.

At startup it:

- loads/creates the blockchain;
- loads/creates the wallet;
- scans current chain/mempool state;
- then starts P2P.

Wallet state is refreshed after:

- local transaction submission;
- wallet send;
- local mining;
- initial peer blockchain synchronization;
- mempool catch-up;
- accepted network transaction;
- accepted network block/reorg.

The development CLI can now:

- display the current receive address and wallet balances;
- create a new receive address;
- send to a QUINTUM address with an explicit fee;
- back up `wallet.dat`;
- unlock/create encrypted v2 wallets using `--wallet-passphrase-file PATH`;
- explicitly migrate a legacy v1 wallet with `--encrypt-wallet`;
- mine to the wallet automatically when no explicit miner public key is supplied.

The passphrase itself is not accepted as a command-line argument, avoiding normal process-argument exposure.

## Current limitations before production

The current pre-mainnet wallet still does not claim these are finished:

- GUI presentation/confirmation workflow for the implemented 24-word recovery phrase;
- recovery-rescan performance optimization for very large chains;
- confirmation-target fee estimation and automatic fee selection;
- transaction labels/address book metadata;
- hardware-wallet support;
- P2PKH/P2WPKH-style locking;
- GUI/RPC wallet control.

Those should be completed and adversarially tested before Mainnet is frozen.
