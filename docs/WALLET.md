# QUINTUM Wallet Core

Status: **DRAFT — pre-mainnet**

Stage 20 introduced the first real QUINTUM wallet core. Stage 21 hardens that core with authenticated password encryption and deterministic BIP32 recovery while preserving the Stage 20 v1 wallet reader for migration.

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

The wallet scans the active chain and current mempool for P2PK outputs whose public keys belong to the wallet.

It exposes four amounts:

- **confirmed** — mature active-chain wallet outputs;
- **available** — mature confirmed outputs not already spent by a mempool transaction;
- **pending** — wallet outputs created by unconfirmed mempool transactions;
- **immature** — wallet coinbase outputs that have not reached 100-block maturity.

Mempool spends subtract from available balance immediately. Unconfirmed change appears as pending.

A reorg or mempool reconciliation is handled by rescanning the current authoritative active chain/mempool view rather than trusting cached wallet ownership state.

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

Stage 20 intentionally does not claim these are finished:

- user-facing mnemonic encoding/import and recovery UX;
- deterministic gap-limit/rescan policy for mnemonic restoration;
- dynamic fee estimation;
- persistent transaction history/labels;
- optimized incremental wallet indexing (the current correctness-first scan can rescan the active chain);
- hardware-wallet support;
- P2PKH/P2WPKH-style locking;
- GUI/RPC wallet control.

Those should be completed and adversarially tested before Mainnet is frozen.
