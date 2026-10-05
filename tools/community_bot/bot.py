#!/usr/bin/env python3
"""QUINTUM Community Telegram bot.

Dependency-free, read-only bridge between Telegram Bot API and QUINTUM JSON-RPC.
Secrets are read only from environment variables.
"""
import base64
import json
import os
import time
import urllib.error
import urllib.parse
import urllib.request

TOKEN = os.environ.get("QUINTUM_TELEGRAM_BOT_TOKEN", "").strip()
RPC_URL = os.environ.get("QUINTUM_RPC_URL", "").strip()
RPC_USER = os.environ.get("QUINTUM_RPC_USER", "")
RPC_PASSWORD = os.environ.get("QUINTUM_RPC_PASSWORD", "")
GITHUB_URL = os.environ.get("QUINTUM_GITHUB_URL", "https://github.com/Sheff1981/quintum-core")

COMMANDS = {
    "/start": "QUINTUM Testnet Community Bot\n\nQUINTUM (QMU) is in active testnet development. Use /help to see commands.",
    "/help": "QUINTUM Testnet commands:\n/start — start\n/status — node status\n/network — network info\n/mining — mining info\n/node — run a node\n/github — source code\n/download — downloads\n/report — report a problem",
    "/node": "Run a QUINTUM Testnet node using the current instructions in the repository:\n" + GITHUB_URL,
    "/github": "QUINTUM source code and development:\n" + GITHUB_URL,
    "/download": "Official QUINTUM Testnet builds are published through the project repository/releases:\n" + GITHUB_URL + "/releases",
    "/report": "Found a QUINTUM Testnet bug? Report reproducible steps, OS, build/version and logs here:\n" + GITHUB_URL + "/issues",
}

def http_json(url, payload=None, headers=None):
    data = None if payload is None else json.dumps(payload).encode()
    req = urllib.request.Request(url, data=data, headers=headers or {})
    with urllib.request.urlopen(req, timeout=15) as response:
        return json.loads(response.read().decode())

def telegram(method, params):
    return http_json(
        "https://api.telegram.org/bot" + TOKEN + "/" + method,
        params,
        {"Content-Type": "application/json"},
    )

def rpc(method):
    if not RPC_URL:
        raise RuntimeError("QUINTUM_RPC_URL is not configured")
    headers = {"Content-Type": "application/json"}
    if RPC_USER or RPC_PASSWORD:
        raw = (RPC_USER + ":" + RPC_PASSWORD).encode()
        headers["Authorization"] = "Basic " + base64.b64encode(raw).decode()
    result = http_json(
        RPC_URL,
        {"jsonrpc": "2.0", "id": "community-bot", "method": method, "params": []},
        headers,
    )
    if "error" in result:
        raise RuntimeError(result["error"].get("message", "RPC error"))
    return result["result"]

def live(command):
    if command == "/status":
        chain, net, mem = rpc("getblockchaininfo"), rpc("getnetworkinfo"), rpc("getmempoolinfo")
        return (
            "QUINTUM Testnet status\n"
            f"Chain: {chain.get('chain', 'unknown')}\n"
            f"Blocks: {chain.get('blocks', 0)}\n"
            f"Headers: {chain.get('headers', 0)}\n"
            f"Sync: {float(chain.get('verificationprogress', 0)) * 100:.2f}%\n"
            f"Peers: {net.get('connections', 0)}\n"
            f"Mempool: {mem.get('size', 0)} tx"
        )
    if command == "/network":
        net = rpc("getnetworkinfo")
        return (
            "QUINTUM Testnet network\n"
            f"Active: {'yes' if net.get('networkactive') else 'no'}\n"
            f"Peers: {net.get('connections', 0)}\n"
            f"Outbound: {net.get('connections_out', 0)}\n"
            f"Known addresses: {net.get('knownaddresses', 0)}\n"
            f"Port: {net.get('localport', 'n/a')}\n"
            f"Protocol: {net.get('protocolversion', 'n/a')}"
        )
    if command == "/mining":
        info = rpc("getmininginfo")
        return (
            "QUINTUM Testnet mining\n"
            f"PoW: {info.get('powalgorithm', 'unknown')}\n"
            f"Blocks: {info.get('blocks', 0)}\n"
            f"Target spacing: {info.get('targetspacing', 'n/a')} sec\n"
            f"Mempool tx: {info.get('currentblocktx', 0)}"
        )
    raise KeyError(command)

def reply(chat_id, text):
    telegram("sendMessage", {"chat_id": chat_id, "text": text, "disable_web_page_preview": True})

def handle(message):
    text = message.get("text", "").strip()
    if not text.startswith("/"):
        return
    command = text.split()[0].split("@")[0].lower()
    chat_id = message["chat"]["id"]
    try:
        if command in ("/status", "/network", "/mining"):
            answer = live(command)
        else:
            answer = COMMANDS.get(command, "Unknown command. Use /help.")
    except Exception as exc:
        answer = "QUINTUM node data is temporarily unavailable.\n" + str(exc)
    reply(chat_id, answer)

def main():
    if not TOKEN:
        raise SystemExit("QUINTUM_TELEGRAM_BOT_TOKEN is required")
    offset = 0
    while True:
        try:
            data = telegram("getUpdates", {"offset": offset, "timeout": 30, "allowed_updates": ["message"]})
            for update in data.get("result", []):
                offset = max(offset, update["update_id"] + 1)
                if "message" in update:
                    handle(update["message"])
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as exc:
            print("community-bot:", exc, flush=True)
            time.sleep(3)

if __name__ == "__main__":
    main()
