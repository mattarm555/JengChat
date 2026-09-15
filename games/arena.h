#pragma once

#include "../app_state.h"

#include <string>

// Arena owns its gameplay state and a 1280x720 render texture, but NOT
// the application window. JENG CHAT remains the only Raylib window.
void ArenaUpdateAndRender(const std::string& username);

// Call during JENG CHAT's normal BeginDrawing()/EndDrawing() pass.
void ArenaDrawToWindow();

// ESC behavior:
//   active match -> Arena setup
//   Arena setup  -> JENG CHAT game hub
void ArenaHandleEscape(AppState& app);

// Release models/render texture before JENG CHAT closes.
void ArenaShutdown();
