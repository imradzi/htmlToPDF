#!/usr/bin/env bash
# Build the vendored PlutoBook 0.19.0 for native Linux x64.
#
# Output: htmlToPDF/third_party/prefix/linux-x64/{include,lib}
#
# Requirements (Ubuntu/Debian):
#   sudo apt install libcairo2-dev libexpat1-dev libicu-dev libfreetype6-dev \
#                    libfontconfig1-dev libharfbuzz-dev libcurl4-openssl-dev \
#                    libturbojpeg0-dev libwebp-dev ninja-build pkg-config
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HTML2PDF_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PLUTOBOOK_SRC="${HTML2PDF_DIR}/third_party/plutobook"
PREFIX="${HTML2PDF_DIR}/third_party/prefix/linux-x64"
BUILD_DIR="${PLUTOBOOK_SRC}/build-linux"
TOOLS_VENV="${PLUTOBOOK_TOOLS_VENV:-$HOME/.cache/plutobook-tools/venv}"

if [ ! -f "${PLUTOBOOK_SRC}/meson.build" ]; then
    echo "ERROR: PlutoBook sources not found at ${PLUTOBOOK_SRC}" >&2
    exit 1
fi

# Meson is not packaged with the app; use a private venv when it is missing.
if ! command -v meson >/dev/null 2>&1; then
    if [ ! -x "${TOOLS_VENV}/bin/meson" ]; then
        echo "=== Installing Meson into ${TOOLS_VENV} ==="
        python3 -m venv "${TOOLS_VENV}"
        "${TOOLS_VENV}/bin/pip" install -q meson
    fi
    export PATH="${TOOLS_VENV}/bin:${PATH}"
fi

MESON_ARGS=(
    --prefix="${PREFIX}"
    --buildtype=release
    --default-library=static
    -Dtools=disabled
)

if [ -f "${BUILD_DIR}/meson-info/intro-buildoptions.json" ]; then
    meson setup --reconfigure "${BUILD_DIR}" "${PLUTOBOOK_SRC}" "${MESON_ARGS[@]}"
else
    meson setup "${BUILD_DIR}" "${PLUTOBOOK_SRC}" "${MESON_ARGS[@]}"
fi

meson compile -C "${BUILD_DIR}"
meson install -C "${BUILD_DIR}"

echo ""
echo "=== PlutoBook installed ==="
echo "  headers: ${PREFIX}/include/plutobook"
echo "  library: ${PREFIX}/lib (pkg-config: plutobook)"
