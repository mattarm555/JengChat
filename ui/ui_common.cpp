#include "ui_common.h"

#include "../theme.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <sstream>

using namespace std;

namespace
{
    float gUIScale = 1.0f;
    float gUIOffsetX = 0.0f;
    float gUIOffsetY = 0.0f;
}

void SetUITransform(float scale, float offsetX, float offsetY)
{
    gUIScale = (scale > 0.0f) ? scale : 1.0f;
    gUIOffsetX = offsetX;
    gUIOffsetY = offsetY;
}

Vector2 GetUIMousePosition()
{
    Vector2 mouse = GetMousePosition();
    mouse.x = (mouse.x - gUIOffsetX) / gUIScale;
    mouse.y = (mouse.y - gUIOffsetY) / gUIScale;
    return mouse;
}

bool IsMouseInside(Rectangle rect)
{
    return CheckCollisionPointRec(GetUIMousePosition(), rect);
}

bool IsAllowedUsernameChar(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '-';
}

string CurrentTime()
{
    time_t now = time(nullptr);
    tm localTime{};

#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif

    char buffer[16];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &localTime);
    return buffer;
}

void AddChatLine(vector<ChatLine>& history, const string& text, Color color)
{
    history.push_back({text, color});

    if (history.size() > 150)
        history.erase(history.begin());
}

void DrawCenteredText(const char* text, Rectangle rect, int fontSize, Color color)
{
    const int size = std::max(1, std::min(fontSize, (int)rect.height - 4));
    const std::string fitted = FitUIText(text, size, std::max(0, (int)rect.width - 16));
    int width = MeasureText(fitted.c_str(), size);
    int x = (int)(rect.x + (rect.width - width) / 2.0f);
    int y = (int)(rect.y + (rect.height - size) / 2.0f);
    DrawText(fitted.c_str(), x, y, size, color);
}

bool DrawButton(Rectangle rect,
                const char* label,
                Color baseColor,
                Color hoverColor,
                Color textColor,
                int fontSize)
{
    bool hover = IsMouseInside(rect);

    DrawRectangleRounded(
        rect,
        0.10f,
        8,
        hover ? hoverColor : baseColor
    );

    DrawCenteredText(label, rect, fontSize, textColor);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

vector<WrappedLine> BuildWrappedChatLines(
    const vector<ChatLine>& history,
    int fontSize,
    int maxPixelWidth)
{
    vector<WrappedLine> result;

    for (const ChatLine& chat : history)
    {
        for (const std::string& line : WrapUIText(chat.text, fontSize, maxPixelWidth))
            result.push_back({line, chat.color});

        // Small visual separation between chat events.
        result.push_back({"", chat.color});
    }

    if (!result.empty() && result.back().text.empty())
        result.pop_back();

    return result;
}

std::string FitUIText(const std::string& text, int fontSize, int maxWidth)
{
    if (maxWidth <= 0) return "";
    if (MeasureText(text.c_str(), fontSize) <= maxWidth) return text;
    const std::string suffix = "...";
    if (MeasureText(suffix.c_str(), fontSize) > maxWidth) return "";
    std::string result = text;
    while (!result.empty() && MeasureText((result + suffix).c_str(), fontSize) > maxWidth)
    {
        // Remove a complete UTF-8 codepoint.
        size_t pos = result.size() - 1;
        while (pos > 0 && ((unsigned char)result[pos] & 0xc0) == 0x80) --pos;
        result.resize(pos);
    }
    return result + suffix;
}

std::vector<std::string> WrapUIText(const std::string& text, int fontSize, int maxWidth)
{
    std::vector<std::string> lines;
    if (maxWidth <= 0) return lines;
    std::istringstream paragraphs(text);
    std::string paragraph;
    while (std::getline(paragraphs, paragraph))
    {
        std::istringstream words(paragraph);
        std::string word, line;
        while (words >> word)
        {
            const std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && MeasureText(candidate.c_str(), fontSize) > maxWidth)
            {
                lines.push_back(line);
                line.clear();
            }
            if (!line.empty()) line += " ";
            for (size_t pos = 0; pos < word.size();)
            {
                size_t next = pos + 1;
                while (next < word.size() && ((unsigned char)word[next] & 0xc0) == 0x80) ++next;
                std::string glyph = word.substr(pos, next - pos);
                if (!line.empty() && MeasureText((line + glyph).c_str(), fontSize) > maxWidth)
                {
                    lines.push_back(line);
                    line.clear();
                }
                line += glyph;
                pos = next;
            }
        }
        lines.push_back(line);
    }
    return lines;
}

Rectangle UIGridCell(Rectangle bounds, int columns, int rows, int index, float gap)
{
    columns = std::max(1, columns);
    rows = std::max(1, rows);
    float width = std::max(0.0f, (bounds.width - gap * (columns - 1)) / columns);
    float height = std::max(0.0f, (bounds.height - gap * (rows - 1)) / rows);
    return {bounds.x + (index % columns) * (width + gap),
        bounds.y + (index / columns) * (height + gap), width, height};
}

void DrawFittedText(const std::string& text, Rectangle bounds, int fontSize, Color color)
{
    const int size = std::max(1, std::min(fontSize, (int)bounds.height));
    const std::string fitted = FitUIText(text, size, (int)bounds.width);
    DrawText(fitted.c_str(), (int)bounds.x, (int)bounds.y, size, color);
}

bool DrawActionButton(Rectangle bounds, const char* text, bool blocked, Color accent)
{
    bool hover = !blocked && IsMouseInside(bounds);
    DrawRectangleRounded(bounds, 0.12f, 8, hover ? accent : PANEL_LIGHT);
    DrawCenteredText(text, bounds, 16, hover ? BG : TEXT_MAIN);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

std::vector<std::string> WrapUIMessage(const std::string& text, int fontSize,
    int maxWidth, int maxLines)
{
    if (text.empty() || maxLines <= 0) return {};
    auto lines = WrapUIText(text, fontSize, maxWidth);
    if ((int)lines.size() > maxLines)
    {
        lines.resize(maxLines);
        lines.back() = FitUIText(lines.back() + "...", fontSize, maxWidth);
    }
    return lines;
}

void DrawUILines(const std::vector<std::string>& lines, float x, float y,
    int fontSize, int lineHeight, Color color)
{
    for (const auto& line : lines)
    {
        DrawText(line.c_str(), (int)x, (int)y, fontSize, color);
        y += lineHeight;
    }
}
