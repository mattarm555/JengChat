#!/usr/bin/env bash

set -e

echo "Building JENG CHAT for Windows..."

echo "Compiling Windows icon resource..."
windres jengchat.rc -O coff -o jengchat_res.o

echo "Compiling JENG CHAT..."
g++ -std=c++17 \
    main.cpp \
    win_icon.cpp \
    mac_bundle.cpp \
    networking.cpp \
    protocol.cpp \
    ui/*.cpp \
    games/*.cpp \
    games/cards/*.cpp \
    jengchat_res.o \
    -I. \
    -o JengChat.exe \
    -lraylib \
    -lopengl32 \
    -lgdi32 \
    -lwinmm \
    -lws2_32 \
    -pthread

echo
echo "================================"
echo " JENG CHAT BUILD SUCCESSFUL"
echo "================================"
echo
echo "Run with:"
echo "  ./JengChat.exe"
