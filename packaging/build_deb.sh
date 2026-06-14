#!/bin/bash
# Create a .deb: gpc_sequence_recorder + firmware tree under /opt/gpc-recorder/repo,
# host binaries (programmer, USB bridge) under /opt/gpc-recorder/bin.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FW_LLC_ROOT="${FW_LLC_ROOT:-${REPO_ROOT}/../fw-llc}"
OUT_DIR="${REPO_ROOT}/packaging/out"
PKG_NAME="gpc-recorder"

usage() {
    cat <<'EOF'
Usage: packaging/build_deb.sh [OPTIONS]

Build a local .deb for GPC recorder tooling (sequence recorder, USB bridge,
programmer, config firmware build).

Options:
  --fw-llc PATH   Path to fw-llc repo (default: ../fw-llc next to this repo)
  --version VER   Package version (default: from versions.hpp config version)
  -h, --help      Show this help

Requires on build host: rsync, dpkg-deb, cmake, make, g++, python3
Optional: aarch64-linux-gnu-g++ for arm64 USB/programmer binaries

Output: packaging/out/gpc-recorder_<version>_<arch>.deb
EOF
}

read_version_from_header() {
    local header="${REPO_ROOT}/versions.hpp"
    local major minor patch
    major=$(awk '/CONFIG_READ_ONLY_MEMORY_VERSION_MAJOR/{gsub(/U/,"",$3); print $3}' "$header")
    minor=$(awk '/CONFIG_READ_ONLY_MEMORY_VERSION_MINOR/{gsub(/U/,"",$3); print $3}' "$header")
    patch=$(awk '/CONFIG_READ_ONLY_MEMORY_VERSION_PATCH/{gsub(/U/,"",$3); print $3}' "$header")
    echo "${major}.${minor}.${patch}"
}

detect_deb_arch() {
    local machine
    machine="$(uname -m)"
    case "$machine" in
        x86_64|amd64) echo "amd64" ;;
        aarch64|arm64) echo "arm64" ;;
        *)
            echo "Unsupported build architecture: $machine" >&2
            exit 1
            ;;
    esac
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required command: $1" >&2
        exit 1
    fi
}

VERSION=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --fw-llc)
            FW_LLC_ROOT="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "$VERSION" ]]; then
    VERSION="$(read_version_from_header)"
fi

DEB_ARCH="$(detect_deb_arch)"
require_cmd rsync
require_cmd dpkg-deb
require_cmd cmake
require_cmd make
require_cmd g++
require_cmd python3

if [[ ! -d "${FW_LLC_ROOT}/scripts/common" ]]; then
    echo "fw-llc not found at ${FW_LLC_ROOT} (need scripts/common for serial/unix)." >&2
    echo "Set FW_LLC_ROOT or clone fw-llc next to this repository." >&2
    exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

PKG_ROOT="${WORK_DIR}/${PKG_NAME}_${VERSION}_${DEB_ARCH}"
INSTALL_ROOT="${PKG_ROOT}/opt/gpc-recorder/repo"
INSTALL_BIN="${PKG_ROOT}/opt/gpc-recorder/bin"
DEBIAN_DIR="${PKG_ROOT}/DEBIAN"
LLC_COMMON="${FW_LLC_ROOT}/scripts/common"

echo "Building ${PKG_NAME} ${VERSION} for ${DEB_ARCH}..."
echo "  staging: ${INSTALL_ROOT}"

mkdir -p "${INSTALL_ROOT}" "${INSTALL_BIN}" "${DEBIAN_DIR}" "${PKG_ROOT}/usr/bin"

rsync_paths=(
    "build_all.sh"
    "CMakeLists.txt"
    "CMakePresets.json"
    "versions.hpp"
    "cmake"
    "configs"
    "interfaces"
    "config_projects/config_g474"
    "tools/gpc_sequence_recorder"
    "3rd_party/bluelink_sdk/bluelink_messages"
    "3rd_party/bluelink_sdk/bluelink_serializer"
    "3rd_party/bluelink_sdk/bluelink_version"
    "3rd_party/bluelink_sdk/scripts"
)

for rel in "${rsync_paths[@]}"; do
    src="${REPO_ROOT}/${rel}"
    if [[ ! -e "$src" ]]; then
        echo "Missing expected path: $src" >&2
        exit 1
    fi
    dest_parent="$(dirname "${INSTALL_ROOT}/${rel}")"
    mkdir -p "$dest_parent"
    rsync -a \
        --exclude '.venv' \
        --exclude '.pytest_cache' \
        --exclude '__pycache__' \
        --exclude '*.pyc' \
        --exclude 'build' \
        --exclude 'Debug' \
        --exclude 'Release' \
        --exclude '.git' \
        "$src" "$dest_parent/"
done

