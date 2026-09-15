#!/usr/bin/env bash

set -euo pipefail

APP_NAME="JengChat"
DIST_ROOT="dist"
PACKAGE_DIR="$DIST_ROOT/JengChat_Windows"
ZIP_PATH="$DIST_ROOT/JengChat_Windows.zip"

echo
echo "========================================"
echo "      JENG CHAT WINDOWS RELEASE"
echo "========================================"
echo

# ------------------------------------------------------------
# BUILD
# ------------------------------------------------------------

echo "[1/5] Building JENG CHAT..."

./build_windows.sh

# ------------------------------------------------------------
# CLEAN OLD PACKAGE
# ------------------------------------------------------------

echo "[2/5] Preparing release folder..."

rm -rf "$PACKAGE_DIR"
rm -f "$ZIP_PATH"

mkdir -p "$PACKAGE_DIR"

# ------------------------------------------------------------
# COPY PROGRAM + ASSETS
# ------------------------------------------------------------

echo "[3/5] Copying application files..."

cp JengChat.exe "$PACKAGE_DIR/"

cp -r assets "$PACKAGE_DIR/"

# ------------------------------------------------------------
# COPY REQUIRED DLL FILES
# ------------------------------------------------------------

echo "[4/5] Copying runtime DLLs..."

# Automatically find MinGW/UCRT64 DLL dependencies.
while IFS= read -r dll
do
    if [ -f "$dll" ]; then
        echo "  -> $(basename "$dll")"
        cp -f "$dll" "$PACKAGE_DIR/"
    fi
done < <(
    ldd JengChat.exe |
    awk '
        /\/ucrt64\/bin\/.*\.dll/ {
            if ($2 == "=>")
                print $3
            else
                print $1
        }
    '
)

# Fallback/common JENG CHAT dependencies.
for dll in \
    libraylib.dll \
    raylib.dll \
    glfw3.dll \
    libgcc_s_seh-1.dll \
    libstdc++-6.dll \
    libwinpthread-1.dll
do
    if [ -f "/ucrt64/bin/$dll" ]; then
        cp -f "/ucrt64/bin/$dll" "$PACKAGE_DIR/"
    fi
done

# ------------------------------------------------------------
# README FOR THE PERSON RECEIVING IT
# ------------------------------------------------------------

cat > "$PACKAGE_DIR/README.txt" <<'EOF'
JENG CHAT
=========

1. Extract the entire JengChat_Windows folder.
2. Keep the assets folder and DLL files next to JengChat.exe.
3. Double-click JengChat.exe.

You do NOT need:
- MSYS2
- GCC / G++
- Raylib
- Visual Studio
- the source code

Windows SmartScreen may show a warning because JENG CHAT is not currently
signed with a commercial Windows code-signing certificate.

If that happens:
More info -> Run anyway
EOF

# ------------------------------------------------------------
# CREATE ZIP
# ------------------------------------------------------------

echo "[5/5] Creating ZIP..."

powershell.exe -NoProfile -Command \
    "Compress-Archive -Force -Path 'dist\\JengChat_Windows' -DestinationPath 'dist\\JengChat_Windows.zip'"

echo
echo "========================================"
echo " RELEASE COMPLETE"
echo "========================================"
echo
echo "Send this file:"
echo
echo "  dist/JengChat_Windows.zip"
echo