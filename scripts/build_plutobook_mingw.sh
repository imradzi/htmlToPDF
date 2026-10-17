#!/usr/bin/env bash
# Cross-build PlutoBook 0.19.0 and its dependencies for Windows x64 (MinGW-w64).
#
# ppServer is x64-only (Windows 10+). ppClient is unaffected by this script.
#
# Installs everything into ${MINGW_PREFIX:-/opt/mingw-w64-root} using the same
# toolchain and static-link conventions as install_cross_lib.sh, then installs
# PlutoBook into htmlToPDF/third_party/prefix/win64.
#
# Requirements (Ubuntu host):
#   sudo apt install g++-mingw-w64-x86-64 mingw-w64-tools cmake ninja-build \
#                    meson pkg-config gperf autoconf automake libtool python3-venv
#
# Optional overrides:
#   MINGW_PREFIX=/opt/mingw-w64-root
#   WORKDIR=$HOME/devtools/cross_lib/win64/plutobook
#   JOBS=$(nproc)
#
# NOTE: build order matters. ICU is the slowest step and must have a host build
# for its cross-build tooling.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HTML2PDF_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PLUTOBOOK_SRC="${HTML2PDF_DIR}/third_party/plutobook"

MINGW_PREFIX="${MINGW_PREFIX:-/opt/mingw-w64-root}"
CROSS="x86_64-w64-mingw32"
WORKDIR="${WORKDIR:-$HOME/devtools/cross_lib/win64/plutobook}"
JOBS="${JOBS:-$(nproc)}"
PLUTOBOOK_DEST="${HTML2PDF_DIR}/third_party/prefix/win64"
TOOLS_VENV="${PLUTOBOOK_TOOLS_VENV:-$HOME/.cache/plutobook-tools/venv}"
WINVER="${MINGW_WINVER:-0x0601}"

EXPAT_VER="2.7.1"
LIBPNG_VER="1.6.50"
FREETYPE_VER="2.13.3"
ICU_VER="76.1"
HARFBUZZ_VER="11.2.1"
PIXMAN_VER="0.46.2"
FONTCONFIG_VER="2.15.0"
CAIRO_VER="1.18.4"
JPEGTURBO_VER="3.1.0"

EXPAT_URL="https://github.com/libexpat/libexpat/releases/download/R_2_7_1/expat-${EXPAT_VER}.tar.gz"
LIBPNG_URL="https://github.com/pnggroup/libpng/archive/refs/tags/v${LIBPNG_VER}.tar.gz"
FREETYPE_URL="https://download.savannah.gnu.org/releases/freetype/freetype-${FREETYPE_VER}.tar.xz"
ICU_URL="https://github.com/unicode-org/icu/releases/download/release-76-1/icu4c-76_1-src.tgz"
HARFBUZZ_URL="https://github.com/harfbuzz/harfbuzz/releases/download/${HARFBUZZ_VER}/harfbuzz-${HARFBUZZ_VER}.tar.xz"
PIXMAN_URL="https://cairographics.org/releases/pixman-${PIXMAN_VER}.tar.gz"
FONTCONFIG_URL="https://github.com/fontconfig/fontconfig/archive/refs/tags/${FONTCONFIG_VER}.tar.gz"
CAIRO_URL="https://cairographics.org/releases/cairo-${CAIRO_VER}.tar.xz"
JPEGTURBO_URL="https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${JPEGTURBO_VER}/libjpeg-turbo-${JPEGTURBO_VER}.tar.gz"

for prog in "${CROSS}-gcc" "${CROSS}-g++" "${CROSS}-ar" "${CROSS}-ranlib" cmake ninja; do
    if ! command -v "$prog" >/dev/null 2>&1; then
        echo "ERROR: required tool not found: $prog" >&2
        echo "Install with: sudo apt install g++-mingw-w64-x86-64 mingw-w64-tools cmake ninja-build" >&2
        exit 1
    fi
done
if ! command -v gperf >/dev/null 2>&1; then
    echo "ERROR: gperf is required by fontconfig: sudo apt install gperf" >&2
    exit 1
fi

if ! command -v meson >/dev/null 2>&1; then
    if [ ! -x "${TOOLS_VENV}/bin/meson" ]; then
        echo "=== Installing Meson into ${TOOLS_VENV} ==="
        python3 -m venv "${TOOLS_VENV}"
        "${TOOLS_VENV}/bin/pip" install -q meson
    fi
    export PATH="${TOOLS_VENV}/bin:${PATH}"
fi

mkdir -p "$WORKDIR" "$MINGW_PREFIX" "$PLUTOBOOK_DEST"

