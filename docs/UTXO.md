# QUINTUM UTXO State

Status: **DRAFT — pre-mainnet**

## Purpose

The UTXO set is the authoritative in-memory model of outputs that exist and have not yet been spent.

A normal transaction is valid at the UTXO layer only if every referenced previous output currently exists and the total input value is at least the total output value.

## Coin record

Each unspent output stores:

- the original `TxOutput`
- the block height where it was created
- whether it was created by a coinbase transaction

Coinbase maturity is not enforced yet; the metadata is already stored so that rule can be added later without redesigning the record.

## Atomic transaction application

`UtxoSet::apply_transaction()` performs all checks before mutating the set.

Current checks:

- transaction structure is valid
- every non-coinbase input exists
- total input amount does not overflow
- input value covers output value
- new output keys do not collide with existing UTXOs

Only after all checks pass are spent outputs removed and new outputs inserted.

## Double-spend protection

After an output is spent it is removed from the set. A second transaction trying to spend the same outpoint fails with `missing_input`.

This is the first concrete double-spend protection layer in QUINTUM.

## Fees

For a non-coinbase transaction:

`fee = sum(inputs) - sum(outputs)`

Fee policy and minimum relay fee are not consensus-frozen yet.

## Undo data

Every successful application returns `UtxoUndo` containing:

- coins that were spent
- outpoints that were created

`undo_transaction()` removes created outputs and restores spent outputs.

This is the foundation for block disconnect and chain reorganization handling.

## Not implemented yet

- signature/script verification
- coinbase subsidy limits
- coinbase maturity
- block-level atomic connect/disconnect
- persistent chainstate database
- reorg selection
