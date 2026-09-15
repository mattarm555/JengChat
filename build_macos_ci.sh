#!/bin/bash
set -euo pipefail

APP_NAME="JengChat"
APP_BUNDLE="dist/${APP_NAME}.app"
RAYLIB_DIR="vendor/raylib"
RAYLIB_VERSION="6.0"

echo "========================================"
echo "        JENG CHAT macOS CI build"
echo "========================================"
echo "Host architecture: $(uname -m)"
echo

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "ERROR: This script must run on macOS."
    exit 1
fi

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

echo
echo "Building static raylib..."
make -C "${RAYLIB_DIR}/src" \
    PLATFORM=PLATFORM_DESKTOP \
    RAYLIB_LIBTYPE=STATIC

echo
echo "Building JENG CHAT..."

clang++ -std=c++17 \
    main.cpp \
    mac_bundle.cpp \
    networking.cpp \
    protocol.cpp \
    ui/*.cpp \
    games/*.cpp \
    games/cards/*.cpp \
    -I. \
    -I"${RAYLIB_DIR}/src" \
    "${RAYLIB_DIR}/src/libraylib.a" \
    -framework OpenGL \
    -framework Cocoa \
    -framework IOKit \
    -framework CoreAudio \
    -framework CoreVideo \
    -framework QuartzCore \
    -pthread \
    -o JengChat

echo
echo "Creating ${APP_BUNDLE}..."

rm -rf "${APP_BUNDLE}"

mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"

cp JengChat "${APP_BUNDLE}/Contents/MacOS/JengChat"
chmod +x "${APP_BUNDLE}/Contents/MacOS/JengChat"

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
echo "Checking executable dependencies..."
otool -L "${APP_BUNDLE}/Contents/MacOS/JengChat"

echo
echo "macOS app created:"
echo "  ${APP_BUNDLE}"
