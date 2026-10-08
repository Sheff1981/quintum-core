# QUINTUM Android P2P — Engineering Diagnostic Log

Status: **OPEN — root cause not confirmed**  
Network: **RandomX Testnet**  
Reference branch: `android-foundation`, PR #45  
Last documented: 2026-10-08

## Evidence and observations

- Android device: Xiaomi M2011K2G, arm64-v8a, 8 CPU cores.
- Native core starts and reports `Core running`; one measured startup was **14 seconds**. Earlier observations indicated **2–3 minutes**, not consistently reproduced.
- Android node remains at **height 0**, with **0 connected peers**, while **1 known peer address** is displayed.
- VPS seed address: `212.193.15.139:39444` (RandomX Testnet). This is a known address, not proof of an established P2P session.
- Android UI TCP transport test reported **TCP connected in 4 ms; handshake not tested**. This confirms TCP reachability at the time of the test, not protocol compatibility or block synchronization.
- A VPN icon was visible in the device status bar during testing; its impact has **not** been established.
- Desktop QUINTUM Core previously displayed height **3550** and one peer; the observations are not necessarily simultaneous and do not establish the current network tip.
- Mining was disabled on Android (0 threads, no measured hashrate). Wallet functionality is not yet enabled in the Android testnet build.

## Instrumentation changes

- `src/net/runtime.cpp`: Android-only outbound diagnostic states for peer selection, discovery/reconnect failures, post-handshake preparation failure, and completed connection.
- `src/net/runtime.hpp`: atomic diagnostic state, readable across threads.
- `src/android/native_bridge.cpp`: JNI accessor `nativeP2pDiagnostic`.
- `android/app/src/main/java/org/quintum/wallet/core/NativeCore.kt`: Kotlin JNI declaration.
- `android/app/src/main/java/org/quintum/wallet/MainActivity.kt`: diagnostic display in Network technical details.
- Latest instrumentation commit: `19134e35198b08aa2d20b118d1701b25f8819cb2`.

## Diagnostic codes (Android development builds)

| Code | Meaning |
| --- | --- |
| -1 | Native runtime not available |
| 0 | No outbound attempt recorded |
| 10 | Selecting peer / attempting outbound connection |
| 1000 + peer error | Reconnection transport or handshake failed |
| 2000 + 100 × discovery error + peer error | Discovery or outbound connection failed |
| 3001 | Reconnected peer passed transport handshake but failed post-handshake preparation |
| 3002 | Discovered peer passed transport handshake but failed post-handshake preparation |
| 4000 | Peer successfully prepared and added to live peer list |

**Note:** These are internal development codes, not stable public protocol codes. A completed TCP socket connection is not equivalent to a successful QUINTUM P2P handshake.

## Open verification tasks

1. Confirm Android GitHub Actions build and tests pass for the instrumentation commit.
2. Install the resulting APK **without clearing application data** and capture the diagnostic code while peers remain zero.
3. Correlate the code with `QUINTUM-P2P` Android logcat messages and server-side inbound handshake logs.
4. Check network magic, protocol version, genesis, RandomX chain parameters, and peer post-handshake `chainwork` negotiation only after transport evidence is available.
5. Verify a live Android peer appears on both Android and VPS, headers/blocks synchronize, and Android height advances.
6. Repeat across Wi-Fi and mobile data; test VPN routing separately rather than treating VPN as the proven cause.
7. Re-test application restart, peer reconnection, and wallet/blockchain data preservation.

## Release gate

Do not report Android P2P as fixed until **peer count > 0, handshake confirmed, height synchronization verified, and reconnect after restart verified**. Do not mine against an isolated height-0 chain as a production-equivalent test.

## UI defect tracked separately

The VPS diagnostic action was reported to be below the visible viewport. Ensure the Network screen is vertically scrollable, the diagnostic action remains reachable, and the result is visible after the action. This is independent of the unresolved P2P failure.