# zlib/curl/webp come from install_cross_lib.sh; fail early instead of letting
# Meson pick host libraries.
for lib in libz.a libcurl.a libwebp.a; do
    if [ ! -f "${MINGW_PREFIX}/lib/${lib}" ]; then
        echo "ERROR: ${MINGW_PREFIX}/lib/${lib} not found." >&2
        echo "Run ./install_cross_lib.sh first (it builds zlib, curl and webp)." >&2
        exit 1
    fi
done

# ── Toolchain / pkg-config environment ─────────────────────────
export CC="${CROSS}-gcc"
export CXX="${CROSS}-g++"
export AR="${CROSS}-ar"
export RANLIB="${CROSS}-ranlib"
export STRIP="${CROSS}-strip"
export WIN32_WINNT_FLAG="-D_WIN32_WINNT=${WINVER} -DWINVER=${WINVER}"
export CPPFLAGS="-I${MINGW_PREFIX}/include ${WIN32_WINNT_FLAG}"
export CFLAGS="${WIN32_WINNT_FLAG}"
export CXXFLAGS="${WIN32_WINNT_FLAG}"
export LDFLAGS="-L${MINGW_PREFIX}/lib"
export PKG_CONFIG_PATH="${MINGW_PREFIX}/lib/pkgconfig:${MINGW_PREFIX}/share/pkgconfig"
# Do not let Meson/configure pick up host .pc files.
export PKG_CONFIG_LIBDIR="${MINGW_PREFIX}/lib/pkgconfig:${MINGW_PREFIX}/share/pkgconfig"

CROSS_FILE="${WORKDIR}/mingw-cross.ini"
cat > "${CROSS_FILE}" <<EOF
[binaries]
c = '${CROSS}-gcc'
cpp = '${CROSS}-g++'
ar = '${CROSS}-ar'
strip = '${CROSS}-strip'
ranlib = '${CROSS}-ranlib'
pkg-config = 'pkg-config'

[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
EOF

TOOLCHAIN_FILE="${WORKDIR}/mingw-toolchain.cmake"
cat > "${TOOLCHAIN_FILE}" <<EOF
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER ${CROSS}-gcc)
set(CMAKE_CXX_COMPILER ${CROSS}-g++)
set(CMAKE_RC_COMPILER ${CROSS}-windres)
set(CMAKE_FIND_ROOT_PATH ${MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF

fetch() {
    local url="$1" out="$2"
    if [ ! -f "$out" ]; then
        echo "=== Downloading $(basename "$out") ==="
        curl -sSL --fail -o "$out" "$url"
    fi
}

extract() {
    local archive="$1"
    case "$archive" in
        *.tar.gz|*.tgz) tar xf "$archive" ;;
        *.tar.xz)        tar xf "$archive" ;;
        *) echo "ERROR: unknown archive: $archive" >&2; exit 1 ;;
    esac
}

cross_configure() {
    ./configure --host="${CROSS}" --build="$(gcc -dumpmachine)" \
        --prefix="${MINGW_PREFIX}" --enable-static --disable-shared "$@"
}

# ── 1. expat ───────────────────────────────────────────────────
build_expat() {
    [ -f "${MINGW_PREFIX}/lib/libexpat.a" ] && { echo "expat already installed"; return; }
    echo "=== Building expat ${EXPAT_VER} ==="
    local dir="${WORKDIR}/expat"
    mkdir -p "$dir" && cd "$dir"
    fetch "$EXPAT_URL" "expat-${EXPAT_VER}.tar.gz" && extract "expat-${EXPAT_VER}.tar.gz"
    cmake -S "expat-${EXPAT_VER}" -B build -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DEXPAT_BUILD_TOOLS=OFF -DEXPAT_BUILD_EXAMPLES=OFF \
        -DEXPAT_BUILD_TESTS=OFF -DEXPAT_BUILD_DOCS=OFF
    cmake --build build -j "$JOBS" && cmake --install build
}

# ── 2. libpng ──────────────────────────────────────────────────
build_libpng() {
    [ -f "${MINGW_PREFIX}/lib/libpng16.a" ] && { echo "libpng already installed"; return; }
    echo "=== Building libpng ${LIBPNG_VER} ==="
    local dir="${WORKDIR}/libpng"
    mkdir -p "$dir" && cd "$dir"
    fetch "$LIBPNG_URL" "libpng-${LIBPNG_VER}.tar.gz" && extract "libpng-${LIBPNG_VER}.tar.gz"
    cmake -S "libpng-${LIBPNG_VER}" -B build -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DPNG_TOOLS=OFF
    cmake --build build -j "$JOBS" && cmake --install build
}

