#!/bin/bash
set -euo pipefail

echo "================================"
echo "        JENG CHAT macOS"
echo "================================"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "ERROR: build_macos.sh must be run on macOS."
    exit 1
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "ERROR: clang++ was not found."
    echo "Install Apple's Command Line Tools with:"
    echo "  xcode-select --install"
    exit 1
fi

if ! command -v pkg-config >/dev/null 2>&1; then
    echo "ERROR: pkg-config was not found."
    echo "Install Homebrew, then run:"
    echo "  brew install pkg-config raylib"
    exit 1
fi

if ! pkg-config --exists raylib; then
    echo "ERROR: Raylib was not found by pkg-config."
    echo "Install it with:"
    echo "  brew install raylib"
    exit 1
fi

echo "Building native macOS executable for $(uname -m)..."

clang++ -std=c++17 \
    main.cpp \
    mac_bundle.cpp \
    networking.cpp \
    protocol.cpp \
    ui/*.cpp \
    games/*.cpp \
    games/cards/*.cpp \
    -I. \
    -o JengChat \
    $(pkg-config --cflags --libs raylib) \
    -pthread

echo
echo "Build complete:"
echo "  ./JengChat"
