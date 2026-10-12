#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "Run this installer as root (for example: sudo ./install-randomx-testnet.sh)." >&2
    exit 1
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_BINARY="${1:-${SCRIPT_DIR}/quintumd}"
SERVICE_SOURCE="${SCRIPT_DIR}/quintumd-randomx-testnet.service"

TARGET_BINARY="/usr/local/bin/quintumd-randomx-testnet"
TARGET_SERVICE="/etc/systemd/system/quintumd-randomx-testnet.service"
DATA_ROOT="/var/lib/quintum-randomx"

if [[ ! -f "${SOURCE_BINARY}" ]]; then
    echo "QUINTUM node binary not found: ${SOURCE_BINARY}" >&2
    exit 1
fi

if [[ ! -f "${SERVICE_SOURCE}" ]]; then
    echo "systemd service file not found: ${SERVICE_SOURCE}" >&2
    exit 1
fi

if ! command -v systemctl >/dev/null 2>&1; then
    echo "systemd is required for this deployment package." >&2
    exit 1
fi

if ! id quintum >/dev/null 2>&1; then
    useradd         --system         --home-dir "${DATA_ROOT}"         --shell /usr/sbin/nologin         quintum
fi

install -d -o quintum -g quintum -m 0750 "${DATA_ROOT}"

# Keep the previous executable and unit file for a reversible upgrade.
# The blockchain directory is never deleted or rewritten by this installer.
backup_dir="$(mktemp -d /var/tmp/quintum-upgrade.XXXXXXXX)"
chmod 0700 "${backup_dir}"
had_binary=0
had_service=0
was_active=0
if [[ -f "${TARGET_BINARY}" ]]; then
    cp -p "${TARGET_BINARY}" "${backup_dir}/quintumd"
    had_binary=1
fi
if [[ -f "${TARGET_SERVICE}" ]]; then
    cp -p "${TARGET_SERVICE}" "${backup_dir}/service"
    had_service=1
fi
if systemctl is-active --quiet quintumd-randomx-testnet.service; then
    was_active=1
fi
rollback() {
    local status=$?
    trap - ERR
    echo "Upgrade failed (exit ${status}); restoring previous service and executable." >&2
    systemctl stop quintumd-randomx-testnet.service >/dev/null 2>&1 || true
    if [[ "${had_binary}" -eq 1 ]]; then
        cp -p "${backup_dir}/quintumd" "${TARGET_BINARY}"
    else
        rm -f "${TARGET_BINARY}"
    fi
    if [[ "${had_service}" -eq 1 ]]; then
        cp -p "${backup_dir}/service" "${TARGET_SERVICE}"
    else
        rm -f "${TARGET_SERVICE}"
    fi
    systemctl daemon-reload || true
    if [[ "${was_active}" -eq 1 ]]; then
        systemctl start quintumd-randomx-testnet.service || true
    fi
    echo "Rollback files retained in ${backup_dir}" >&2
    exit "${status}"
}
trap rollback ERR

# Stop only the RandomX service. Historical quintumd remains untouched.
systemctl stop quintumd-randomx-testnet.service
temporary="${TARGET_BINARY}.new"
install -o root -g root -m 0755 "${SOURCE_BINARY}" "${temporary}"
mv -f "${temporary}" "${TARGET_BINARY}"
install -o root -g root -m 0644 "${SERVICE_SOURCE}" "${TARGET_SERVICE}"
systemctl daemon-reload
systemctl enable quintumd-randomx-testnet.service >/dev/null
systemctl start quintumd-randomx-testnet.service
sleep 1
systemctl is-active --quiet quintumd-randomx-testnet.service
trap - ERR
echo "Previous executable/service backup: ${backup_dir}"

if command -v ufw >/dev/null 2>&1 &&
   ufw status 2>/dev/null | grep -q '^Status: active'; then
    ufw allow 39444/tcp comment 'QUINTUM RandomX Testnet P2P' >/dev/null
fi

sleep 1

if ! systemctl is-active --quiet quintumd-randomx-testnet.service; then
    echo "QUINTUM RandomX Testnet service failed to start." >&2
    journalctl -u quintumd-randomx-testnet.service -n 80 --no-pager >&2 || true
    exit 1
fi

echo "QUINTUM RandomX Testnet node is running."
echo "P2P port: 39444/tcp"
echo "Data: ${DATA_ROOT}/randomx-testnet"
echo "Wallet: disabled (no wallet private keys on this public seed node)"
echo
systemctl --no-pager --full status quintumd-randomx-testnet.service || true
