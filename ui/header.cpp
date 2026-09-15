#include "header.h"

#include "../config.h"
#include "../networking.h"
#include "../theme.h"
#include "ui_common.h"

#include <string>

using namespace std;

void DrawHeader(AppState& app, bool interactionsBlocked)
{
    DrawRectangle(0, 0, WINDOW_WIDTH, 70, PANEL);

    DrawText("JENG CHAT", 28, 19, 30, JENG_RED);

    Rectangle helpButton = {205, 15, 90, 40};
    bool helpHover = !interactionsBlocked && IsMouseInside(helpButton);

    DrawRectangleRounded(
        helpButton,
        0.12f,
        8,
        helpHover ? JENG_RED : PANEL_LIGHT
    );

    DrawCenteredText("HELP", helpButton, 17, TEXT_MAIN);

    if (
        helpHover &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
    )
    {
        app.showHelpMenu = true;
    }

    string userText = "@" + app.username;
    DrawText(userText.c_str(), 315, 25, 17, TEXT_MUTED);

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
