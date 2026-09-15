#pragma once

#include "../app_state.h"
#include "raylib.h"

#include <string>
#include <vector>

struct WrappedLine
{
    std::string text;
    Color color;
};

void SetUITransform(float scale, float offsetX, float offsetY);
Vector2 GetUIMousePosition();
bool IsMouseInside(Rectangle rect);

bool IsAllowedUsernameChar(char c);
std::string CurrentTime();
void AddChatLine(std::vector<ChatLine>& history, const std::string& text, Color color);

bool DrawButton(Rectangle rect,
                const char* label,
                Color baseColor,
                Color hoverColor,
                Color textColor,
                int fontSize = 18);

void DrawCenteredText(const char* text, Rectangle rect, int fontSize, Color color);

std::vector<WrappedLine> BuildWrappedChatLines(
    const std::vector<ChatLine>& history,
    int fontSize,
    int maxPixelWidth);
