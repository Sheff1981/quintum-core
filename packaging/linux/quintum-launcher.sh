#!/usr/bin/env bash
set -euo pipefail

launcher="${BASH_SOURCE[0]}"
if command -v readlink >/dev/null 2>&1; then
    resolved="$(readlink -f -- "$launcher" 2>/dev/null || true)"
    if [[ -n "$resolved" ]]; then
        launcher="$resolved"
    fi
fi
APP_ROOT="$(cd -- "$(dirname -- "$launcher")" && pwd)"

export LD_LIBRARY_PATH="${APP_ROOT}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export QT_PLUGIN_PATH="${APP_ROOT}/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${APP_ROOT}/plugins/platforms"

exec "${APP_ROOT}/bin/QUINTUM" "$@"
