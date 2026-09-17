#!/bin/bash
set -euo pipefail

APP_NAME="JengChat"
APP_BUNDLE="dist/${APP_NAME}.app"
RAYLIB_DIR="vendor/raylib"
RAYLIB_VERSION="6.0"
DEPLOYMENT_TARGET="11.0"

echo "========================================"
echo "   JENG CHAT macOS UNIVERSAL CI build"
echo "========================================"
echo "Host architecture: $(uname -m)"
echo "Target architectures: arm64 + x86_64"
echo "Minimum macOS: ${DEPLOYMENT_TARGET}"
echo

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "ERROR: This script must run on macOS."
    exit 1
fi

for tool in clang++ cmake git lipo codesign; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "ERROR: ${tool} was not found."
        exit 1
    fi
done

if [[ ! -f "main.cpp" ]]; then
    echo "ERROR: Run this from the JENG CHAT project root."
    exit 1
fi

if [[ ! -f "mac_bundle.cpp" || ! -f "mac_bundle.h" ]]; then
    echo "ERROR: mac_bundle.cpp/mac_bundle.h are missing."
    exit 1
fi

if [[ ! -f "macos/Info.plist" ]]; then
    echo "ERROR: macos/Info.plist is missing."
    exit 1
fi

if [[ ! -f "macos/JengChat.icns" ]]; then
    echo "ERROR: macos/JengChat.icns is missing."
    exit 1
fi

echo "Fetching raylib ${RAYLIB_VERSION}..."
rm -rf "${RAYLIB_DIR}"
mkdir -p vendor

git clone \
    --depth 1 \
    --branch "${RAYLIB_VERSION}" \
    https://github.com/raysan5/raylib.git \
    "${RAYLIB_DIR}"

SOURCES=(
    main.cpp
    mac_bundle.cpp
    networking.cpp
    protocol.cpp
    ui/*.cpp
    games/*.cpp
    games/cards/*.cpp
)

build_arch() {
    local arch="$1"
    local raylib_build="build/macos-raylib-${arch}"
    local app_build="build/macos-${arch}"
    local executable="${app_build}/${APP_NAME}"

    echo
    echo "========================================"
    echo "Building raylib for ${arch}..."
    echo "========================================"

    rm -rf "${raylib_build}" "${app_build}"
    mkdir -p "${app_build}"

    cmake \
        -S "${RAYLIB_DIR}" \
        -B "${raylib_build}" \
        -DBUILD_EXAMPLES=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_ARCHITECTURES="${arch}" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET}"

    cmake --build "${raylib_build}" --config Release --parallel

    local raylib_lib
    raylib_lib="$(find "${raylib_build}" -type f -name 'libraylib.a' -print -quit)"

    if [[ -z "${raylib_lib}" || ! -f "${raylib_lib}" ]]; then
        echo "ERROR: Could not find libraylib.a for ${arch}."
        exit 1
    fi

    echo
    echo "Building ${APP_NAME} for ${arch}..."

    clang++ \
        -std=c++17 \
        -O2 \
        -arch "${arch}" \
        -mmacosx-version-min="${DEPLOYMENT_TARGET}" \
        "${SOURCES[@]}" \
        -I. \
        -I"${RAYLIB_DIR}/src" \
        "${raylib_lib}" \
        -framework OpenGL \
        -framework Cocoa \
        -framework IOKit \
        -framework CoreAudio \
        -framework CoreVideo \
        -framework QuartzCore \
        -pthread \
        -o "${executable}"

    echo "Built: ${executable}"
    file "${executable}"
}

build_arch arm64
build_arch x86_64

echo
echo "========================================"
echo "Creating Universal binary..."
echo "========================================"

rm -f "${APP_NAME}"

lipo \
    -create \
    "build/macos-arm64/${APP_NAME}" \
    "build/macos-x86_64/${APP_NAME}" \
    -output "${APP_NAME}"

echo
echo "Universal executable architectures:"
lipo -info "${APP_NAME}"
file "${APP_NAME}"

echo
echo "Creating ${APP_BUNDLE}..."

rm -rf "${APP_BUNDLE}"
mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"

cp "${APP_NAME}" "${APP_BUNDLE}/Contents/MacOS/${APP_NAME}"
chmod +x "${APP_BUNDLE}/Contents/MacOS/${APP_NAME}"

cp macos/Info.plist \
   "${APP_BUNDLE}/Contents/Info.plist"

cp macos/JengChat.icns \
   "${APP_BUNDLE}/Contents/Resources/JengChat.icns"

cp -R assets \
   "${APP_BUNDLE}/Contents/Resources/assets"

echo
echo "Applying ad-hoc code signature..."

codesign \
    --force \
    --deep \
    --sign - \
    "${APP_BUNDLE}"

echo
echo "Verifying code signature..."
codesign --verify --deep --strict --verbose=2 "${APP_BUNDLE}"

echo
echo "Checking executable dependencies..."
otool -L "${APP_BUNDLE}/Contents/MacOS/${APP_NAME}"

echo
echo "Final architecture check:"
lipo -info "${APP_BUNDLE}/Contents/MacOS/${APP_NAME}"

echo
echo "macOS Universal app created:"
echo "  ${APP_BUNDLE}"
