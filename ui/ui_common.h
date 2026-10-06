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

// Shared bounded text and layout primitives, in virtual UI coordinates.
std::string FitUIText(const std::string& text, int fontSize, int maxWidth);
std::vector<std::string> WrapUIText(const std::string& text, int fontSize, int maxWidth);
Rectangle UIGridCell(Rectangle bounds, int columns, int rows, int index, float gap);
void DrawFittedText(const std::string& text, Rectangle bounds, int fontSize, Color color);
bool DrawActionButton(Rectangle bounds, const char* text, bool blocked, Color accent);

// Wrap by rendered width; cap unusually long messages with an ellipsis.
std::vector<std::string> WrapUIMessage(const std::string& text, int fontSize,
    int maxWidth, int maxLines);
void DrawUILines(const std::vector<std::string>& lines, float x, float y,
    int fontSize, int lineHeight, Color color);
