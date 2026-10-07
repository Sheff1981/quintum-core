# QUINTUM — Start Here

This document is intended for readers who are not required to be programmers.

## What we are building

QUINTUM is an independent decentralized network. It is not an Ethereum token and not an entry inside another blockchain.

In the future, an ordinary user should be able to:

1. download the application;
2. install it;
3. launch the wallet;
4. automatically discover other nodes;
5. synchronize the blockchain;
6. obtain an address;
7. send and receive QUINTUM;
8. optionally participate in mining.

## What the coin consists of

**Consensus** — the rules by which all computers decide whether a block or transaction is valid.

**Blockchain** — the sequence of confirmed blocks.

**UTXO** — accounting for coins that have not yet been spent. This is the model used by Bitcoin.

**PoW** — miners perform real computational work to find a valid block.

**P2P** — computers discover each other and exchange blocks and transactions without a central server.

**Wallet** — stores the user's keys and creates signed transactions.

## What is important to understand

Before Mainnet is launched, protocol parameters can still be changed deliberately and safely. After launch, changing rewards, the mining algorithm, Genesis, or other consensus-critical rules without an explicit network upgrade can split old and new nodes into incompatible networks.

The development order is therefore:

**rules → tests → blockchain → P2P → mining → wallet → GUI → installer → Testnet → Mainnet.**

We do not prioritize cosmetic UI work until the network foundation has demonstrated that it works.