# ── 3. freetype (without harfbuzz to break the dependency cycle) ─
build_freetype() {
    [ -f "${MINGW_PREFIX}/lib/libfreetype.a" ] && { echo "freetype already installed"; return; }
    echo "=== Building freetype ${FREETYPE_VER} ==="
    local dir="${WORKDIR}/freetype"
    mkdir -p "$dir" && cd "$dir"
    fetch "$FREETYPE_URL" "freetype-${FREETYPE_VER}.tar.xz" && extract "freetype-${FREETYPE_VER}.tar.xz"
    cd "freetype-${FREETYPE_VER}"
    cross_configure --without-harfbuzz --without-bzip2 --without-brotli
    make -j "$JOBS" && make install
}

# ── 4. ICU (host tools + cross build) ──────────────────────────
build_icu() {
    [ -f "${MINGW_PREFIX}/lib/libicuuc.a" ] && { echo "ICU already installed"; return; }
    echo "=== Building ICU ${ICU_VER} (host tools then cross) ==="
    local dir="${WORKDIR}/icu"
    mkdir -p "$dir" && cd "$dir"
    fetch "$ICU_URL" "icu4c-76_1-src.tgz" && extract "icu4c-76_1-src.tgz"
    local src="$dir/icu/source"
    if [ ! -x "$dir/host/bin/icupkg" ] || [ ! -x "$dir/host/bin/genrb" ]; then
        # The host tools must be built with the *native* compiler. The script
        # exports CC/CXX/CFLAGS/... for the MinGW cross build, so strip them
        # here or configure will try to run cross-compiled .exe files.
        rm -rf "$dir/host"
        mkdir -p "$dir/host"
        cd "$dir/host"
        env -u CC -u CXX -u AR -u RANLIB -u STRIP \
            -u CPP -u CPPFLAGS -u CFLAGS -u CXXFLAGS -u LDFLAGS \
            -u PKG_CONFIG_PATH -u PKG_CONFIG_LIBDIR \
            CC=gcc CXX=g++ AR=ar RANLIB=ranlib \
            "$src/runConfigureICU" Linux --disable-shared --enable-static \
            --prefix="$dir/host-install"
        make -j "$JOBS"
        cd ..
    fi
    mkdir -p cross && cd cross
    "$src/configure" --host="${CROSS}" --build="$(gcc -dumpmachine)" \
        --prefix="${MINGW_PREFIX}" --disable-shared --enable-static \
        --with-cross-build="$dir/host" --with-data-packaging=static
    make -j "$JOBS" && make install
}

# ── 5. harfbuzz ────────────────────────────────────────────────
build_harfbuzz() {
    [ -f "${MINGW_PREFIX}/lib/libharfbuzz.a" ] && { echo "harfbuzz already installed"; return; }
    echo "=== Building harfbuzz ${HARFBUZZ_VER} ==="
    local dir="${WORKDIR}/harfbuzz"
    mkdir -p "$dir" && cd "$dir"
    fetch "$HARFBUZZ_URL" "harfbuzz-${HARFBUZZ_VER}.tar.xz" && extract "harfbuzz-${HARFBUZZ_VER}.tar.xz"
    meson setup build "harfbuzz-${HARFBUZZ_VER}" --cross-file="$CROSS_FILE" \
        --prefix="$MINGW_PREFIX" --buildtype=release --default-library=static \
        -Dglib=disabled -Dgobject=disabled -Dicu=enabled -Dfreetype=enabled \
        -Dcairo=disabled -Dgdi=disabled -Ddirectwrite=disabled \
        -Dintrospection=disabled -Dtests=disabled -Ddocs=disabled \
        -Dutilities=disabled -Dbenchmark=disabled
    meson compile -C build && meson install -C build
}

# ── 6. pixman ──────────────────────────────────────────────────
build_pixman() {
    [ -f "${MINGW_PREFIX}/lib/libpixman-1.a" ] && { echo "pixman already installed"; return; }
    echo "=== Building pixman ${PIXMAN_VER} ==="
    local dir="${WORKDIR}/pixman"
    mkdir -p "$dir" && cd "$dir"
    fetch "$PIXMAN_URL" "pixman-${PIXMAN_VER}.tar.gz" && extract "pixman-${PIXMAN_VER}.tar.gz"
    cd "pixman-${PIXMAN_VER}"
    cross_configure --disable-libpng --disable-gtk
    make -j "$JOBS" && make install
}

