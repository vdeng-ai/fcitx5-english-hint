#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGE="fcitx5-english-hint"
SUITE="noble"
UBUNTU_VERSION="24.04"
PPA_REVISION="${PPA_REVISION:-1}"
OUTPUT_DIR="${ROOT_DIR}/build-source"
SIGN_KEY=""
SIGN_COMMAND=""
UNSIGNED=false

usage() {
  cat <<'EOF'
Usage: scripts/build-source-package.sh [options]

Build an Ubuntu 24.04 (noble) Debian source package for Launchpad PPA.

Options:
  --unsigned              Build without OpenPGP signatures (local/CI validation).
  --sign-key KEY          OpenPGP key fingerprint/key ID used for signing.
  --sign-command COMMAND  OpenPGP signing command passed to dpkg-buildpackage.
  --ppa-revision N        PPA revision, default: 1.
  --output DIR            Output directory, default: ./build-source.
  -h, --help              Show this help.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --unsigned)
      UNSIGNED=true
      shift
      ;;
    --sign-key)
      SIGN_KEY="${2:?missing value for --sign-key}"
      shift 2
      ;;
    --sign-command)
      SIGN_COMMAND="${2:?missing value for --sign-command}"
      shift 2
      ;;
    --ppa-revision)
      PPA_REVISION="${2:?missing value for --ppa-revision}"
      shift 2
      ;;
    --output)
      OUTPUT_DIR="$(realpath -m "${2:?missing value for --output}")"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "ERROR: Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

for command in dpkg-buildpackage dch tar gzip; do
  if ! command -v "${command}" >/dev/null 2>&1; then
    echo "ERROR: Missing required command: ${command}" >&2
    exit 1
  fi
done

VERSION="$(grep -oP 'project\(fcitx5-english-hint VERSION \K[0-9.]+' "${ROOT_DIR}/CMakeLists.txt" | head -1)"
if [[ -z "${VERSION}" ]]; then
  echo "ERROR: Could not read project version from CMakeLists.txt" >&2
  exit 1
fi

if [[ ! "${PPA_REVISION}" =~ ^[1-9][0-9]*$ ]]; then
  echo "ERROR: --ppa-revision must be a positive integer" >&2
  exit 2
fi

DEBIAN_VERSION="${VERSION}-1~ppa${PPA_REVISION}~ubuntu${UBUNTU_VERSION}.1"
if [[ -z "${SOURCE_DATE_EPOCH:-}" ]]; then
  if command -v git >/dev/null 2>&1 && git -C "${ROOT_DIR}" rev-parse --git-dir >/dev/null 2>&1; then
    SOURCE_DATE_EPOCH="$(git -C "${ROOT_DIR}" log -1 --format=%ct)"
  else
    SOURCE_DATE_EPOCH=0
  fi
fi
export SOURCE_DATE_EPOCH

WORK_ROOT="$(mktemp -d)"
SOURCE_DIR="${WORK_ROOT}/${PACKAGE}-${VERSION}"
ORIG_TARBALL="${WORK_ROOT}/${PACKAGE}_${VERSION}.orig.tar.gz"
trap 'rm -rf "${WORK_ROOT}"' EXIT

mkdir -p "${SOURCE_DIR}" "${OUTPUT_DIR}"
rm -f "${OUTPUT_DIR}"/${PACKAGE}_*.dsc \
      "${OUTPUT_DIR}"/${PACKAGE}_*.changes \
      "${OUTPUT_DIR}"/${PACKAGE}_*.buildinfo \
      "${OUTPUT_DIR}"/${PACKAGE}_*.debian.tar.* \
      "${OUTPUT_DIR}"/${PACKAGE}_*.orig.tar.*

# Copy the working tree, but never package VCS metadata or generated builds.
tar -C "${ROOT_DIR}" \
  --exclude='./.git' \
  --exclude='./build' \
  --exclude='./build-*' \
  --exclude='./build-source' \
  --exclude='./.cache' \
  --exclude='*.deb' \
  -cf - . | tar -C "${SOURCE_DIR}" -xf -

# debian/rules must be executable in a Debian source package.
chmod 0755 "${SOURCE_DIR}/debian/rules"

export DEBFULLNAME="${DEBFULLNAME:-vdeng-ai}"
export DEBEMAIL="${DEBEMAIL:-8124973+vdeng-ai@users.noreply.github.com}"

rm -f "${SOURCE_DIR}/debian/changelog"
(
  cd "${SOURCE_DIR}"
  dch --create \
      --package "${PACKAGE}" \
      --newversion "${DEBIAN_VERSION}" \
      --distribution "${SUITE}" \
      --force-distribution \
      "Release ${VERSION} to the Launchpad PPA for Ubuntu ${UBUNTU_VERSION}."
)

# 3.0 (quilt) requires a pristine upstream tarball beside the source tree.
# Exclude debian/ so dpkg-source can represent packaging as debian.tar.*.
tar -C "${SOURCE_DIR}" \
  --exclude='./debian' \
  --sort=name \
  --mtime="@${SOURCE_DATE_EPOCH}" \
  --owner=0 --group=0 --numeric-owner \
  -cf - . | gzip -n -9 > "${ORIG_TARBALL}"

build_args=(-S -sa)
if [[ "${UNSIGNED}" == true ]]; then
  build_args+=(-us -uc)
else
  if [[ -z "${SIGN_KEY}" ]]; then
    echo "ERROR: Signed build requires --sign-key KEY" >&2
    exit 2
  fi
  build_args+=(--sign-key="${SIGN_KEY}")
  if [[ -n "${SIGN_COMMAND}" ]]; then
    build_args+=(--sign-command="${SIGN_COMMAND}")
  fi
fi

(
  cd "${SOURCE_DIR}"
  dpkg-buildpackage "${build_args[@]}"
)

for artifact in \
  "${WORK_ROOT}/${PACKAGE}_${VERSION}.orig.tar.gz" \
  "${WORK_ROOT}/${PACKAGE}_${DEBIAN_VERSION}.dsc" \
  "${WORK_ROOT}/${PACKAGE}_${DEBIAN_VERSION}.debian.tar."* \
  "${WORK_ROOT}/${PACKAGE}_${DEBIAN_VERSION}_source.changes" \
  "${WORK_ROOT}/${PACKAGE}_${DEBIAN_VERSION}_source.buildinfo"; do
  if [[ -f "${artifact}" ]]; then
    cp -f "${artifact}" "${OUTPUT_DIR}/"
  fi
done

CHANGES="${OUTPUT_DIR}/${PACKAGE}_${DEBIAN_VERSION}_source.changes"
if [[ ! -f "${CHANGES}" ]]; then
  echo "ERROR: Source .changes was not generated" >&2
  exit 1
fi

printf '%s\n' "VERSION=${VERSION}"
printf '%s\n' "DEBIAN_VERSION=${DEBIAN_VERSION}"
printf '%s\n' "SUITE=${SUITE}"
printf '%s\n' "CHANGES=${CHANGES}"
find "${OUTPUT_DIR}" -maxdepth 1 -type f -name "${PACKAGE}_${VERSION}*" -print | sort
