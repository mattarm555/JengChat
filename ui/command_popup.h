#pragma once

#include "../app_state.h"

#include <string>
#include <vector>

void OpenCommandPrompt(
    CommandPromptState& popup,
    const std::string& title,
    const std::string& description,
    const std::string& command,
    const std::vector<std::string>& fieldLabels = {}
);

void DrawCommandPopup(AppState& app);
