#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-package"

if [[ "$(. /etc/os-release && printf '%s' "${VERSION_ID}")" != "24.04" ]]; then
  echo "ERROR: Official .deb packaging is supported only on Ubuntu 24.04." >&2
  exit 1
fi

if [[ "$(dpkg --print-architecture)" != "amd64" ]]; then
  echo "ERROR: Official .deb packaging is supported only on amd64." >&2
  exit 1
fi

rm -rf "${BUILD_DIR}"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${BUILD_DIR}"

ctest --test-dir "${BUILD_DIR}" --output-on-failure

(
  cd "${BUILD_DIR}"
  cpack -G DEB
)

find "${BUILD_DIR}" -maxdepth 1 -type f -name 'fcitx5-english-hint_*.deb' -print
