# QUINTUM Windows Distribution

Status: **pre-alpha / unsigned**

Stage 28 produces a self-contained x64 Windows package.

## Artifact contents

The CI artifact `quintum-windows-x64` contains:

- `QUINTUM-Core-Setup-0.0.1-prealpha-x64.exe`;
- `windows/QUINTUM.exe` plus deployed Qt runtime DLLs/plugins;
- `SHA256SUMS.txt`.

Qt deployment is generated with Qt's `windeployqt` tool and then smoke-tested outside the CMake build tree.

## Installer behavior

The installer is per-user and does not require administrator privileges for the normal path.

It installs program files under the user's LocalAppData Programs directory. Wallet, metadata and blockchain data remain in the Qt application-data directory, outside the program directory.

A fixed Inno Setup AppId is used across updates.

During an update, the installer is configured to close a running `QUINTUM.exe` through the Windows Restart Manager / close-applications path. CI verifies this using a persistent disposable wallet process.

Uninstall intentionally does not delete the user's QUINTUM AppData. Removing wallet/blockchain data is a separate explicit user action, not an installer side effect.

## CI release test

The Windows pipeline:

1. builds QUINTUM with MSVC and Qt 6.8;
2. runs the live GUI/runtime smoke test;
3. stages a portable deployment with `windeployqt`;
4. runs the portable deployment;
5. builds the Inno Setup installer;
6. performs a silent clean install;
7. launches the installed executable;
8. starts a long-running disposable QUINTUM process;
9. runs the installer again as an update and verifies the old process closes;
10. verifies AppData sentinel data survived update;
11. uninstalls silently;
12. verifies AppData sentinel data survived uninstall;
13. generates SHA-256 checksums;
14. uploads the installer and portable package.

## Signing

The pre-alpha installer is currently unsigned. No developer master key or fake signing credential is embedded in QUINTUM. Production releases require a real code-signing certificate and an explicit protected signing workflow.
