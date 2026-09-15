#pragma once

#include "../app_state.h"
#include "raylib.h"

void DrawChessPanel(
    AppState& app,
    Rectangle bounds,
    bool interactionsBlocked
);
