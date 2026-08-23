#!/usr/bin/env bash
set -euo pipefail

libdir="$(pkg-config --variable=libdir Fcitx5Core 2>/dev/null || echo /usr/lib/x86_64-linux-gnu)"
pkgdatadir="$(pkg-config --variable=pkgdatadir Fcitx5Core 2>/dev/null || echo /usr/share/fcitx5)"

sudo rm -f "${libdir}/fcitx5/libenglish-hint.so"
sudo rm -f "${pkgdatadir}/addon/english-hint.conf"

if command -v fcitx5 >/dev/null 2>&1; then
  fcitx5 -r -d || true
elif command -v fcitx5-remote >/dev/null 2>&1; then
  fcitx5-remote -r || true
fi

echo "Removed fcitx5-english-hint binaries. User config/cache were kept."
