# QUINTUM RandomX Public Testnet Testing

QUINTUM is **pre-alpha testnet software**. There is no Mainnet yet and Testnet QMU has no monetary value.

## Download

Official release:

https://github.com/Sheff1981/quintum-core/releases/tag/v0.0.2-prealpha

Choose the package for your system:

- **Windows x64:** `QUINTUM-Core-Setup-0.0.2-prealpha-x64.exe`
- **Ubuntu/Debian Linux x64:** `QUINTUM-Core-0.0.2-prealpha-linux-x64.deb`
- **Other Linux x64 / portable:** `QUINTUM-Core-0.0.2-prealpha-linux-x64.tar.gz`
- **macOS Apple Silicon:** `QUINTUM-Core-0.0.2-prealpha-macos-arm64.dmg`
- **macOS Intel:** `QUINTUM-Core-0.0.2-prealpha-macos-x64.dmg`

Also download `SHA256SUMS.txt` and verify the selected package.

All desktop builds use the same QUINTUM Qt UI/UX and default to **RandomX Testnet**. No manual peer IP should be required; the first public bootstrap endpoint is `212.193.15.139:39444`, after which nodes exchange peer addresses normally.

## Platform notes

- **Windows:** run the Setup `.exe`. The pre-alpha installer is not Authenticode-signed, so SmartScreen may warn.
- **Linux:** install the `.deb` on Debian/Ubuntu or extract the portable `.tar.gz` and run `run-quintum.sh`.
- **macOS:** open the correct `.dmg`, drag `QUINTUM.app` to Applications, then launch it. The pre-alpha app is ad-hoc signed but not Developer ID signed/notarized.

Verify the GitHub release and checksum rather than disabling system security globally.

## What we want tested

- installation, update and restart;
- automatic peer discovery and reconnect;
- blockchain synchronization;
- wallet creation and 24-word recovery;
- receiving and sending Testnet transactions;
- RandomX mining and real block acceptance;
- 500-block coinbase maturity;
- transaction/block relay;
- fork/reorg behavior;
- persistence after restart;
- backup/restore;
- crashes, freezes, incorrect status or balances.

## Reporting

Open an issue:

https://github.com/Sheff1981/quintum-core/issues

Include OS/version, QUINTUM version/commit, exact reproduction steps, expected result, actual result, and relevant logs/screenshots.

Never publish recovery words, private keys or wallet passwords.

## Testnet notice

- Testnet QMU is for testing only.
- Testnet balances are not promised to transfer to Mainnet.
- Testnet participation does not guarantee future rewards, allocations, listings or monetary value.
- Primary subsidy uses a transparent consensus-enforced 95% miner / 5% founder split; transaction fees go 100% to miners.
- Mainnet parameters remain subject to deliberate finalization after public network testing.

Built in public. Tested in public.
