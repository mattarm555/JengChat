#pragma once

#include "raylib.h"

#include <string>

void LoadCardAssets();
void UnloadCardAssets();

// Draw a face-up card using assets/cards/<code>.png.
// Example codes: AS, 10H, QC. If the PNG is missing, a clean fallback card is drawn.
void DrawPlayingCard(const std::string& code, Rectangle destination);

// Draw assets/cards/back.png, or a fallback card back if the PNG is missing.
void DrawCardBack(Rectangle destination);
