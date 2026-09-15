#!/bin/bash

set -e

echo "Building JENG CHAT..."

windres jengchat.rc -O coff -o jengchat_res.o

g++ -std=c++17 \
    main.cpp \
    win_icon.cpp \
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

echo "Creating distribution..."

rm -rf dist/JengChatV1_Windows
mkdir -p dist/JengChatV1_Windows

cp JengChat.exe dist/JengChatV1_Windows/

cp /ucrt64/bin/libraylib.dll dist/JengChatV1_Windows/
cp /ucrt64/bin/glfw3.dll dist/JengChatV1_Windows/
cp /ucrt64/bin/libgcc_s_seh-1.dll dist/JengChatV1_Windows/
cp /ucrt64/bin/libstdc++-6.dll dist/JengChatV1_Windows/
cp /ucrt64/bin/libwinpthread-1.dll dist/JengChatV1_Windows/

cp -r assets dist/JengChatV1_Windows/

echo "Done!"
echo "Send the dist/JengChatV1_Windows folder."