# QUINTUM Community Bot

This directory contains the community-facing Telegram bot. It is deliberately isolated from consensus, P2P and mining code.

## Commands

- `/start` — welcome and project summary
- `/help` — command list
- `/status` — live testnet node status
- `/network` — live P2P/network information
- `/mining` — RandomX testnet mining information
- `/node` — node setup guidance
- `/github` — source repository
- `/download` — release/download guidance
- `/report` — bug reporting guidance

## Configuration

The bot reads secrets from environment variables. Never commit the Telegram token.

Required:

```
QUINTUM_TELEGRAM_BOT_TOKEN=...
```

Optional:

```
QUINTUM_RPC_URL=http://127.0.0.1:...
QUINTUM_RPC_USER=...
QUINTUM_RPC_PASSWORD=...
QUINTUM_GITHUB_URL=https://github.com/Sheff1981/quintum-core
```

`/status` uses `getblockchaininfo`, `getnetworkinfo` and `getmempoolinfo`.
`/network` uses `getnetworkinfo`.
`/mining` uses `getmininginfo`.

The integration is read-only: the bot does not expose wallet spending, mining-control, peer-control, or consensus mutation RPC calls.
