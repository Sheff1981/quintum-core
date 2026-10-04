# QUINTUM Public Testnet Testing

QUINTUM is currently in **pre-alpha public testnet development**. There is no mainnet yet, and testnet QTM has **no monetary value**.

We welcome external testers who want to run the Windows client, test networking and wallet behavior, mine testnet blocks, send transactions, and report reproducible bugs.

## Download the Windows test build

The official public Windows pre-alpha build is available from GitHub Releases:

https://github.com/Sheff1981/quintum-core/releases/tag/v0.0.1-prealpha

1. Open the latest successful **gui** workflow run on `main`.
2. In **Artifacts**, download `quintum-windows-x64`.
3. Extract the ZIP archive.
4. Run:
   `installer/QUINTUM-Core-Setup-0.0.1-prealpha-x64.exe`
5. The archive also contains `SHA256SUMS.txt` so you can verify the downloaded binaries.

The installer is currently **unsigned pre-alpha software**. Windows may display a SmartScreen warning. Do not disable system security globally; verify the artifact source and checksum before running it.

## What we want tested

Please help us test real behavior, not just whether the application opens.

Useful test areas include:

- installation and update behavior;
- first startup and restart;
- peer discovery and reconnect behavior;
- blockchain synchronization;
- wallet creation and restore;
- receiving and sending testnet transactions;
- testnet mining and block creation;
- coinbase maturity;
- persistence after restart;
- backup and restore;
- crashes, freezes, incorrect status displays, and inconsistent balances.

## How to report a problem

Open a GitHub issue:

https://github.com/Sheff1981/quintum-core/issues

Include, when possible:

- Windows version;
- QUINTUM build/commit;
- exact steps to reproduce;
- expected behavior;
- actual behavior;
- relevant logs or screenshots;
- whether the problem happens again after restart.

Do **not** publish recovery phrases, private keys, passwords, or other wallet secrets in an issue.

## Developers

Code review and contributions are welcome. See [CONTRIBUTING.md](../CONTRIBUTING.md).

Useful contributions include:

- reproducible bug reports;
- regression tests;
- networking and synchronization testing;
- Windows compatibility fixes;
- wallet and UX fixes;
- documentation corrections;
- carefully reviewed pull requests.

Consensus-affecting changes require especially strong justification, tests, and documentation.

## Important testnet notice

- Testnet QTM is for testing only.
- Testnet balances are not promised to transfer to mainnet.
- Testnet participation does not guarantee future rewards, allocation, listing, or monetary value.
- Mainnet consensus parameters remain subject to deliberate finalization before launch.

Built in public. Tested in public.
