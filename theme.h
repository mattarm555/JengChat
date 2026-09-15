#pragma once

#include "raylib.h"

#include <algorithm>

// ============================================================
// JENG CHAT RUNTIME THEME
// ============================================================
//
// These are mutable C++17 inline variables on purpose.
// Existing JENG CHAT code can keep using BG, PANEL, TEXT_MAIN,
// JENG_RED, etc. exactly as before while the Appearance menu
// changes them at runtime.
// ============================================================

inline Color BG          = {13, 14, 18, 255};

inline Color PANEL       = {24, 25, 31, 255};
inline Color PANEL_LIGHT = {35, 37, 45, 255};
inline Color PANEL_ALT   = {19, 20, 25, 255};

inline Color JENG_RED    = {235, 64, 64, 255};
inline Color JENG_YELLOW = {245, 205, 66, 255};

inline Color TEXT_MAIN   = {235, 235, 240, 255};
inline Color TEXT_MUTED  = {150, 153, 165, 255};

inline Color SUCCESS     = {70, 210, 120, 255};
inline Color ERROR_COLOR = {255, 90, 90, 255};

// Chat-only colors.
inline Color CHAT_BG     = {19, 20, 25, 255};
inline Color CHAT_TEXT   = {235, 235, 240, 255};
inline Color CHAT_SYSTEM = {245, 205, 66, 255};
inline Color CHAT_GAME   = {70, 210, 120, 255};

// Existing chess colors kept exactly as before.
inline Color BOARD_LIGHT = {208, 210, 218, 255};
inline Color BOARD_DARK  = {93, 99, 116, 255};


inline unsigned char ThemeClampChannel(int value)
{
    return static_cast<unsigned char>(
        std::clamp(value, 0, 255)
    );
}


// PANEL_ALT and PANEL_LIGHT are companion shades of PANEL.
// These offsets reproduce JENG CHAT's original defaults exactly.
inline void RefreshDerivedThemeColors()
{
    PANEL_ALT = Color{
        ThemeClampChannel((int)PANEL.r - 5),
        ThemeClampChannel((int)PANEL.g - 5),
        ThemeClampChannel((int)PANEL.b - 6),
        255
    };

    PANEL_LIGHT = Color{
        ThemeClampChannel((int)PANEL.r + 11),
        ThemeClampChannel((int)PANEL.g + 12),
        ThemeClampChannel((int)PANEL.b + 14),
        255
    };
}


inline void ResetClientTheme()
{
    BG          = Color{13, 14, 18, 255};

    PANEL       = Color{24, 25, 31, 255};
    PANEL_LIGHT = Color{35, 37, 45, 255};
    PANEL_ALT   = Color{19, 20, 25, 255};

    JENG_RED    = Color{235, 64, 64, 255};
    JENG_YELLOW = Color{245, 205, 66, 255};

    TEXT_MAIN   = Color{235, 235, 240, 255};
    TEXT_MUTED  = Color{150, 153, 165, 255};

    SUCCESS     = Color{70, 210, 120, 255};
    ERROR_COLOR = Color{255, 90, 90, 255};

    CHAT_BG     = Color{19, 20, 25, 255};
    CHAT_TEXT   = Color{235, 235, 240, 255};
    CHAT_SYSTEM = Color{245, 205, 66, 255};
    CHAT_GAME   = Color{70, 210, 120, 255};

    BOARD_LIGHT = Color{208, 210, 218, 255};
    BOARD_DARK  = Color{93, 99, 116, 255};
}
