#include "challenge_popup.h"

#include "../config.h"
#include "../networking.h"
#include "../theme.h"
#include "ui_common.h"

void DrawPendingChallengePopup(AppState& app)
{
    PendingChallengeState& challenge = app.pendingChallenge;

    if (!challenge.active)
        return;

    DrawRectangle(
        0,
        0,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        Color{0, 0, 0, 215}
    );

    const float panelWidth = 620.0f;
    const float panelHeight = 285.0f;

    Rectangle panel = {
        WINDOW_WIDTH / 2.0f - panelWidth / 2.0f,
        WINDOW_HEIGHT / 2.0f - panelHeight / 2.0f,
        panelWidth,
        panelHeight
    };

    DrawRectangleRounded(panel, 0.04f, 10, PANEL);
    DrawRectangleRoundedLinesEx(panel, 0.04f, 10, 2.0f, JENG_YELLOW);

    DrawText(
        challenge.title.c_str(),
        (int)panel.x + 30,
        (int)panel.y + 28,
        29,
        JENG_RED
    );

    DrawText(
        "You received a game challenge.",
        (int)panel.x + 30,
        (int)panel.y + 78,
        18,
        TEXT_MUTED
    );

    DrawText(
        challenge.message.c_str(),
        (int)panel.x + 30,
        (int)panel.y + 118,
        17,
        TEXT_MAIN
    );

    Rectangle acceptButton = {
        panel.x + 70,
        panel.y + 188,
        210,
        52
    };

    Rectangle declineButton = {
        panel.x + panel.width - 280,
        panel.y + 188,
        210,
        52
    };

    bool acceptHover = IsMouseInside(acceptButton);
    bool declineHover = IsMouseInside(declineButton);

    DrawRectangleRounded(
        acceptButton,
        0.10f,
        8,
        acceptHover ? Color{90, 235, 140, 255} : SUCCESS
    );

    DrawCenteredText("ACCEPT", acceptButton, 20, BG);

    DrawRectangleRounded(
        declineButton,
        0.10f,
        8,
        declineHover ? Color{255, 90, 90, 255} : JENG_RED
    );

    DrawCenteredText("DECLINE", declineButton, 20, WHITE);

    if (!challenge.error.empty())
    {
        DrawText(
            challenge.error.c_str(),
            (int)panel.x + 30,
            (int)panel.y + (int)panel.height - 23,
            14,
            ERROR_COLOR
        );
    }

    if (
        acceptHover &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
    )
    {
        if (NetSendLine("/accept"))
        {
            if (challenge.game != GameView::HOME)
                app.gameView = challenge.game;

            challenge.active = false;
            challenge.error.clear();
        }
        else
        {
            challenge.error = NetLastError();
            AddChatLine(
                app.history,
                "[!] " + challenge.error,
                ERROR_COLOR
            );
        }
    }

    if (
        declineHover &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
    )
    {
        if (NetSendLine("/decline"))
        {
            challenge.active = false;
            challenge.error.clear();
        }
        else
        {
            challenge.error = NetLastError();
            AddChatLine(
                app.history,
                "[!] " + challenge.error,
                ERROR_COLOR
            );
        }
    }
}
