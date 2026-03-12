#!/usr/bin/env bash
# scripts/make-release.sh — build portable release packages for Linux and/or Windows.
#
# Usage:
#   bash scripts/make-release.sh [version] [--linux] [--windows] [--all]
#   bash scripts/make-release.sh 0.1 --all
#   bash scripts/make-release.sh 0.1 --windows
#
# Defaults to --all if no platform flag given.
#
# Output:
#   dist/TheCustodian-<version>-linux-x86_64.tar.gz
#   dist/TheCustodian-<version>-windows-x86_64.zip
#
# Linux requirements:
#   - g++ (C++17)
#   - raylib 5.5 static lib (libraylib.a) in common system paths
#
# Windows requirements (cross-compile from Linux):
#   - x86_64-w64-mingw32-g++  (Fedora: sudo dnf install mingw64-gcc-c++)
#   - raylib 5.5 MinGW package extracted to /opt/raylib-mingw or RAYLIB_MINGW_DIR
#     Download: https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip
set -euo pipefail

VERSION="${1:-0.1}"
BUILD_LINUX=false
BUILD_WINDOWS=false

# Parse platform flags
for arg in "$@"; do
    case "$arg" in
        --linux)   BUILD_LINUX=true ;;
        --windows) BUILD_WINDOWS=true ;;
        --all)     BUILD_LINUX=true; BUILD_WINDOWS=true ;;
    esac
done
# Default to all if no platform specified
if ! $BUILD_LINUX && ! $BUILD_WINDOWS; then
    BUILD_LINUX=true; BUILD_WINDOWS=true
fi

DIST_DIR="dist"
mkdir -p "$DIST_DIR"

# ---------------------------------------------------------------------------
# Linux build
# ---------------------------------------------------------------------------
build_linux() {
    local pkg_name="TheCustodian-${VERSION}-linux-x86_64"
    local pkg_dir="${DIST_DIR}/${pkg_name}"

    echo "[linux] Finding raylib static lib..."
    local raylib_a=""
    for path in \
        /usr/local/lib/libraylib.a \
        /usr/local/lib64/libraylib.a \
        /usr/lib/libraylib.a \
        /usr/lib64/libraylib.a \
        /usr/lib/x86_64-linux-gnu/libraylib.a; do
        [ -f "$path" ] && raylib_a="$path" && break
    done
    [ -z "$raylib_a" ] && {
        echo "ERROR: libraylib.a not found. Install raylib 5.5 (sudo dnf install raylib-devel)."
        return 1
    }
    echo "[linux] Using: ${raylib_a}"

    mkdir -p "$pkg_dir"
    g++ -std=c++17 -O2 -I src/ \
        -o "${pkg_dir}/custodian" \
        src/renderer/main.cpp src/custodian.cpp \
        "${raylib_a}" \
        -lGL -lm -lpthread -ldl -lX11 -lXrandr -lXi -lXinerama -lXcursor

    ldd "${pkg_dir}/custodian" | grep "not found" && {
        echo "ERROR: Linux binary has unresolved dependencies."; return 1
    }

    cp setup.sh "${pkg_dir}/setup.sh"
    chmod +x "${pkg_dir}/custodian" "${pkg_dir}/setup.sh"
    tar -czf "${DIST_DIR}/${pkg_name}.tar.gz" -C "$DIST_DIR" "$pkg_name"
    rm -rf "$pkg_dir"
    echo "[linux] Done: ${DIST_DIR}/${pkg_name}.tar.gz"
}

# ---------------------------------------------------------------------------
# Windows build (cross-compile)
# ---------------------------------------------------------------------------
build_windows() {
    local pkg_name="TheCustodian-${VERSION}-windows-x86_64"
    local pkg_dir="${DIST_DIR}/${pkg_name}"

    # Locate MinGW raylib
    local mingw_dir="${RAYLIB_MINGW_DIR:-}"
    if [ -z "$mingw_dir" ]; then
        for path in \
            /opt/raylib-mingw/raylib-5.5_win64_mingw-w64 \
            /tmp/raylib-win/raylib-5.5_win64_mingw-w64 \
            "${HOME}/raylib-mingw/raylib-5.5_win64_mingw-w64"; do
            [ -f "${path}/lib/libraylib.a" ] && mingw_dir="$path" && break
        done
    fi
    [ -z "$mingw_dir" ] && {
        echo "ERROR: raylib MinGW libs not found."
        echo "  Download: https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip"
        echo "  Extract to /opt/raylib-mingw/ or set RAYLIB_MINGW_DIR"
        return 1
    }
    echo "[windows] Using raylib MinGW: ${mingw_dir}"

    command -v x86_64-w64-mingw32-g++ &>/dev/null || {
        echo "ERROR: x86_64-w64-mingw32-g++ not found. Install: sudo dnf install mingw64-gcc-c++"
        return 1
    }

    mkdir -p "$pkg_dir"
    x86_64-w64-mingw32-g++ -std=c++17 -O2 \
        -I src/ -I "${mingw_dir}/include" \
        -o "${pkg_dir}/custodian.exe" \
        src/renderer/main.cpp src/custodian.cpp \
        "${mingw_dir}/lib/libraylib.a" \
        -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 \
        -static -lm -lpthread

    # Simple launch batch file
    cat > "${pkg_dir}/launch.bat" <<'BAT'
@echo off
start "" "%~dp0custodian.exe"
BAT

    cd "$DIST_DIR" && zip -qr "${pkg_name}.zip" "$pkg_name" && cd - > /dev/null
    rm -rf "$pkg_dir"
    echo "[windows] Done: ${DIST_DIR}/${pkg_name}.zip"
}

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
echo "[make-release] TheCustodian ${VERSION}"
echo ""
$BUILD_LINUX   && build_linux
$BUILD_WINDOWS && build_windows
echo ""
echo "[make-release] Complete. Upload to GitHub Releases:"
ls -lh "${DIST_DIR}/TheCustodian-${VERSION}"* 2>/dev/null || true
