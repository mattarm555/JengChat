#!/usr/bin/env bash
set -e

# Run this from an MSYS2 UCRT64 terminal in the project root.
g++ -std=c++17 \
    main.cpp \
    networking.cpp \
    protocol.cpp \
    ui/*.cpp \
    games/*.cpp \
    games/cards/*.cpp \
    -I. \
    -o JengChat.exe \
    -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32 -pthread

echo "Built JengChat.exe"
