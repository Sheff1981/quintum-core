# Android P2P and service follow-up

Base: `android-foundation`, PR #45, commit `89ec800e558cd31b49166381adf3971504b9e5a3`.

## Changes and scope

- Retry interrupted socket reads/writes and readiness waits. `EINTR` is not a disconnect. Readiness retries retain the original monotonic timeout deadline.
- Serialize native core ownership across Service instances. A late shutdown from an old Service must not stop a core adopted by its replacement; destroyed Services cannot start it.
- Hold a partial CPU wake lock only while the foreground Service exists, release it on destruction, and stop on Android 15's `dataSync` foreground timeout. This does not bypass Doze, user force-stop, vendor battery policy, or Android's six-hour background dataSync budget.
- Distinguish completed handshake, chainwork negotiation, initial blockchain sync, address exchange and mempool exchange in Android diagnostics. Log socket errors, handshake errors and sync rejection details. Initial sync still uses the existing consensus validation path.
- Add a read-only live-seed probe to CI. It uses the existing RandomX Testnet chain parameters, negotiates the same advertised capabilities as Android, and requests headers from the existing genesis hash. It creates no chainstate or wallet and does not mine. External endpoint failure is recorded separately from deterministic build/test failures.

Consensus, genesis, network identity, protocol version, ports, wallet storage and keys are unchanged. Existing persistent app data is retained.

## Evidence limits

The original `Peers: 0` observation alone does not prove handshake failure: the runtime adds a peer to its count only after initial sync and peer setup. The device/server failure's root cause remains unconfirmed until device and server logs identify the phase. A successful CI seed probe is evidence for the runner's route and host build, not for Android JNI, VPN routing, on-device RandomX execution or background reconnect.

The interrupted-I/O test runs against a real socket pair with a non-restarting signal handler. Local sandbox denies socket send operations; use the GitHub Actions red/green runs to verify this regression. Android ownership tests exercise replacement, failed adoption and destroyed-service startup. Android CI builds ARM64, runs JVM tests, assembles APK and checks native payload and signing certificate.

## Device acceptance

Install the fix-branch APK as an update without uninstalling or clearing data. Record its source SHA, UTC time, device/Android version, VPN state and transport. Capture `QUINTUM-SOCKET`, `QUINTUM-HANDSHAKE`, `QUINTUM-SYNC`, `QUINTUM-P2P` and `QUINTUM-Node` logcat tags, then correlate with seed logs. Verify version/verack and encrypted upgrade, chainwork, peer count above zero, validated height convergence, screen-off operation, network transitions and restart reconnect. Confirm app data survives upgrade. Do not include keys, mnemonic, signing secrets or RPC cookies in diagnostic reports.

Do not close the original Android P2P incident solely because CI passes.