echo "Building programmer (g474)..."
(
    cd "${REPO_ROOT}/programmer/g474"
    make CXX=g++ LLC_COMMON="${LLC_COMMON}" clean
    make CXX=g++ LLC_COMMON="${LLC_COMMON}" all
    install -m 755 g474_x86_64 "${INSTALL_BIN}/"
    if command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
        make CXX=aarch64-linux-gnu-g++ LLC_COMMON="${LLC_COMMON}" all
        install -m 755 g474_aarch64 "${INSTALL_BIN}/"
    fi
)

echo "Building gpc_usb_bluelink..."
(
    cd "${REPO_ROOT}/tools/gpc_usb_bluelink"
    make CC=g++ LLC_COMMON="${LLC_COMMON}" clean
    make CC=g++ LLC_COMMON="${LLC_COMMON}" all
    install -m 755 gpc_usb_bluelink_x86_64 "${INSTALL_BIN}/"
    if command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
        make CC=aarch64-linux-gnu-g++ LLC_COMMON="${LLC_COMMON}" all
        install -m 755 gpc_usb_bluelink_aarch64 "${INSTALL_BIN}/"
    fi
)

if [[ -f "${REPO_ROOT}/programmer/can_setup.sh" ]]; then
    install -m 755 "${REPO_ROOT}/programmer/can_setup.sh" "${INSTALL_BIN}/"
fi

for wrapper in "${SCRIPT_DIR}/wrappers/"*; do
    [[ "$(basename "$wrapper")" == "seed-exports.sh" ]] && continue
    install -m 755 "$wrapper" "${PKG_ROOT}/usr/bin/$(basename "$wrapper")"
done

if [[ "$DEB_ARCH" == "arm64" ]]; then
    PROG_NAME="g474_aarch64"
    USB_NAME="gpc_usb_bluelink_aarch64"
else
    PROG_NAME="g474_x86_64"
    USB_NAME="gpc_usb_bluelink_x86_64"
fi

if [[ ! -f "${INSTALL_BIN}/${PROG_NAME}" ]]; then
    PROG_NAME="g474_x86_64"
fi
if [[ ! -f "${INSTALL_BIN}/${USB_NAME}" ]]; then
    USB_NAME="gpc_usb_bluelink_x86_64"
fi

ln -sf "/opt/gpc-recorder/bin/${PROG_NAME}" "${PKG_ROOT}/usr/bin/prog-gpc-g4"
ln -sf "/opt/gpc-recorder/bin/${USB_NAME}" "${PKG_ROOT}/usr/bin/gpc-usb-bluelink"

install -m 755 "${SCRIPT_DIR}/wrappers/seed-exports.sh" "${PKG_ROOT}/opt/gpc-recorder/seed-exports.sh"

cat > "${DEBIAN_DIR}/control" <<EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Section: devel
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: BlueWhite Robotics <rnd@bluewhite.ai>
Depends: python3-full, cmake, make, g++, gcc-arm-none-eabi, ca-certificates
Description: GPC sequence recorder and programming tools
 Installs the GPC sequence recorder (web UI + REPL), USB bluelink bridge,
 host programmer for config/app flash, and CMake-based config_g474.bin builds.
 .
 Commands: gpc-recorder, gpc-recorder-repl, gpc-recorder-build-config,
 prog-gpc-g4, gpc-usb-bluelink
 .
 Firmware tree (schema, config CMake project) is under /opt/gpc-recorder/repo.
 Host binaries are under /opt/gpc-recorder/bin (prog-gpc-g4, gpc-usb-bluelink).
 Exports and CMake builds use ~/.cache/gpc-recorder (override with GPC_RECORDER_DATA).
 .
 STM32CubeIDE bundled GNU Tools are auto-detected when newer than apt
 gcc-arm-none-eabi.
EOF

cat > "${DEBIAN_DIR}/postinst" <<'EOF'
#!/bin/sh
set -e

if [ -f /opt/gpc-recorder/seed-exports.sh ]; then
    # shellcheck source=/opt/gpc-recorder/seed-exports.sh
    . /opt/gpc-recorder/seed-exports.sh
    if [ -n "${SUDO_USER:-}" ]; then
        gpc_seed_exports_for_user "$SUDO_USER"
    fi
fi

exit 0
EOF

cat > "${DEBIAN_DIR}/prerm" <<'EOF'
#!/bin/sh
set -e
exit 0
EOF

chmod 755 "${DEBIAN_DIR}/postinst" "${DEBIAN_DIR}/prerm"

mkdir -p "$OUT_DIR"
OUTPUT_DEB="${OUT_DIR}/${PKG_NAME}_${VERSION}_${DEB_ARCH}.deb"
rm -f "$OUTPUT_DEB"
dpkg-deb --build --root-owner-group "$PKG_ROOT" "$OUTPUT_DEB"

echo ""
echo "Created: $OUTPUT_DEB"
echo "Install (pulls apt dependencies): sudo apt install $OUTPUT_DEB"
echo "  Do not use dpkg -i alone — it does not install Depends."
