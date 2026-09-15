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
    int width = MeasureText(text, fontSize);
    int x = (int)(rect.x + rect.width / 2.0f - width / 2.0f);
    int y = (int)(rect.y + rect.height / 2.0f - fontSize / 2.0f);
    DrawText(text, x, y, fontSize, color);
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
        istringstream words(chat.text);
        string word;
        string current;

        while (words >> word)
        {
            string candidate = current.empty()
                ? word
                : current + " " + word;

            if (
                !current.empty() &&
                MeasureText(candidate.c_str(), fontSize) > maxPixelWidth
            )
            {
                result.push_back({current, chat.color});
                current = word;
            }
            else
            {
                current = candidate;
            }
        }

        if (!current.empty())
            result.push_back({current, chat.color});

        // Small visual separation between chat events.
        result.push_back({"", chat.color});
    }

    if (!result.empty() && result.back().text.empty())
        result.pop_back();

    return result;
}
