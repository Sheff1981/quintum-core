# QUINTUM Core

QUINTUM is an independent proof-of-work cryptocurrency and peer-to-peer network being built from the ground up in modern C++23.

> Status: **pre-alpha / protocol design**. No mainnet exists yet. Coins created in development networks have no monetary value.


## Public RandomX Testnet — testers wanted

External testers are welcome on Windows, Linux and macOS.

**Official cross-platform pre-alpha release:**  
https://github.com/Sheff1981/quintum-core/releases/tag/v0.0.3-prealpha

Downloads:
- Windows x64: `QUINTUM-Core-Setup-0.0.3-prealpha-x64.exe`
- Linux x64 (Debian/Ubuntu): `QUINTUM-Core-0.0.3-prealpha-linux-x64.deb`
- Linux x64 portable: `QUINTUM-Core-0.0.3-prealpha-linux-x64.tar.gz`
- macOS Apple Silicon: `QUINTUM-Core-0.0.3-prealpha-macos-arm64.dmg`
- macOS Intel: `QUINTUM-Core-0.0.3-prealpha-macos-x64.dmg`

Download `SHA256SUMS.txt` from the same release and verify the package before running it.

All desktop packages contain the same Qt UI/UX and default to the isolated **RandomX Testnet**. Windows binaries are currently unsigned. macOS packages are ad-hoc signed for bundle integrity but are not Developer ID signed/notarized.

Testing instructions: [docs/TESTNET_TESTING.md](docs/TESTNET_TESTING.md)  
Community / first testers: [COMMUNITY.md](COMMUNITY.md)  
Code contributions: [CONTRIBUTING.md](CONTRIBUTING.md)

## Principles

- Independent network and genesis block
- Proof of Work
- UTXO accounting model
- Fully validating nodes
- Peer-to-peer block and transaction relay
- No hidden premine, hidden mint, master key, or developer backdoor; RandomX primary emission includes a public consensus-enforced 5% founder output
- Private keys remain under the user's control
- Cross-platform Qt desktop experience for Windows, Linux and macOS
- Consensus rules are documented and tested before mainnet launch

## Technology

- C++23
- CMake
- secp256k1 for transaction signatures
- RandomX v2.0.1 mining PoW; SHA-256 family hashing remains used for identifiers and non-mining cryptographic hashing
- Durable restart-safe blockchain storage
- Authenticated encrypted wallet storage with Argon2id + XChaCha20-Poly1305
- BIP32 deterministic recovery with 24-word English recovery phrases and atomic restoration
- Qt 6 Widgets desktop wallet shell, kept separate from the consensus/core library

## Repository map

- `src/` — node/core source code
- `docs/` — protocol and architecture documentation
- `docs/ru/START_HERE.md` — plain-language Russian project guide
- `.github/workflows/` — reproducible CI builds
- `TRADEMARKS.md` — QUINTUM trademark and brand-use policy
- `CONTRIBUTING.md` — contribution guidelines
- `docs/TESTNET_TESTING.md` — public testnet download and testing guide
- `COMMUNITY.md` — community bootstrap, first-tester goals and fair-launch principles

## Development rule

Consensus-critical constants are **DRAFT** until the genesis block and mainnet specification are deliberately frozen. After mainnet launch, incompatible consensus changes require explicit network-upgrade rules.

## Current milestone

**M15 — Release hardening + Windows distribution (Stage 28):** QUINTUM now produces a self-contained Windows desktop package and a tested per-user installer while keeping wallet/blockchain data outside the installation directory.

Wallet privacy/storage hardening:
- `wallet_meta.dat` v2 uses authenticated XChaCha20-Poly1305 encryption with the active wallet encryption key, a fresh nonce and authenticated network/wallet identity;
- legacy plaintext metadata v1 remains readable and is migrated automatically for encrypted wallets;
- explicit legacy `Encrypt Wallet` now migrates metadata in the same operation with rollback on failure;
- Windows wallet/metadata temporary files receive a protected current-user-only DACL before sensitive bytes are written; POSIX remains mode `0600`;
- the Stage 28 regression suite verifies encrypted metadata roundtrip/tamper detection, migration, complete backup/restore guards and Windows ACLs.

Backup/distribution:
- one checksummed, network-bound `.qtmbackup` contains `wallet.dat` plus wallet metadata; blockchain and rebuildable `wallet_state.dat` are intentionally excluded;
- desktop startup supports **Restore backup** and Settings creates **Backup complete wallet**;
- Windows CI uses Qt `windeployqt`, builds `QUINTUM-Core-Setup-0.0.1-prealpha-x64.exe` with Inno Setup, performs portable smoke, silent install, live-process update, uninstall and user-data preservation checks;
- release artifacts include the installer, portable deployment tree and `SHA256SUMS.txt`.

The installer is currently **unsigned pre-alpha**. Code-signing requires a release signing certificate and is deliberately not simulated.

Next: **Stage 29 — public testnet readiness:** seed/bootstrap infrastructure, multi-node soak/adversarial tests, external Windows installs and release-candidate networking before any mainnet freeze.

## Exchange listing / due diligence

A continuously maintained English exchange-facing documentation pack is available at [docs/exchange/README.md](docs/exchange/README.md). It covers technical due diligence, integration requirements, verification evidence and public development history. The package is pre-Mainnet and must be updated whenever consensus, networking, wallet, security, release or public-network behavior changes.


## License and brand

QUINTUM Core source code is distributed under the [MIT License](LICENSE).

The MIT License grants broad rights to use and modify the software, but it does **not** grant trademark rights or permission to present another project, network, product or service as official QUINTUM. Forks and modified versions should use their own distinct name and branding.

See [TRADEMARKS.md](TRADEMARKS.md) for the QUINTUM trademark and brand-use policy.
