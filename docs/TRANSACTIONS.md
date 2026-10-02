# QUINTUM Transactions

Status: **DRAFT — pre-mainnet**

## Transaction structure

A transaction currently contains:

- `version`
- zero or more inputs during object construction; validation requires at least one
- zero or more outputs during object construction; validation requires at least one
- `lock_time`

Each input contains:

- previous transaction hash
- previous output index
- unlocking script bytes
- sequence

Each output contains:

- unsigned 64-bit amount in atomic units
- locking script bytes

The number of decimal places and the public name of the atomic unit are not frozen yet.

## Serialization order

1. version — uint32 little-endian
2. input count — CompactSize
3. each input:
   - previous tx hash — 32 raw bytes
   - output index — uint32 little-endian
   - unlocking script length — CompactSize
   - unlocking script bytes
   - sequence — uint32 little-endian
4. output count — CompactSize
5. each output:
   - value — uint64 little-endian
   - locking script length — CompactSize
   - locking script bytes
6. lock_time — uint32 little-endian

## Transaction ID

For the current base format:

`txid = double_sha256(serialized_transaction)`

Human-facing hash display byte order is not yet frozen.

## Coinbase shape

A transaction is recognized as coinbase-shaped when it has exactly one input whose previous outpoint is null:

- previous tx hash = 32 zero bytes
- previous output index = `0xffffffff`

Full coinbase consensus rules, subsidy and maturity are not yet implemented.

## Structural validation currently implemented

- at least one input
- at least one output
- no duplicate previous outpoint inside one transaction
- output total must not overflow the 64-bit amount type

Signature validation, UTXO existence, ownership, fee rules, monetary range and script semantics belong to later consensus stages.
