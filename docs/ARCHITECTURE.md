# QUINTUM Architecture

## Layers

1. **Primitives** — fixed-width integers, hashes, serialization.
2. **Crypto** — hashing, key handling, secp256k1 signatures.
3. **Transactions** — inputs, outputs, scripts/signature checks, txid.
4. **UTXO set** — authoritative spendable-output state.
5. **Blocks** — headers, Merkle root, transactions.
6. **Consensus** — PoW, difficulty, subsidy, maturity, validation.
7. **Chainstate** — active chain, undo data, reorgs, persistence.
8. **Mempool** — valid unconfirmed transactions.
9. **P2P** — peer discovery, handshake, headers/blocks/tx relay.
10. **Wallet** — keys, addresses, balances, transaction creation.
11. **RPC/GUI** — user-facing control and desktop application.

## Boundary rule

UI, RPC, wallet convenience code and networking policy must never silently redefine consensus. Consensus validation remains deterministic and independently testable.

## Persistence rule

A crash or normal restart must not lose accepted chainstate or wallet keys. Database updates that change chainstate will be designed for atomic recovery.

## Reorg rule

The active chain is selected by cumulative valid proof of work, not merely block count.
