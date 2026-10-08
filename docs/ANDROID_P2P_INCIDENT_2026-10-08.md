# QUINTUM RandomX Testnet — Android P2P Incident Log

Status: OPEN — root cause not established.
Date opened: 2026-10-08.
Branch: `android-foundation`.
Affected device: Xiaomi M2011K2G, arm64-v8a, Android.
Expected bootstrap endpoint: `212.193.15.139:39444`.
Network: RandomX Testnet; P2P protocol version observed from server bot: 2.

## Observed evidence

1. Android QUINTUM Core reports `Node running` but `Peers: 0`, `Height: 0`.
2. The device is able to run the native core and read hardware information; this does **not** establish outbound P2P connectivity.
3. The user tested while Wi-Fi and VPN were enabled. No verified TCP or handshake trace has yet been collected.
4. A Windows QUINTUM Core screenshot shows chain height 3550, synchronization up to date and one peer. A Telegram bot screenshot reports one inbound peer and four *known* addresses on the server. These screenshots do not independently prove Android can reach the server, nor do they prove the two desktop/server observations were simultaneous.
5. Android wallet functionality is intentionally not enabled in this testnet build. Mining is off; neither is the cause of zero peers.

## Investigation hypotheses (not confirmed)

| Layer | Possible failure | Evidence required |
| --- | --- | --- |
| Bootstrap | Seed not loaded or address manager excludes seed | Known-address count; selected endpoint |
| Transport | TCP connect refused, timed out, or blocked on VPN/mobile/Wi-Fi path | Socket error, endpoint, elapsed time |
| Handshake | Wrong network magic, protocol version, genesis/chain parameters, or rejected version | PeerError/WireError on both ends |
| Peer lifecycle | Successful handshake followed by peer preparation or sync failure | Connection/disconnection reason, remote height |
| Retry | Backoff, persistent peer database or selection prevents new attempt | Retry deadline, addrman state |
| Server | Listener/firewall/connection cap prevents Android connection | Server-side inbound logs and port reachability |

## Implemented changes

- `e67e91e`: outbound resolver uses `AF_UNSPEC` to support IPv6-capable resolution.
- `a0bc26b`: re-enable persisted DNS seed addresses for retries.
- `26f08e6`, `fcbee77`, `56c51c9`, `33987df`: surface known-address count in Android.
- `b3f7305`: record Android native startup duration and result.
- `b9192d0`: permit fallback to an ephemeral local listener port on Android.
- `6915450`: distinguish slow native initialization from network connection in UI.
- `a943927`: collapse developer diagnostics on Network screen.
- `ba24744`: initial Android outbound connection/discovery failure logging.

None of these changes is evidence that the Android P2P issue has been resolved.

## Required diagnostic sequence

1. Install the APK built from the latest relevant commit; preserve the existing app data and keys. Do not uninstall the app.
2. Confirm `Known peer addresses` on Android and the actual selected bootstrap endpoint.
3. Capture `QUINTUM-P2P` Android log messages with error code and connection phase; map numeric codes to named errors in a follow-up.
4. Verify TCP reachability of `212.193.15.139:39444` from the same device/network path, with VPN still enabled.
5. Correlate timestamps with server logs; determine whether the inbound socket arrives and where the handshake stops.
6. Only after a stable peer connection is confirmed, verify header/block synchronization, restart persistence and reconnect after network changes.

## Acceptance criteria

- Android automatically discovers at least one valid QUINTUM Testnet peer without manually entering an IP.
- Successful protocol handshake with the server and a stable live peer count of at least one.
- Android chain height advances beyond genesis and converges to the server's valid chain.
- Reconnects after Wi-Fi/mobile transitions and app restart without deleting blockchain or wallet data.
- No consensus, genesis, network magic, address-prefix, wallet-key or emission parameter changes.

## Reporting rule

For each test, record UTC timestamp, APK commit SHA, device/Android version, Wi-Fi or mobile transport, VPN on/off, known addresses, selected endpoint, TCP/handshake error, peer count, block height, server-side corroboration and result. Do not record private keys, wallet seed phrases, RPC credentials or VPN secrets.
