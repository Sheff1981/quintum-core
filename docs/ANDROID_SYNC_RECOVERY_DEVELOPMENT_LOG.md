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

## Bounded initial synchronization (2026-10-09; validation pending)

Baseline inspected: android-foundation / PR #45, a062ab87f0eab5ba82e1627ed67d626b874bad75. The original src/net/runtime.cpp:2399-2434 ran sync_from_peer under state_mutex_ before inserting the LivePeer into peers_. That call ran on run_loop via maintain_outbound/accept_inbound, stopping normal peer servicing, accounting and cancellation. src/net/sync.cpp:1200 called Chainstate::validate_headers for up to 2000 headers; its overlay was discarded afterward, causing full block acceptance to repeat RandomX checks. pow.cpp:117 already reused a thread-local VM/cache. Android RandomX explicitly selected V2 interpreter flags; cache reuse was not missing.

One persistent ValidationWorker now admits exactly one initial-setup task, with no backlog or detached threads. A unique heap-owned peer is transferred only after its future completes. The event loop continues existing peer servicing; new connection setup is deferred while the single slot is occupied. Header metadata snapshots exclude block bodies, UTXOs and undo. The worker hashes outside state_mutex_, keeps the original ordered consensus overlay and reports each completed header. It services bounded ping/pong/inventory traffic between hashes and detects liveness failures. Socket waits poll cancellation; partial framed reads remain bounded by the existing socket IO timeout. Shutdown cancels and joins before peer/node destruction. A hash already executing finishes before cancellation.

Per-operation node locks preserve atomic block persistence, wallet synchronization and reorganization rules. Header validation does not commit metadata. Actual body submission still rechecks contextual rules; a bounded thread-local cache reuses only the actual RandomX digest for the exact canonical header bytes plus seed. Target, PoW limit, ancestry, difficulty, timestamps and chainwork are checked normally. No externally supplied validation receipt is trusted. Active resynchronization uses the same worker. Relay replies serialize owned payloads and release the state lock before socket sends.

Diagnostics expose lifecycle, actual endpoints, validation count/current item/batch progress, committed/rejected counts, real hash latency, aggregate hash/cache time, result reuse, message/activity UTC, disconnect/timeout reasons and retries. Android foreground-service ownership, Activity resume behavior, data formats and wallet storage were not changed. No genesis, network magic, PoW rules, RandomX version/flags, emission, difficulty adjustment, addresses or wire changes. Android standalone NDK CI explicitly uses Release; the APK native build configuration requires device profiling before per-hash optimization claims.

Local evidence: strict GCC 14 C++23 syntax checks for changed production/test units; ValidationWorker admission/cancellation test and diagnostic tests; worker-only ThreadSanitizer executed successfully. Full local build unavailable: environment lacks CMake/NDK/SDK and its configured HTTP proxy is unreachable; the network permission operation twice stalled and was aborted. Sources were restored through the GitHub connector. Native, Android and live-network CI results will be recorded after the branch update. Added real socket regression tests cover nonblocking status/cancellation while the server withholds headers and ping/pong during header validation outside the node lock. RandomX regression measures two raw hashes versus exact-result reuse, also rejecting reuse across changed input/seed.

No ARM64 before/after measurements, physical-device screen-off/background/interruption/restart validation, VPS access, Windows runtime or full-runtime TSan success is claimed here. User-observed 23-minute RandomX / 33-minute header validation is evidence from the phone, not a benchmark run in this environment. Real-network acceptance (blocks accepted >0, increasing local height, tip convergence and restart durability) remains pending. PR #45 must not be merged automatically.

### First published verification (5fa18aa662320b91321eed76f0753ee98f9cb89a)

- Linux actual VPS protocol-v2 synchronization: https://github.com/Sheff1981/quintum-core/actions/runs/37936063397/job/113838304738 succeeded. 4286 real bodies accepted, local/advertised height 4286; restart restored 4286 and no wallet was created by the walletless probe. Real RandomX calls: 4286, hash verification total 74005 ms, final hash 17382 us, cache initialization total 783 ms. Retries produced 5934 header validation operations and 10220 exact result reuses. These are Linux observations, not ARM64 measurements. The initial peer recovered from two actual transport/connect failures without disabling security checks.
- ARM64 core, Android JVM tests, signed APK build, certificate validation and artifact upload passed: https://github.com/Sheff1981/quintum-core/actions/runs/37936063220 (artifact 11618273521). x86_64 real-JNI emulator foreground/background/service/process-restart checks and JVM tests passed: https://github.com/Sheff1981/quintum-core/actions/runs/37936063244 . Neither proves physical ARM64 convergence or OEM screen-off behavior.
- Linux native and ASan/UBSan suites initially reported 37/39. The two failures were a legacy test requiring absent height during the former mutex hold, and a new ping test assuming TCP packet arrival immediately after send. The updated test requires a readable unchanged height and explicitly synchronizes ping packet arrival during a simulated slow verification interval. All other suites passed; the first Windows matrix job was cancelled by matrix fail-fast, not a Windows correctness result. Complete suites will be rerun for the follow-up commit.

