# QUINTUM Roadmap

## M0 — Foundation
C++23 build, repository structure, CI, specification, deterministic test harness.

## M1 — Local blockchain
Serialization, hashes, transactions, UTXO, blocks, PoW validation, chainstate, disk persistence, reorg tests.

## M2 — Development networks
Regtest and testnet parameters, unique genesis blocks, mining and restart/recovery testing.

## M3 — P2P
Handshake, peer database, seeds, address relay, headers-first synchronization, block and transaction propagation.

## M4 — Wallet
**Core implemented in Stage 20:** OS-CSPRNG key generation, network-specific address encoding, receive/send, explicit fees, coin selection, signing, balances and keypool-based backup/recovery. Before public release: password encryption, HD recovery, fee estimation and persistent wallet history/indexing.

## M5 — Desktop
Qt 6 GUI, synchronization state, peers, balances, transaction history and mining status.

## M6 — Windows distribution
Signed/reproducible release pipeline where possible, installer, safe upgrades, preserved wallet/blockchain data.

## M7 — Public testnet
Multiple geographically separate nodes, adversarial tests, reorgs, disconnect/reconnect, long-duration soak tests.

## M8 — Mainnet
Freeze consensus/network specification, generate and independently verify genesis, publish release hashes and documentation, then launch.
