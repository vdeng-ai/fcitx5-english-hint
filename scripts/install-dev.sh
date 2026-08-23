#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${BUILD_DIR}"
sudo cmake --install "${BUILD_DIR}"

CONFIG_DIR="${XDG_CONFIG_HOME:-${HOME}/.config}/fcitx5/conf"
CONFIG_FILE="${CONFIG_DIR}/english-hint.conf"
if [[ ! -f "${CONFIG_FILE}" ]]; then
  mkdir -p "${CONFIG_DIR}"
  cp "${ROOT_DIR}/config/english-hint.conf.example" "${CONFIG_FILE}"
  chmod 600 "${CONFIG_FILE}"
  echo "Created default config: ${CONFIG_FILE}"
else
  chmod 600 "${CONFIG_FILE}"
  echo "Keeping existing config: ${CONFIG_FILE}"
fi

# A newly installed addon is only discovered by a fresh Fcitx5 process.
# `fcitx5-remote -r` reloads configuration, but does not rescan/load a new .so.
if command -v fcitx5 >/dev/null 2>&1; then
  fcitx5 -r -d || true
elif command -v fcitx5-remote >/dev/null 2>&1; then
  fcitx5-remote -r || true
fi

echo "Installed fcitx5-english-hint 0.4.1."
echo "Config: ${CONFIG_FILE}"
echo "Type with Rime; translated candidates should appear as: 中文 [English]"
