#pragma once

#include "../app_state.h"

#include <string>

// Arena owns its local gameplay state and 1280x720 render texture, but NOT
// the application window. AppState supplies the logged-in username plus the
// server-backed Arena lobby state.
void ArenaUpdateAndRender(AppState& app);

// Call during JENG CHAT's normal BeginDrawing()/EndDrawing() pass.
void ArenaDrawToWindow();

// ESC behavior:
//   active match -> Arena setup
//   Arena setup  -> JENG CHAT game hub
void ArenaHandleEscape(AppState& app);

// Release models/render texture before JENG CHAT closes.
void ArenaShutdown();
