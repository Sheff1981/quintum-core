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

Coinbase maturity is enforced. A coinbase output created at height H cannot be spent before height H + 100.

## Atomic transaction application

`UtxoSet::apply_transaction()` performs all checks before mutating the set.

Current checks:

- transaction structure is valid
- every non-coinbase input exists
- input and output values remain inside the monetary range
- total input amount does not exceed the monetary range
- coinbase inputs have at least 100 blocks of maturity
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
- persistent chainstate database
- fee relay/minimum policy

Coinbase subsidy limits, maturity, block-level atomic connect/disconnect and reorg selection are implemented in the consensus/chainstate layers.
