# QUINTUM RandomX Testnet: optional Tor P2P ingress

This is an **optional transport** for the existing P2P service. It does not change genesis, RandomX, consensus, network magic, chain data, or the wallet.

## VPS: publish the existing P2P listener as a Tor v3 onion service

Prerequisites: an already running QUINTUM RandomX Testnet node listening on TCP **127.0.0.1:39444 or 0.0.0.0:39444** and an installed Tor daemon. Do not expose the RPC port (39445) over Tor.

1. Install Tor using the OS package manager and confirm the daemon starts.
2. Add these lines to the Tor daemon's `torrc` (the path varies by OS):

   ```torrc
   HiddenServiceDir /var/lib/tor/quintum-testnet/
   HiddenServiceVersion 3
   HiddenServicePort 39444 127.0.0.1:39444
   ```

3. Ensure `/var/lib/tor/quintum-testnet/` is owned by the Tor service account and is not accessible by other users. Restart Tor and inspect its logs for configuration errors.
4. Read `/var/lib/tor/quintum-testnet/hostname` to obtain the generated v3 `.onion` hostname. **Never publish or copy the private keys from this directory.**
5. Confirm the QUINTUM daemon is listening on localhost TCP port 39444 and that the Tor service is running. Do not replace the existing public IPv4 seed.
6. On a machine with a functioning Tor SOCKS5 proxy, verify the onion TCP route before changing any QUINTUM client configuration. A successful SOCKS5 CONNECT only establishes transport; a QUINTUM protocol handshake and full sync are separate required tests.

## Existing QUINTUM client support and missing integration

The C++ networking layer supports SOCKS5 and `AddressNetwork::tor_v3` via `ProxyRoutes.tor`, and validates v3 onion addresses. **It does not automatically install or start Tor.** The Android JNI startup currently does not populate `NetworkRuntimeConfig.proxies` and the hardcoded seed is IPv4 only. Therefore, creating the onion service alone will **not** make Android clients connect via Tor automatically.

Required before enabling automatic fallback:
- Provide a supported, user-consented Tor runtime on Android and verify SOCKS5 availability (no silent download, hidden proxy, or remote telemetry).
- Add a separately configurable onion bootstrap address, supplied from the actual VPS hostname, not a fabricated placeholder.
- Implement bounded direct-first / Tor-second retry logic with network-change handling and diagnostics showing selected transport and errors.
- Verify Android and Windows connection, headers/blocks, restarts, and failure behavior when Tor is unavailable. Do not send Tor onion names to system DNS.
- Keep direct IPv4 bootstrap available and preserve existing peer storage and wallets.

## Operational security

The onion service must forward **only** QUINTUM P2P TCP/39444. Do not map localhost RPC/39445, expose Tor service keys, disable QUINTUM consensus checks, or imply that Tor guarantees connectivity through every ISP.
