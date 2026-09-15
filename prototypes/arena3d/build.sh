#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Building JENG ARENA..."

windres arena_icon.rc -O coff -o arena_icon_res.o

g++ -std=c++17 \
    arena_demo.cpp \
    win_icon.cpp \
    arena_icon_res.o \
    -I. \
    -o JengArenaDemo.exe \
    -lraylib \
    -lopengl32 \
    -lgdi32 \
    -lwinmm \
    -lws2_32 \
    -pthread

rm -f arena_icon_res.o

echo "Built: $SCRIPT_DIR/JengArenaDemo.exe"
