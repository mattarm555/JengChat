#include "header.h"

#include "../config.h"
#include "../networking.h"
#include "../theme.h"
#include "ui_common.h"
#include "audio_settings.h"

#include <string>

using namespace std;

void DrawHeader(AppState& app, bool interactionsBlocked)
{
    DrawRectangle(0, 0, WINDOW_WIDTH, 70, PANEL);

    DrawText("JENG CHAT", 28, 19, 30, JENG_RED);

    Rectangle helpButton = {205, 15, 90, 40};
    Rectangle audioButton = {305, 15, 90, 40};

    bool helpHover =
        !interactionsBlocked &&
        IsMouseInside(helpButton);

    bool audioHover =
        !interactionsBlocked &&
        IsMouseInside(audioButton);

    DrawRectangleRounded(
        helpButton,
        0.12f,
        8,
        helpHover ? JENG_RED : PANEL_LIGHT
    );

    DrawCenteredText(
        "HELP",
        helpButton,
        17,
        TEXT_MAIN
    );

    DrawRectangleRounded(
        audioButton,
        0.12f,
        8,
        audioHover ? JENG_YELLOW : PANEL_LIGHT
    );

    DrawCenteredText(
        "AUDIO",
        audioButton,
        16,
        audioHover ? BG : TEXT_MAIN
    );

    if (
        helpHover &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
    )
    {
        app.showHelpMenu = true;
    }

    if (
        audioHover &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
    )
    {
        OpenAudioSettings();
    }

    string userText = "@" + app.username;
    DrawText(
        userText.c_str(),
        415,
        25,
        17,
        TEXT_MUTED
    );

    Color statusColor = NetIsConnected() ? SUCCESS : ERROR_COLOR;

    DrawCircle(WINDOW_WIDTH - 165, 35, 7, statusColor);
    DrawText(
        NetIsConnected() ? "CONNECTED" : "OFFLINE",
        WINDOW_WIDTH - 145,
        25,
        18,
        statusColor
    );
}
