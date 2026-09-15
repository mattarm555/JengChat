#include "chat.h"

#include "../networking.h"
#include "../theme.h"
#include "ui_common.h"

#include <algorithm>

using namespace std;

void DrawChatPanel(AppState& app, Rectangle bounds, bool interactionsBlocked)
{
    DrawRectangleRounded(bounds, 0.025f, 8, PANEL);
    DrawRectangleRoundedLinesEx(bounds, 0.025f, 8, 1.0f, PANEL_LIGHT);

    DrawText(
        "CHAT",
        (int)bounds.x + 18,
        (int)bounds.y + 16,
        19,
        JENG_YELLOW
    );

    DrawText(
        "always open",
        (int)bounds.x + 78,
        (int)bounds.y + 19,
        13,
        TEXT_MUTED
    );

    Rectangle messagesArea = {
        bounds.x + 14,
        bounds.y + 47,
        bounds.width - 28,
        bounds.height - 118
    };

    DrawRectangleRounded(messagesArea, 0.02f, 6, PANEL_ALT);

    const int fontSize = 14;
    const int lineHeight = 19;
    const int textWidth = (int)messagesArea.width - 20;

    vector<WrappedLine> lines = BuildWrappedChatLines(
        app.history,
        fontSize,
        textWidth
    );

    int maxVisible = max(
        1,
        ((int)messagesArea.height - 16) / lineHeight
    );

    int maxScroll = max(0, (int)lines.size() - maxVisible);

    if (
        !interactionsBlocked &&
        IsMouseInside(messagesArea)
    )
    {
        float wheel = GetMouseWheelMove();

        if (wheel > 0)
            app.chatScrollOffset += 3;
        else if (wheel < 0)
            app.chatScrollOffset -= 3;
    }

    if (!interactionsBlocked && IsKeyPressed(KEY_PAGE_UP))
        app.chatScrollOffset += max(1, maxVisible - 2);

    if (!interactionsBlocked && IsKeyPressed(KEY_PAGE_DOWN))
        app.chatScrollOffset -= max(1, maxVisible - 2);

    app.chatScrollOffset = clamp(
        app.chatScrollOffset,
        0,
        maxScroll
    );

    int first = max(
        0,
        (int)lines.size() - maxVisible - app.chatScrollOffset
    );

    int last = min(
        (int)lines.size(),
        first + maxVisible
    );

    int y = (int)messagesArea.y + 9;

    for (int i = first; i < last; i++)
    {
        if (!lines[i].text.empty())
        {
            DrawText(
                lines[i].text.c_str(),
                (int)messagesArea.x + 10,
                y,
                fontSize,
                lines[i].color
            );
        }

        y += lineHeight;
    }

    Rectangle inputBox = {
        bounds.x + 14,
        bounds.y + bounds.height - 58,
        bounds.width - 96,
        42
    };

    Rectangle sendButton = {
        bounds.x + bounds.width - 74,
        bounds.y + bounds.height - 58,
        60,
        42
    };

    DrawRectangleRounded(inputBox, 0.10f, 8, PANEL_LIGHT);
    DrawRectangleRoundedLinesEx(inputBox, 0.10f, 8, 1.5f, JENG_YELLOW);

    string visibleInput = app.chatInput;
    const int inputFont = 14;
    const int maxInputPixels = (int)inputBox.width - 20;

    while (
        !visibleInput.empty() &&
        MeasureText(visibleInput.c_str(), inputFont) > maxInputPixels
    )
    {
        visibleInput.erase(visibleInput.begin());
    }

    if (app.chatInput.empty())
    {
        DrawText(
            "Message...",
            (int)inputBox.x + 10,
            (int)inputBox.y + 13,
            inputFont,
            TEXT_MUTED
        );
    }
    else
    {
        DrawText(
            visibleInput.c_str(),
            (int)inputBox.x + 10,
            (int)inputBox.y + 13,
            inputFont,
            TEXT_MAIN
        );
    }

    bool sendHover = !interactionsBlocked && IsMouseInside(sendButton);

    DrawRectangleRounded(
        sendButton,
        0.10f,
        8,
        sendHover ? Color{255, 80, 80, 255} : JENG_RED
    );

    DrawCenteredText("SEND", sendButton, 14, WHITE);

    if (!interactionsBlocked)
    {
        int key = GetCharPressed();

        while (key > 0)
        {
            if (
                key >= 32 &&
                key <= 125 &&
                app.chatInput.length() < 120
            )
            {
                app.chatInput += (char)key;
            }

            key = GetCharPressed();
        }

        if (
            IsKeyPressed(KEY_BACKSPACE) &&
            !app.chatInput.empty()
        )
        {
            app.chatInput.pop_back();
        }
    }

    bool sendRequested =
        !interactionsBlocked &&
        (
            (sendHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ||
            IsKeyPressed(KEY_ENTER)
        );

    if (!sendRequested || app.chatInput.empty())
        return;

    if (app.chatInput == "/help")
    {
        app.chatInput.clear();
        app.showHelpMenu = true;
        return;
    }

    if (NetSendLine(app.chatInput))
    {
        app.chatInput.clear();
        app.chatScrollOffset = 0;
    }
    else
    {
        AddChatLine(
            app.history,
            "[!] " + NetLastError(),
            ERROR_COLOR
        );
    }
}
