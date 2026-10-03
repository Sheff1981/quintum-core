# QUINTUM Wallet Core

Status: **DRAFT — pre-mainnet**

Stage 20 introduced the first real QUINTUM wallet core. Stage 21 hardened key storage and deterministic recovery. Stage 22 added persistent transaction history, a restart-safe incremental wallet index, reorg-safe cache rebuilding and a local fee-policy foundation. Stage 23 added a 24-word human recovery representation, gap-aware restoration and atomic recovery commit semantics. Stage 24 added shared relay-fee policy and automatic wallet fee selection. Stage 25 added durable user metadata plus guarded preview/confirm. Stage 26 introduced the Qt desktop shell. Stage 27 added operational recovery, password-gated seed reveal, address-book/mining/settings integration and recovery-metadata hardening. Stage 28 encrypts wallet metadata, adds complete backup/restore bundles and enforces Windows private-file ACLs. Stage 31 moves the default wallet into a structured per-network `wallets/default/` directory through a non-overwriting legacy migration; the wallet formats and key derivation are unchanged.

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

Each network directory has its own default wallet at `wallets/default/wallet.dat`. Legacy flat `wallet.dat`, `wallet_state.dat` and `wallet_meta.dat` files are moved into that directory by Stage 31 without rewriting their bytes; any source/destination conflict aborts startup rather than overwriting a wallet.

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

On POSIX systems wallet/metadata/backup temporary files use mode `0600`. On Windows, Stage 28 applies a protected DACL that grants file access only to the current user's SID before sensitive bytes are written, then atomically replaces the destination.

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

After the recovered keys are safely committed, Stage 27 writes an empty wallet-bound `wallet_meta.dat`. This intentionally replaces orphaned metadata that cannot be proven to belong to the recovered seed and prevents a stale metadata file from producing a `wrong_wallet` failure on the next restart. If that metadata rebind cannot be persisted, recovery reports a store failure and the newly created wallet file is removed best-effort.

`Wallet::recovery_mnemonic()` and the explicit `NetworkRuntime::wallet_recovery_mnemonic()` bridge return a phrase only when the wallet is fully seed-recoverable. If legacy/random imported private keys are present, seed-only recovery remains disabled and `wallet.dat` backup is required. The phrase is not included in ordinary runtime status, transaction history, P2P messages or wallet-state cache.

Stage 27 desktop seed display is password-gated. `Wallet::verify_passphrase()` derives a candidate key with the wallet's stored Argon2id parameters and compares all 32 bytes against the active encryption key without early exit. The GUI asks for the password first and requests the mnemonic only after verification succeeds. Password and phrase buffers controlled by the desktop/runtime are best-effort overwritten after use.

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

## Fee policy and automatic selection

Stage 24 promotes the Stage 22 fee foundation into a shared **node/wallet policy**. It is still not a consensus rule.

QUINTUM currently defines:

- one coin = **100,000,000 atomic units**;
- default wallet fee rate = **1,000 atomic units per 1,000 serialized bytes**;
- default local minimum relay rate = **1,000 atomic units per 1,000 serialized bytes**;
- all byte-based fee calculations use ceiling arithmetic plus overflow/money-range checks.

The node mempool calculates the minimum acceptable fee from the transaction's actual serialized size. Transactions below the local relay floor are rejected with a dedicated policy error and the required fee is returned to the caller.

This minimum is deliberately **mempool/relay policy only**. Block/transaction consensus validation does not contain a minimum-fee rule. An otherwise-valid transaction that pays less than the local relay minimum can still be valid if it is included in a valid block.

### Auto fee

The wallet's Auto rate is:

`max(wallet default rate, node minimum relay rate, median current-mempool fee rate)`.

If the mempool is empty, Auto uses the policy floor. If observed fee-paying mempool transactions are more expensive, the current median raises the recommendation.

For a candidate payment the wallet:

1. synchronizes wallet state;
2. validates the destination and amount;
3. orders mature spendable UTXOs deterministically;
4. adds inputs until the payment plus the required size fee can be funded;
5. estimates the exact signed P2PK size using the fixed 65-byte unlock script and 34-byte lock script;
6. evaluates both one-output (no change) and two-output (with change) layouts;
7. returns a `WalletFeeQuote` containing rate, byte size, fee, selected value, input/output counts and change;
8. creates/signs the real transaction using that quote;
9. verifies the real serialized size and required fee before returning it.

For the common current P2PK shape with one input and two outputs, the signed serialized size is 202 bytes. At 1,000 atomic/1,000 bytes, the exact minimum fee is therefore 202 atomic units, or 0.00000202 coin.

A no-change transaction may intentionally pay slightly more than the pure size minimum when the remainder is too small to fund an additional change output at the selected rate. This avoids creating an output whose extra serialized cost cannot be funded by the remainder.

`NetworkRuntimeStatus` exposes both `min_relay_fee_rate_per_kb` and `recommended_fee_rate_per_kb`. `NetworkRuntime::quote_send_fee()` lets a desktop UI show the fee before confirmation, while `send_to_address_auto_fee()` creates, submits, syncs and relays the Auto transaction.

