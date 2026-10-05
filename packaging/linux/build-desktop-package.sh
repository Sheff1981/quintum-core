#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 4 ]]; then
    echo "usage: $0 <QUINTUM-binary> <Qt-root> <version> <output-dir>" >&2
    exit 2
fi

binary="$(realpath "$1")"
qt_root="$(realpath "$2")"
version="$3"
output="$(realpath -m "$4")"

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
portable_root="$output/QUINTUM"
deb_root="$output/deb-root"

rm -rf "$portable_root" "$deb_root"
mkdir -p     "$portable_root/bin"     "$portable_root/lib"     "$portable_root/plugins"

install -m 0755 "$binary" "$portable_root/bin/QUINTUM"
install -m 0755     "$repo_root/packaging/linux/quintum-launcher.sh"     "$portable_root/run-quintum.sh"
install -m 0644 "$repo_root/README.md" "$portable_root/README.md"
install -m 0644 "$repo_root/docs/TESTNET_TESTING.md" "$portable_root/TESTNET_TESTING.md"

shopt -s nullglob
qt_libs=("$qt_root"/lib/libQt6*.so*)
if (( ${#qt_libs[@]} == 0 )); then
    echo "Qt runtime libraries were not found under $qt_root/lib" >&2
    exit 1
fi
cp -a "${qt_libs[@]}" "$portable_root/lib/"

for plugin_dir in platforms imageformats iconengines networkinformation tls; do
    if [[ -d "$qt_root/plugins/$plugin_dir" ]]; then
        cp -a "$qt_root/plugins/$plugin_dir" "$portable_root/plugins/"
    fi
done

if [[ ! -f "$portable_root/plugins/platforms/libqxcb.so" ]]; then
    echo "Qt XCB platform plugin was not packaged" >&2
    exit 1
fi

mkdir -p "$output"
tarball="$output/QUINTUM-Core-${version}-linux-x64.tar.gz"
tar -C "$output" -czf "$tarball" QUINTUM

mkdir -p     "$deb_root/DEBIAN"     "$deb_root/opt/quintum"     "$deb_root/usr/bin"     "$deb_root/usr/share/applications"     "$deb_root/usr/share/icons/hicolor/256x256/apps"

cp -a "$portable_root/." "$deb_root/opt/quintum/"
ln -s /opt/quintum/run-quintum.sh "$deb_root/usr/bin/quintum"
install -m 0644     "$repo_root/packaging/linux/quintum.desktop"     "$deb_root/usr/share/applications/quintum.desktop"
install -m 0644     "$repo_root/src/qt/assets/quintum_icon.png"     "$deb_root/usr/share/icons/hicolor/256x256/apps/quintum.png"

deb_version="${version/-/~}"
cat > "$deb_root/DEBIAN/control" <<EOF
Package: quintum-core
Version: ${deb_version}
Section: net
Priority: optional
Architecture: amd64
Maintainer: QUINTUM Project
Description: QUINTUM Core RandomX Testnet desktop wallet and node
Depends: libc6, libstdc++6, libgcc-s1, libgl1, libfontconfig1, libxkbcommon0, libxkbcommon-x11-0, libxcb1, libxcb-cursor0, libxcb-xinerama0, libxcb-xkb1
EOF

deb="$output/QUINTUM-Core-${version}-linux-x64.deb"
dpkg-deb --build --root-owner-group "$deb_root" "$deb"

rm -rf "$deb_root"

printf '%s\n' "$tarball" "$deb"
