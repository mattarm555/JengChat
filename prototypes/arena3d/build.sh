#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

echo "Building JENG ARENA Phase 1..."

g++ -std=c++17 \
    prototypes/arena3d/arena_demo.cpp \
    -I. \
    -o prototypes/arena3d/JengArenaDemo.exe \
    -lraylib \
    -lopengl32 \
    -lgdi32 \
    -lwinmm

echo
echo "Build complete."
echo "Run: ./prototypes/arena3d/JengArenaDemo.exe"
