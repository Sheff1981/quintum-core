# QUINTUM Wallet Core

Status: **DRAFT — pre-mainnet**

Stage 20 introduces the first real QUINTUM wallet core. It owns private keys, derives network-specific receive addresses, discovers wallet outputs on the active chain and mempool, constructs and signs spends, and persists the key material in `wallet.dat`.

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

The file contains:

- wallet magic/version;
- network identity;
- network message-start bytes;
- key records;
- receive/change usage flags;
- double-SHA-256 checksum.

A wallet created for one network is rejected by another network.

Wallet replacement is crash-safe:

1. write a temporary file;
2. flush it;
3. force it to durable storage;
4. atomically replace the old wallet.

On POSIX systems the wallet and backup files are created with mode `0600`.

### Important encryption limitation

Stage 20 does **not** pretend that checksum protection is encryption.

Private keys in the current `wallet.dat` are **not password-encrypted at rest**. The checksum detects corruption; it does not protect a stolen wallet file.

For that reason the project remains pre-mainnet. Password/KDF-based wallet encryption and production Windows data-directory hardening must be completed before a public-money release.

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

This is not an HD deterministic seed wallet. A later wallet-hardening stage should replace finite keypool backup coverage with a deliberate HD/encrypted design before mainnet.

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
- mine to the wallet automatically when no explicit miner public key is supplied.

## Current limitations before production

Stage 20 intentionally does not claim these are finished:

- encrypted/password-protected wallet storage;
- HD deterministic seed/mnemonic recovery;
- dynamic fee estimation;
- persistent transaction history/labels;
- optimized incremental wallet indexing (the current correctness-first scan can rescan the active chain);
- hardware-wallet support;
- P2PKH/P2WPKH-style locking;
- GUI/RPC wallet control.

Those should be completed and adversarially tested before Mainnet is frozen.