# ── 7. fontconfig ──────────────────────────────────────────────
build_fontconfig() {
    [ -f "${MINGW_PREFIX}/lib/libfontconfig.a" ] && { echo "fontconfig already installed"; return; }
    echo "=== Building fontconfig ${FONTCONFIG_VER} ==="
    local dir="${WORKDIR}/fontconfig"
    mkdir -p "$dir" && cd "$dir"
    fetch "$FONTCONFIG_URL" "fontconfig-${FONTCONFIG_VER}.tar.gz" && extract "fontconfig-${FONTCONFIG_VER}.tar.gz"
    meson setup build "fontconfig-${FONTCONFIG_VER}" --cross-file="$CROSS_FILE" \
        --prefix="$MINGW_PREFIX" --buildtype=release --default-library=static \
        -Ddoc=disabled -Dtests=disabled -Dtools=disabled -Dcache-build=disabled
    meson compile -C build && meson install -C build
}

# ── 8. cairo ───────────────────────────────────────────────────
build_cairo() {
    [ -f "${MINGW_PREFIX}/lib/libcairo.a" ] && { echo "cairo already installed"; return; }
    echo "=== Building cairo ${CAIRO_VER} ==="
    local dir="${WORKDIR}/cairo"
    mkdir -p "$dir" && cd "$dir"
    fetch "$CAIRO_URL" "cairo-${CAIRO_VER}.tar.xz" && extract "cairo-${CAIRO_VER}.tar.xz"
    meson setup build "cairo-${CAIRO_VER}" --cross-file="$CROSS_FILE" \
        --prefix="$MINGW_PREFIX" --buildtype=release --default-library=static \
        -Dglib=disabled -Dxlib=disabled -Dxcb=disabled -Dpng=enabled \
        -Dzlib=enabled -Dfreetype=enabled -Dfontconfig=enabled \
        -Dtests=disabled -Dtee=disabled
    meson compile -C build && meson install -C build
}

# ── 9. libjpeg-turbo ───────────────────────────────────────────
build_jpeg() {
    [ -f "${MINGW_PREFIX}/lib/libturbojpeg.a" ] && { echo "libjpeg-turbo already installed"; return; }
    echo "=== Building libjpeg-turbo ${JPEGTURBO_VER} ==="
    local dir="${WORKDIR}/libjpeg-turbo"
    mkdir -p "$dir" && cd "$dir"
    fetch "$JPEGTURBO_URL" "libjpeg-turbo-${JPEGTURBO_VER}.tar.gz" && extract "libjpeg-turbo-${JPEGTURBO_VER}.tar.gz"
    cmake -S "libjpeg-turbo-${JPEGTURBO_VER}" -B build -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$MINGW_PREFIX" \
        -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_SIMD=OFF
    cmake --build build -j "$JOBS" && cmake --install build
}

# ── 10. PlutoBook ──────────────────────────────────────────────
build_plutobook() {
    if [ ! -f "${PLUTOBOOK_SRC}/meson.build" ]; then
        echo "ERROR: PlutoBook sources not found at ${PLUTOBOOK_SRC}" >&2
        exit 1
    fi
    echo "=== Building PlutoBook 0.19.0 ==="
    rm -rf "${WORKDIR}/plutobook-build"
    meson setup "${WORKDIR}/plutobook-build" "$PLUTOBOOK_SRC" \
        --cross-file="$CROSS_FILE" \
        --prefix="$PLUTOBOOK_DEST" \
        --buildtype=release --default-library=static --prefer-static \
        -Dtools=disabled -Dcurl=enabled -Dwebp=enabled -Dturbojpeg=enabled \
        -Dcpp_args="-DCAIRO_WIN32_STATIC_BUILD -DCURL_STATICLIB -DU_STATIC_IMPLEMENTATION" \
        -Dcpp_link_args="-static -static-libgcc -static-libstdc++ -lwinpthread"
    meson compile -C "${WORKDIR}/plutobook-build"
    meson install -C "${WORKDIR}/plutobook-build"

    # Stage the Fontconfig config used at runtime (see PdfGenerator::initLibrary).
    mkdir -p "${PLUTOBOOK_DEST}/fonts"
    cp -f "${HTML2PDF_DIR}/fonts/fonts.conf" "${PLUTOBOOK_DEST}/fonts/fonts.conf"
    for dtd in "${MINGW_PREFIX}/etc/fonts/fonts.dtd" "${MINGW_PREFIX}/share/xml/fontconfig/fonts.dtd"; do
        if [ -f "$dtd" ]; then
            cp -f "$dtd" "${PLUTOBOOK_DEST}/fonts/fonts.dtd"
            break
        fi
    done
}

build_expat
build_libpng
build_freetype
build_icu
build_harfbuzz
build_pixman
build_fontconfig
build_cairo
build_jpeg
build_plutobook

echo ""
echo "=== PlutoBook for Windows x64 installed ==="
echo "  prefix: ${PLUTOBOOK_DEST}"
echo "  fonts:  ${PLUTOBOOK_DEST}/fonts"
