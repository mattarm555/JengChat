#pragma once

// Load saved color settings from the user's computer.
// Safe to call before InitWindow().
void LoadAppearanceSettings();

// Open the color-customization window.
void OpenAppearanceSettings();

// Draw near the end of the virtual 1000x650 canvas pass.
void DrawAppearanceSettings();

// Used by main.cpp to block the normal app while the modal is open.
bool IsAppearanceSettingsOpen();

// ESC / Cancel restores the colors from before the window was opened.
void CancelAppearanceSettings();
