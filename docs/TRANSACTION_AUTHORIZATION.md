# QUINTUM Transaction Authorization

Status: **DRAFT — pre-mainnet**

## Cryptographic library

QUINTUM uses the official Bitcoin Core `libsecp256k1` library for ECDSA operations.

Pinned dependency:

- release: `v0.8.0`
- release date: 2026-08-03
- source archive SHA-256:
  `dd685546f9e717b9adde329acd5a4cd8083d40f710fdb0a603ee5a83f908132b`

The build does not follow a moving branch or an unpinned "latest" reference.

## Key format

Current consensus helper types:

- private key: 32 bytes
- compressed public key: 33 bytes
- compact ECDSA signature: 64 bytes

A private key must pass secp256k1 secret-key validation before use.

The known private-key-1 vector is tested against the compressed generator public key:

`0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798`

## Reserved provably-unspendable lock

Locking-script version `0x00` is permanently reserved as **provably unspendable**.

Any UTXO whose locking script begins with this version is rejected by authorization before public-key parsing or signature checking. This version is used by the Genesis coinbase so the creator cannot own or later recover the Genesis subsidy.

Future script extensions must never reinterpret `0x00` as spendable without deliberately creating an incompatible consensus network.

## Initial locking model: P2PK v1

QUINTUM begins with a deliberately small, auditable locking model.

A P2PK v1 locking script is exactly:

- byte `0x01` — script/version marker
- 33-byte compressed secp256k1 public key

Total: 34 bytes.

An unlocking script is exactly:

- byte `0x01` — unlock/version marker
- 64-byte compact ECDSA signature

Total: 65 bytes.

Unknown or malformed script formats are rejected.

This is intentionally simpler than a general scripting virtual machine. More script types must not be added until the base consensus is stable.

## Signature hash

Current sighash mode is fixed to:

`SIGHASH_ALL = 1`

The signed digest is double-SHA-256 of a deterministic preimage containing:

1. domain tag: `QUINTUM-SIGHASH-V1`
2. transaction version
3. every input outpoint and sequence
4. every transaction output, including value and locking script
5. lock_time
6. the input index being authorized
7. the exact previous-output value
8. the exact previous-output locking script
9. sighash type

Unlocking scripts are deliberately excluded from the preimage.

Including the spent output value and locking script binds the signature to the exact UTXO being spent.

The QUINTUM-specific domain tag prevents accidental cross-protocol use of the same serialization as an unsigned signing message.

## Signature rules

- signatures are produced through libsecp256k1 ECDSA
- verification uses the public key embedded in the previous UTXO
- high-S signatures are rejected rather than normalized during validation
- changing transaction outputs after signing invalidates the signature
- a private key whose public key does not match the UTXO cannot create a valid unlock

## UTXO integration

For every non-coinbase input, validation now performs:

1. referenced UTXO existence check
2. money-range check
3. coinbase maturity check when applicable
4. locking-script parsing
5. unlocking-signature parsing
6. deterministic sighash calculation
7. ECDSA verification
8. amount accounting

No UTXO mutation begins until authorization succeeds.

## Key generation

Consensus supports secret-key validation, public-key derivation, signing and verification.

Secure random private-key generation is intentionally **not** implemented with `std::random_device` or another weak convenience RNG.

Wallet key creation will be added with an explicit operating-system CSPRNG and wallet backup/recovery design. Until then, no production wallet should generate user funds.

## Current limitations

Not yet implemented:

- address encoding
- P2PKH/P2WPKH-style address locking
- HD wallet derivation
- secure OS-backed private-key generation
- encrypted wallet storage
- key backup/recovery
- hardware-wallet support
- general script engine