This is not yet a historical confirmation-target estimator. The current mempool median is a deterministic local load signal, not a prediction that a given fee will confirm within N blocks.
## Transaction creation

Two send modes now coexist:

- **Auto (default user path):** quote and calculate fee from transaction size plus current policy/load;
- **Manual:** preserve the existing explicit atomic fee API for testing and expert override.

Both paths still select mature UTXOs deterministically, use an internal change key, sign every input through the existing QUINTUM P2PK authorization path, validate against a staged chain+mempool UTXO view and finally submit through the same node mempool/P2P relay path. Auto does not bypass any transaction or relay validation.

## User metadata and desktop send confirmation

Stage 25 adds a separate `wallet_meta.dat` for user-created metadata that is not derivable from the blockchain:

- address-book entries: canonical network address + label;
- transaction labels keyed by txid;
- maximum 10,000 address labels and 10,000 transaction labels;
- maximum label length: 128 bytes;
- empty label removes the stored label.

The metadata file contains **no private keys, recovery seed or wallet encryption password**, but labels are privacy-sensitive. Stage 28 introduces metadata **v2**, authenticated-encrypted with XChaCha20-Poly1305 using the active encrypted-wallet key and a fresh 24-byte nonce. Network/message-start and wallet identity are authenticated as associated data. Legacy checksummed plaintext metadata v1 remains readable; encrypted wallets migrate it to v2. Explicit legacy wallet encryption migrates metadata in the same operation and rolls the wallet/metadata files back if metadata migration cannot be committed.

Persistence uses the same safety pattern as the other wallet stores: temporary file, flush/fsync, atomic replacement and checksum verification. The file is bound to the QUINTUM network and to a stable hash of a public key actually owned by the wallet. Key-record reordering, new receive addresses, change-key use, keypool refill and imported additional keys therefore do not change the binding. Copying metadata from another wallet is rejected as `wrong_wallet`.

Unlike `wallet_state.dat`, labels are not rebuildable from the blockchain. Corrupt, wrong-network or wrong-wallet metadata is therefore **not silently ignored**: wallet startup returns a metadata failure so the user has a chance to restore or repair the file rather than unknowingly losing labels.

The legacy single-file `wallet.dat` backup API remains for compatibility. Stage 28 adds a complete `.qtmbackup`: magic/version, network identity, wallet.dat bytes, wallet metadata bytes and a double-SHA-256 checksum. Restore refuses to overwrite an existing wallet, rejects corrupt/wrong-network bundles and discards rebuildable `wallet_state.dat` so state is reconstructed from the authoritative chain. Losing metadata still cannot lose coins/private keys, but the complete bundle preserves labels as well.

### Guarded preview / confirm

The desktop send contract is now explicitly two-step.

`NetworkRuntime::preview_send()` returns a `NetworkWalletSendPreview` containing:

- destination and amount;
- current automatic `WalletFeeQuote`;
- recipient label when present in the address book;
- a `state_hash` covering current active-chain height/tip plus current mempool txids/fees/sizes;
- a `preview_id` binding the request, fee quote and state hash.

`NetworkRuntime::confirm_send()` runs under the same runtime state lock used by node/wallet mutation. Before creating a transaction it:

1. verifies the preview id, rejecting modified preview contents as `invalid_preview`;
2. recomputes the node state hash, rejecting changed chain/mempool state as `stale_preview`;
3. recomputes the automatic fee quote and requires it to match the preview;
4. only then creates/signs the transaction, submits it to the normal mempool, refreshes wallet state and announces it through normal P2P relay.

This prevents a desktop confirmation screen from silently authorizing a different fee after a new block or mempool change. Metadata changes such as editing a recipient label do not alter monetary transaction state and therefore do not invalidate the preview.

### Desktop snapshot

`NetworkRuntime::desktop_snapshot()` provides one GUI-facing read model containing:

- runtime/network status and peer counts;
- active height/tip;
- mempool transaction count;
- wallet balances;
- minimum relay and recommended fee rates;
- current receive address;
- wallet transaction history decorated with optional user labels;
- address-book entries.

The actual Qt GUI is intentionally kept outside consensus/wallet internals and will consume this API in the next stage.

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
- send to a QUINTUM address with automatic fee by default, or an explicit `--fee` override;
- back up `wallet.dat`;
- unlock/create encrypted v2 wallets using `--wallet-passphrase-file PATH`;
- explicitly migrate a legacy v1 wallet with `--encrypt-wallet`;
- mine to the wallet automatically when no explicit miner public key is supplied.

The passphrase itself is not accepted as a command-line argument, avoiding normal process-argument exposure.

## Current limitations before production

The current pre-mainnet wallet still does not claim these are finished:

- recovery-rescan performance optimization for very large chains;
- historical/confirmation-target fee estimation beyond the current mempool-median policy;
- hardware-wallet support;
- P2PKH/P2WPKH-style locking;
- long-duration/adversarial desktop and recovery soak testing;
- hardware-wallet support;
- production release signing and external installer testing.

Those should be completed and adversarially tested before Mainnet is frozen.
