#pragma once

#include "raylib.h"

void LoadChessPieceAssets();
void UnloadChessPieceAssets();

bool ChessPieceAssetsLoaded();

void DrawChessPiece(
    char piece,
    Rectangle square,
    float padding = 4.0f
);
