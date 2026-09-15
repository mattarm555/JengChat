#include "command_popup.h"

#include "../config.h"
#include "../networking.h"
#include "../theme.h"
#include "ui_common.h"

#include <algorithm>

using namespace std;

void OpenCommandPrompt(
    CommandPromptState& popup,
    const string& title,
    const string& description,
    const string& command,
    const vector<string>& fieldLabels)
{
    popup.open = true;
    popup.title = title;
    popup.description = description;
    popup.command = command;
    popup.fieldCount = min(3, (int)fieldLabels.size());
    popup.activeField = 0;
    popup.error.clear();

    for (int i = 0; i < 3; i++)
    {
        popup.fieldLabels[i] =
            i < popup.fieldCount
                ? fieldLabels[i]
                : "";

        popup.fieldValues[i].clear();
    }
}

void DrawCommandPopup(AppState& app)
{
    CommandPromptState& popup = app.commandPopup;

    if (!popup.open)
        return;

    DrawRectangle(
        0,
        0,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        Color{0, 0, 0, 210}
    );

    const float panelWidth = 600.0f;
    const float panelHeight = 235.0f + popup.fieldCount * 76.0f;

    Rectangle panel = {
        WINDOW_WIDTH / 2.0f - panelWidth / 2.0f,
        WINDOW_HEIGHT / 2.0f - panelHeight / 2.0f,
        panelWidth,
        panelHeight
    };

    DrawRectangleRounded(panel, 0.04f, 10, PANEL);
    DrawRectangleRoundedLinesEx(panel, 0.04f, 10, 2.0f, JENG_YELLOW);

    DrawText(
        popup.title.c_str(),
        (int)panel.x + 28,
        (int)panel.y + 24,
        28,
        JENG_RED
    );

    DrawText(
        popup.description.c_str(),
        (int)panel.x + 28,
        (int)panel.y + 67,
        17,
        TEXT_MUTED
    );

    float fieldStartY = panel.y + 105.0f;

    for (int i = 0; i < popup.fieldCount; i++)
    {
        DrawText(
            popup.fieldLabels[i].c_str(),
            (int)panel.x + 28,
            (int)fieldStartY + i * 76,
            15,
            JENG_YELLOW
        );

        Rectangle fieldBox = {
            panel.x + 28,
            fieldStartY + 22 + i * 76,
            panel.width - 56,
            40
        };

        bool hover = IsMouseInside(fieldBox);

        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            popup.activeField = i;

        DrawRectangleRounded(fieldBox, 0.10f, 8, PANEL_LIGHT);
        DrawRectangleRoundedLinesEx(
            fieldBox,
            0.10f,
            8,
            popup.activeField == i ? 2.0f : 1.0f,
            popup.activeField == i ? JENG_YELLOW : TEXT_MUTED
        );

        const string& value = popup.fieldValues[i];

        DrawText(
            value.empty() ? "Type here..." : value.c_str(),
            (int)fieldBox.x + 12,
            (int)fieldBox.y + 11,
            17,
            value.empty() ? TEXT_MUTED : TEXT_MAIN
        );

        if (
            popup.activeField == i &&
            ((int)(GetTime() * 2) % 2) == 0
        )
        {
            int textWidth = MeasureText(value.c_str(), 17);
            DrawRectangle(
                (int)fieldBox.x + 13 + textWidth,
                (int)fieldBox.y + 8,
                2,
                23,
                TEXT_MAIN
            );
        }
    }

    if (popup.fieldCount > 0)
    {
        int key = GetCharPressed();

        while (key > 0)
        {
            if (
                key >= 32 &&
                key <= 125 &&
                popup.fieldValues[popup.activeField].length() < 32
            )
            {
                popup.fieldValues[popup.activeField] += (char)key;
            }

            key = GetCharPressed();
        }

        if (
            IsKeyPressed(KEY_BACKSPACE) &&
            !popup.fieldValues[popup.activeField].empty()
        )
        {
            popup.fieldValues[popup.activeField].pop_back();
        }

        if (IsKeyPressed(KEY_TAB))
        {
            popup.activeField =
                (popup.activeField + 1) % popup.fieldCount;
        }
    }

    bool readyToSend = true;

    for (int i = 0; i < popup.fieldCount; i++)
    {
        if (popup.fieldValues[i].empty())
            readyToSend = false;
    }

    Rectangle sendButton = {
        panel.x + panel.width - 292,
        panel.y + panel.height - 60,
        120,
        40
    };

    Rectangle cancelButton = {
        panel.x + panel.width - 158,
        panel.y + panel.height - 60,
        120,
        40
    };

    bool sendHover = readyToSend && IsMouseInside(sendButton);
    bool cancelHover = IsMouseInside(cancelButton);

    DrawRectangleRounded(
        sendButton,
        0.10f,
        8,
        readyToSend
            ? (sendHover ? Color{255, 80, 80, 255} : JENG_RED)
            : Color{85, 87, 94, 255}
    );

    DrawCenteredText("SEND", sendButton, 17, WHITE);

    DrawRectangleRounded(
        cancelButton,
        0.10f,
        8,
        cancelHover ? JENG_YELLOW : PANEL_LIGHT
    );

    DrawCenteredText(
        "CANCEL",
        cancelButton,
        17,
        cancelHover ? BG : TEXT_MAIN
    );

    if (!popup.error.empty())
    {
        DrawText(
            popup.error.c_str(),
            (int)panel.x + 28,
            (int)panel.y + (int)panel.height - 48,
            14,
            ERROR_COLOR
        );
    }

    bool sendRequested =
        readyToSend &&
        (
            (sendHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ||
            IsKeyPressed(KEY_ENTER)
        );

    if (sendRequested)
    {
        string commandLine = popup.command;

        for (int i = 0; i < popup.fieldCount; i++)
            commandLine += " " + popup.fieldValues[i];

        if (NetSendLine(commandLine))
        {
            if (popup.command == "/chess")
                app.outboundChallengeGame = GameView::CHESS;
            else if (popup.command == "/blackjack" || popup.command == "/bj")
                app.outboundChallengeGame = GameView::BLACKJACK;
            else if (popup.command == "/poker")
                app.outboundChallengeGame = GameView::POKER;

            popup.open = false;
            popup.error.clear();
        }
        else
        {
            popup.error = NetLastError();

            AddChatLine(
                app.history,
                "[!] " + popup.error,
                ERROR_COLOR
            );
        }
    }

    if (
        cancelHover &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
    )
    {
        popup.open = false;
        popup.error.clear();
    }
}
