# QUINTUM Blocks

Status: **DRAFT — pre-mainnet**

## Block header

The current draft header contains:

- version — uint32
- previous block hash — 32 bytes
- Merkle root — 32 bytes
- timestamp — uint64
- compact target field (`bits`) — uint32
- nonce — uint64

Serialized header size: **88 bytes**.

The field sizes are still pre-mainnet draft values and may be revised before genesis.

## Block hash

`block_hash = double_sha256(serialized_block_header)`

## Merkle tree

Transaction IDs are used as leaves.

For each tree level:

- adjacent hashes are concatenated;
- the 64-byte pair is double-SHA-256 hashed;
- an odd final hash is duplicated for that level.

The implementation also detects an important ambiguity: if two real sibling hashes are identical before odd-node duplication, the tree is marked `mutated`.

## Structural block rules currently implemented

- block must contain at least one transaction;
- first transaction must be coinbase-shaped;
- no later transaction may be coinbase-shaped;
- computed Merkle root must equal the header Merkle root;
- mutated Merkle trees are rejected.

## Not implemented yet

- PoW target decoding and validation;
- timestamp consensus rules;
- block subsidy rules;
- block size/weight limit;
- transaction signature validation;
- full block connection to chainstate;
- cumulative chain work.
