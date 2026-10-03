# QUINTUM Testnet Seed Node

This package is for a stable public Linux host used only to bootstrap QUINTUM Testnet peers.

## Install

The host needs a public IPv4 address and inbound TCP 38444 allowed both in the provider firewall/security group and the host firewall.

```bash
sudo useradd --system --home /var/lib/quintum --shell /usr/sbin/nologin quintum 2>/dev/null || true
sudo install -d -o root -g root -m 0755 /opt/quintum
sudo install -m 0755 quintumd /opt/quintum/quintumd
sudo install -d -o root -g quintum -m 0750 /etc/quintum
openssl rand -hex 32 | sudo tee /etc/quintum/wallet.pass >/dev/null
sudo chown root:quintum /etc/quintum/wallet.pass
sudo chmod 0640 /etc/quintum/wallet.pass
sudo install -m 0644 quintum-testnet-seed.service /etc/systemd/system/quintum-testnet-seed.service
sudo systemctl daemon-reload
sudo systemctl enable --now quintum-testnet-seed
```

If UFW is enabled:

```bash
sudo ufw allow 38444/tcp
```

Check the local listener:

```bash
systemctl status quintum-testnet-seed --no-pager
ss -ltn | grep ':38444'
```

## Verify from a different network before pinning the seed

Create a disposable passphrase file on a second Linux machine and run:

```bash
printf '%s\n' 'temporary-test-password-change-me' > probe-wallet.pass
chmod 0600 probe-wallet.pass
./quintumd --testnet --datadir ./probe-data --listen-port 0 \
  --wallet-passphrase-file ./probe-wallet.pass \
  --addnode PUBLIC_IP_OR_DNS:38444 --run-seconds 10
```

A valid QUINTUM handshake should finish with at least one peer, for example `Final peers: 1 (outbound 1)`. Merely seeing TCP 38444 open is not enough.

Do not add an IP/DNS name to the hardcoded Testnet seed list until this cross-network QUINTUM handshake succeeds.
