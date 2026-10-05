#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

export LD_LIBRARY_PATH="${APP_ROOT}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export QT_PLUGIN_PATH="${APP_ROOT}/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${APP_ROOT}/plugins/platforms"

exec "${APP_ROOT}/bin/QUINTUM" "$@"
