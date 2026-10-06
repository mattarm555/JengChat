#pragma once

#include "../app_state.h"
#include <string>

bool IsAccountPanelOpen();
void OpenAccountPanel();
void CancelAccountPanel();
void DrawAccountPanel(AppState& app);

// Returns true when account deletion signed the user out.
bool HandleAccountPacket(AppState& app, const std::string& type, const std::string& data);
