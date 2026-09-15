#!/bin/bash
set -euo pipefail

APP_NAME="JengChat"
APP_BUNDLE="dist/${APP_NAME}.app"

echo "Building JENG CHAT..."
./build_macos.sh

echo "Creating ${APP_BUNDLE}..."

rm -rf "${APP_BUNDLE}"

mkdir -p "${APP_BUNDLE}/Contents/MacOS"
mkdir -p "${APP_BUNDLE}/Contents/Resources"

cp JengChat "${APP_BUNDLE}/Contents/MacOS/JengChat"
chmod +x "${APP_BUNDLE}/Contents/MacOS/JengChat"

cp macos/Info.plist "${APP_BUNDLE}/Contents/Info.plist"
cp macos/JengChat.icns "${APP_BUNDLE}/Contents/Resources/JengChat.icns"

# The app loads assets with relative paths such as assets/cards/*.png.
# mac_bundle.cpp changes the working directory to Contents/Resources.
cp -R assets "${APP_BUNDLE}/Contents/Resources/assets"

# Ad-hoc signing prevents some local bundle-integrity annoyances.
# This is NOT Apple notarization and does not make you an identified developer.
codesign --force --deep --sign - "${APP_BUNDLE}" >/dev/null 2>&1 || true

echo
echo "Created:"
echo "  ${APP_BUNDLE}"
echo
echo "Test it with:"
echo "  open \"${APP_BUNDLE}\""
echo
echo "To make a ZIP for another Mac:"
echo "  ditto -c -k --sequesterRsrc --keepParent \"${APP_BUNDLE}\" \"dist/JengChat_macOS.zip\""
