# Android P2P and blockchain sync recovery development log

## Intent and constraints

Ship real RandomX Testnet P2P/sync diagnostics on Android ARM64 without changing
genesis, chain parameters, wire protocol, ports, PoW, difficulty, emissions,
address format, keys, or chain/wallet storage formats. Diagnostics are local
metadata; nothing is uploaded by the Android application automatically.

## Investigated evidence

- PR #45/android-foundation remained at 89ec800. Previously verified fixes in
  PR #46 at 6ea4228 must be retained, including EINTR handling, Service ownership,
  nonblocking status reads and the Mining nested-scroll crash fix.
- Phone reports at 06:11:49 and 06:15:53 UTC on 2026-10-09 confirm a completed
  protocol-v2 handshake with 212.193.15.139:39444 and advertised height 4286.
  They do not establish validated chain progress or a RandomX bottleneck.
- Legacy code 4002 encloses the entire synchronous initial sync operation:
  network reads, header validation, block download, block validation and storage.
  It is not a specific RandomX state. The chain mutex remains held throughout
  this operation; the new observer is separate from that mutex.
- CI host probes previously completed encrypted handshake and chainwork; one
  header request failed, another received 2000 headers. VPS SSH/logs and its
  localhost-only RPC were not available. No server cause is claimed.
- Android verification currently uses interpreter flags; host builds use
  upstream-recommended CPU flags. This source difference is not performance
  evidence. No RandomX optimization is applied without reproducible timings.

## Implemented recovery instrumentation

- Observe actual TCP, VERSION, validation, VERACK, encrypted upgrade and
  handshake stages without adding wire fields or bypassing checks.
- Observe header wait/validation and block wait/validation, local validated
  height, request/receive/body counters, throughput and time since network IO.
- Measure RandomX cache initialization and hash calls only inside a scoped
  synchronization-thread observer. Mining hashes are excluded. Existing header
  and full-block validation paths remain authoritative.
- Header metrics distinguish newly verified headers, known metadata, the first
  invalid header, the unexamined suffix and accepted/rejected batches. Receiving
  headers does not mean they are consensus-valid or stored in the active chain.
- Fully-synchronized telemetry requires validated local height/work, an active
  prepared peer, and no pending block/compact requests or readable peer traffic.
- A separate bounded journal/snapshot remains readable during chain locking.
  Two JSONL files rotate at 256 KiB each. Export is bounded to 512 KiB. Restart
  restores the last successful synchronization timestamp, but not a claim that
  a newly started process is already connected/synchronized.
- Diagnostics UI shows actual measurements, recent events, explicit copy,
  chosen-document export, and diagnostic-only clear. Mining and Diagnostics
  share the existing parent's vertical scroll; inner containers do not scroll.
- Service/activity lifecycle events use IO workers. The existing foreground
  notification, CPU wake lock, serialized ownership and Android 15 dataSync
  timeout handling remain in place. Platform/user battery restrictions apply.

## Verification design and current evidence

- Local strict C++23 syntax checks pass for changed network/RandomX sources and
  observer compatibility tests. Standalone diagnostic storage tests pass,
  including rotation, restart, concurrent access, errors, export and clear that
  preserves unrelated wallet/chain sentinel files.
- Added real handshake/encrypted-upgrade event assertions and valid/invalid
  header/block sync metric tests to the existing native suites.
- Added Robolectric Compose tests measuring Mining/Diagnostics under a real
  parent scroll, bounded presentation tests and actual ContentResolver export
  tests. They are not physical-device or SAF-picker verification.
- Added x86_64 Android emulator tests using the real packaged JNI Service,
  background/foreground continuity, visible notification, mining stop independent
  of P2P, and process-restart journal persistence with a changed PID. Production
  ABI remains ARM64; x86_64 is opt-in for this test build only.
- Added Linux/Windows live-sync probes using a disposable walletless node, the
  unchanged RandomX Testnet validators, and restart checking of validated height.
  Host probes are not proof of ARM64-device convergence or screen-off behavior.

## Observed GitHub Actions results (2026-10-09 UTC)

- Commit `1c2d75b` integrated the preserved fixes and recovery diagnostics on
  `android-foundation`, without merging PR #45 or #46.
- [Native build 37896087180](https://github.com/Sheff1981/quintum-core/actions/runs/37896087180)
  passed Linux/Windows suites. Linux recorded 38/38 tests passing, including
  P2P, sync and journal tests. The live handshake probe recorded protocol 2,
  encrypted transport, advertised height 4286 and no handshake failure.
- [Sanitizers 37896087138](https://github.com/Sheff1981/quintum-core/actions/runs/37896087138)
  passed all 38 tests. This includes the interrupted-receive regression cited
  in PR #46 comment 6074309491; the EINTR fix is retained.
- [ARM64 build 37896087111](https://github.com/Sheff1981/quintum-core/actions/runs/37896087111)
  passed and produced the signed installable APK. Subsequent ARM64 builds and
  JVM tests also passed compilation/tests; one obsolete run failed only during
  prerelease publication with HTTP 403, after uploading its APK artifact.
  Publication now belongs only to a current-head push; PR/stale runs retain
  tested artifacts without attempting obsolete release tags.
- [Android diagnostics 37897766358](https://github.com/Sheff1981/quintum-core/actions/runs/37897766358)
  passed Kotlin/Robolectric Compose/export tests and both real JNI emulator
  instrumentation invocations. The emulator exercised a backgrounded activity,
  ongoing notification, foreground return, a stop-mining call that left core
  running, and journal persistence across force-stop with a changed process PID.
  It did not mine an actual block or prove physical ARM64 synchronization.
- Initial diagnostics CI failed before tests because setup-android requested
  the removed SDK package `tools`. Commit `45f3d91` selects `platform-tools`;
  subsequent test jobs passed.
- [Live comparison 37896087246](https://github.com/Sheff1981/quintum-core/actions/runs/37896087246):
  Windows fully validated 4286 headers and 4286 block bodies, reported one active
  prepared peer and restored height 4286 after restart, without wallet files.
  It recovered after one header-wait receive failure. Header validation took
  99.339 s; block validation 354.608 s; scoped RandomX hashes 288.134 s total
  (overlapping the header/block durations), caches 2.029 s.
- The initial Linux probe reached validated height 3260 before the external
  timeout: headers 79.188 s, block validation 890.296 s and scoped RandomX
  hashes 172.279 s at its final snapshot. It was progressing, not proven stalled.
  It was configured without a Release build type while Windows used Release;
  commit `550156c` corrects this comparison. Full Linux completion remains
  pending a matched Release probe. No consensus optimization is inferred.
- Commit `c4e005a` records per-session errno/EOF/partial-byte/wire evidence for
  synchronization failures, not just handshake failures. New tests cover EOF,
  partial receive, timeout, recovery, moves and session isolation. Verification
  of that follow-up and matched Release comparison is running; final outcomes
  are tracked in PR #45.

No physical Android device is attached to this environment; user-device sync
convergence, Xiaomi battery behavior, actual SAF picker and long screen-off
acceptance must be verified on a device. No automatic PR merge is requested.
