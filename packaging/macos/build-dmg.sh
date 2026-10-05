#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 5 ]]; then
    echo "usage: $0 <QUINTUM.app> <Qt-root> <version> <arch> <output-dir>" >&2
    exit 2
fi

source_app="$(realpath "$1")"
qt_root="$(realpath "$2")"
version="$3"
arch="$4"
mkdir -p "$5"
output="$(cd "$5" && pwd -P)"

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
stage="$output/stage-${arch}"
app="$stage/QUINTUM.app"

rm -rf "$stage"
mkdir -p "$stage" "$output"
cp -R "$source_app" "$app"

macdeployqt="$qt_root/bin/macdeployqt"
if [[ ! -x "$macdeployqt" ]]; then
    echo "macdeployqt not found: $macdeployqt" >&2
    exit 1
fi

"$macdeployqt" "$app" -always-overwrite

mkdir -p "$app/Contents/Resources/docs"
install -m 0644 "$repo_root/README.md" "$app/Contents/Resources/README.md"
install -m 0644 "$repo_root/docs/TESTNET_TESTING.md" "$app/Contents/Resources/docs/TESTNET_TESTING.md"

# Ad-hoc signing only seals the bundle structure. It is not Developer ID signing
# and it does not imply Apple notarization.
codesign --force --deep --sign - "$app"

QT_QPA_PLATFORM=offscreen     "$app/Contents/MacOS/QUINTUM" --smoke-test

zipfile="$output/QUINTUM-Core-${version}-macos-${arch}.zip"
ditto -c -k --sequesterRsrc --keepParent "$app" "$zipfile"

dmg_root="$output/dmg-${arch}"
rm -rf "$dmg_root"
mkdir -p "$dmg_root"
cp -R "$app" "$dmg_root/QUINTUM.app"
ln -s /Applications "$dmg_root/Applications"

dmg="$output/QUINTUM-Core-${version}-macos-${arch}.dmg"
rm -f "$dmg"
hdiutil create     -volname "QUINTUM Core"     -srcfolder "$dmg_root"     -ov     -format UDZO     "$dmg"

rm -rf "$stage" "$dmg_root"

printf '%s\n' "$zipfile" "$dmg"