### Complete suites and framed-IO shutdown follow-up

Code commit b586d5b58f8fbace0e55970ea3da086ea0acb589 passed Linux and Windows 39/39 native tests (https://github.com/Sheff1981/quintum-core/actions/runs/37937399965), ASan/UBSan (https://github.com/Sheff1981/quintum-core/actions/runs/37937399957), signed ARM64 APK/core (https://github.com/Sheff1981/quintum-core/actions/runs/37937399898) and x86_64 real-JNI lifecycle/restart plus JVM tests (https://github.com/Sheff1981/quintum-core/actions/runs/37937408852). Both Linux and Windows actual VPS synchronization/restart passed again (https://github.com/Sheff1981/quintum-core/actions/runs/37937399970). Linux microbenchmark: unchanged uncached RandomX hash 12738 us, first normal verification 6930 us, exact-result recheck 5 us; five actual hash calls, including changed-input/seed checks. This compares uncached versus cached work within one process and is not an ARM64 before/after benchmark or a per-hash algorithm speedup.

A shutdown review identified that cancellation only before receive_command was insufficient after a partial wire frame entered receive_exact. Session IO now observes an owner-installed runtime cancellation token inside bounded readiness polls and each partial read/write loop. Existing idle timeouts still reset only on actual byte progress; they are not increased. Cancellation is a distinct internal PeerError, never a fabricated peer timeout/EOF/errno. POSIX readiness uses poll to avoid FD_SETSIZE memory hazards in an Android process with many descriptors; Windows keeps select. Ownership stays exclusive: no foreign-thread socket close, detached thread or partial-message acceptance. A regression consumes a real three-byte prefix, cancels the blocked frame and requires completion with exact partial-byte evidence. Shutdown also avoids publishing a just-completed setup or scoring locally cancelled peers as failures. Full CI will rerun for these IO changes.

The local environment additionally rejects AF_UNIX send with EPERM, so the framed-socket regression cannot execute locally; it must be assessed in CI. Local strict syntax validation continues to pass. Physical ARM64 benchmarks and foreground/screen-lock/OEM/network-interruption/chain-and-wallet restart acceptance are still outstanding. Full-runtime TSan was not run; the worker-only TSan result is not represented as full-runtime coverage.

## 2026-10-11: Android VPN-dependent peer loss (investigation open)

- User reports that Huawei and Xiaomi peers appear with VPN and disappear without VPN. Laptop reportedly works in both conditions. This does not prove ISP blocking.
- VPS packet capture: three client-side FIN packets at 08:11:30, 08:12:35 and 08:13:40 from 89.125.21.139 to port 39444, roughly 65 seconds apart. VPN/NAT intermediary remains a possible closer.
- Huawei cc49c517, VPN off, 06:17 UTC: remote height 4286, local height 0, headers received 48, verified 0; sync_error=2 peer_error=10 chain_error=19 socket_error=103. Handshake completed; initial setup failed.
- Huawei same revision, VPN on, 06:19 UTC: received 72, verified 0, validation active at capture time. Last-error fields can be stale; not evidence of a completed sync.
- Older c4e5a723 Android revision previously synchronized to 4286, accepted 3254 blocks without rejection, but had reconnect churn. Do not attribute that success to cc49c517.
- Candidate code path: src/net/sync.cpp services ping/pong and other P2P messages during CPU-intensive header validation; network failure can cancel validation. Root cause not yet confirmed.
- Base android-foundation cf531302 CI: Ubuntu build and sanitizer jobs fail at tests/sync_tests.cpp:392 on expected diagnostics field; Android Core and JVM diagnostics jobs passed.
- Draft PR #49: ef30ba8 adds missing-field/snapshot output on test failure. Earlier cd73ab0 introduced invalid PeerIoFailure member references and broke ARM64 compilation; bdde7b0 removed them.
- No validated VPN-independent fix, APK or two-node regression test yet. Do not merge until tests and real-device reconnection and IBD pass.
- RandomX version and consensus unchanged. User permits debugging RandomX implementation where necessary, but consensus-changing work requires explicit separate review.

## 2026-10-11 07:20:39 UTC: Android c4e5a723 fully synced but disconnected

- Core running; local_height=remote_height=4286; last_sync_utc=07:18:57 UTC; blocks_accepted=3254, blocks_rejected=0, headers_rejected=0.
- Active peers=0, known peers=245, attempt_count=2409, retry_count=2408; peer_state=disconnected; disconnect_reason=initial setup failed.
- Last error text: version-receive peer_error=10 wire_error=0 socket_error=11 eof=0 partial_bytes=0 io_timeout=0 io_cancelled=0. This error may be stale; do not assume it caused the most recent disconnect.
- Last network message 07:20:26 UTC; connection_duration_ms=12740; last network progress 12576 ms ago. No current core/service error reported.
- Important distinction: this revision has already synchronized successfully; persistent connectivity and correct reconnection are separate acceptance criteria. This report does not demonstrate a RandomX consensus failure.
- Next checks: identify exact failure in prepare_live_peer, classify EAGAIN/retry behavior and socket ownership, reproduce VPN transition without losing synchronized chain data.

## 2026-10-11 07:27:16 UTC: Xiaomi connected WITH VPN (user correction)

- Build c4e5a7237c6b22ea37eb3db2a40b2c31c081f69d; user explicitly corrected the network condition: VPN was ON for this 07:27:16 snapshot. Earlier interpretation as VPN OFF was wrong.
- Active peers=2, peer_state=active, stage=fully_synchronized; local_height=remote_height=4286; blocks_rejected=0.
- Previous snapshot at 07:20:39 UTC: active peers=0, attempts=2409, known peers=245. New snapshot: attempts=2427, known peers=247.
- 18 additional connection attempts over 397 seconds; current connection_duration_ms=4578. Because the VPN condition changed, these snapshots cannot establish VPN-OFF recovery or persistent stability.
- Last error still reports version-receive socket_error=11 (EAGAIN) despite active peers, suggesting historical error state remains visible; do not attribute current session failure to it.
- No code fix or APK validation is implied by this observational report. Do not cite this sample as evidence that Android connects without VPN.

## 2026-10-11 07:35:04 UTC: Xiaomi VPN OFF, no active peers

- Device Xiaomi, VPN explicitly OFF; build c4e5a7237c6b22ea37eb3db2a40b2c31c081f69d.
- active_peers=0; known_peers=249; local_height=remote_height=4286; blocks_accepted=3254; blocks_rejected=0.
- attempt_count=2448 (up 21 from 07:27:16 VPN ON snapshot); retry_count=2447.
- Last reported failure: tcp-connect peer_error=7 wire_error=0 socket_error=0 eof=0 partial_bytes=0 io_timeout=0 io_cancelled=0.
- stage=handshake_complete, peer_state=disconnected, disconnect_reason=initial setup failed, last_network_message_utc=07:34:38, last_sync_utc=07:29:35.
- Interpretation: direct connectivity is unreliable or unavailable after VPN OFF, with TCP connect failures among attempts. The last error may be historical; handshake telemetry means some connections progressed further. Do not conclude definitive ISP filtering without direct route tests.
- Compare to VPN ON 07:27:16 snapshot (active_peers=2). Do not change consensus, RandomX, genesis, or wallet state to address this.

## 2026-10-11: Huawei VPN-OFF connectivity (user observation)

- User reports Huawei also works without VPN.
- This is a qualitative observation; no new Huawei diagnostic snapshot accompanied it. Do not infer stable session duration, block sync completion, or active peer count.
- Xiaomi VPN OFF at 07:35 UTC had zero active peers and tcp-connect failure; Huawei VPN OFF working means blanket claims that VPN is mandatory, or that the VPS port is universally blocked without VPN, are unsupported.
- Compare per-device Android build, network path, connection lifecycle, and reconnection telemetry before changing code. Preserve consensus and wallet.

## 2026-10-11: Both Xiaomi and Huawei show intermittent VPN-OFF P2P

- User confirms Huawei behaves like Xiaomi: peers can appear without VPN but periodically disappear.
- This supersedes any interpretation that Huawei is stably connected without VPN.
- Both devices exhibit intermittent VPN-OFF connectivity; exact causes remain unproven and may include shared app reconnect logic and differing mobile-network routes.
- Next priority: inspect common Android/Core connection lifecycle and failure classification, add deterministic reconnect regression tests, then verify APK on both phones. Do not alter RandomX or chain parameters.
