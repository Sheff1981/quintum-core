# Android P2P EAGAIN investigation — 2026-10-11

## Protected baseline

- Source revision: `c4e5a7237c6b22ea37eb3db2a40b2c31c081f69d`
- Android and remote node both reported height 4286.
- 3254 blocks accepted, zero rejected.
- One active peer, completed handshake, stage `fully_synchronized`.
- Between 04:41:18Z and 04:45:02Z attempts increased 1912 → 1915 and retries 1911 → 1914.
- Current connection duration was 30877 ms at the second sample; these counters alone do not establish the reason for connection recycling.
- Reported last error: `version-receive peer_error=10 wire_error=0 socket_error=11 eof=0 partial_bytes=0 io_timeout=0 io_cancelled=0`.

## Source review

- `src/net/peer.cpp` already records `io_timeout` and `io_cancelled` on handshake failures.
- `prepare_cancellable_io()` uses EAGAIN as an internal deadline error on POSIX, but marks `timed_out=true`.
- Since the observed diagnostic says `io_timeout=0`, **do not classify the observed EAGAIN as that internal deadline**.
- `receive_exact()` records raw `recv()` errors; an EAGAIN from the syscall is another plausible source.
- Existing `tests/p2p_tests.cpp` checks SO_RCVTIMEO / EAGAIN and successful recovery for a session.
- Last-error fields may represent a prior failed attempt; an active peer at snapshot time does not imply the active peer has failed.

## Safe next steps

1. Trace socket blocking mode and SO_RCVTIMEO setup for the outbound handshake, including proxy paths.
2. Inspect connect/retry scheduling and peer lifecycle before changing network behavior.
3. Add deterministic regression coverage for syscall EAGAIN versus cancellable deadline, and verify error evidence is attributed to the correct attempt.
4. Compare tip hashes and monitor a physical device over a sustained connection.
5. Require existing native P2P tests, sanitizers, Android build and live sync to pass before merging any behavior change.

## Compatibility constraints

Do not modify genesis, consensus, RandomX, network magic, protocol, ports, addresses, wallet keys, blockchain storage, or existing Android application data. This investigation does not authorize merging into main or publishing an APK.

## Confirmed outbound socket lifecycle

In `connect_tcp_socket()`, outbound sockets are set nonblocking for TCP connect and then explicitly restored to **blocking** mode before `set_io_timeout(candidate, timeout_ms)` applies `SO_RCVTIMEO` and `SO_SNDTIMEO`. A subsequent blocking `recv()` may return EAGAIN/EWOULDBLOCK when the kernel receive timeout expires, without setting the separate `CancellableIo::timed_out` flag. Thus `socket_error=11 io_timeout=0` is consistent with a socket-level receive timeout; it is not sufficient evidence of nonblocking-mode leakage or protocol incompatibility.

The existing POSIX P2P regression test already checks EAGAIN after `SO_RCVTIMEO` and verifies that a subsequent successful receive clears failure evidence. Before changing behavior, inspect the Android call-site timeout value and whether handshake failures are retried at the intended cadence. Avoid increasing timeouts or changing peer selection without measured evidence.
