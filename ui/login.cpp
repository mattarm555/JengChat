#include "login.h"

#include "../config.h"
#include "../networking.h"
#include "../theme.h"
#include "ui_common.h"

using namespace std;

void DrawLoginScreen(AppState& app)
{
    const float panelWidth = 480;
    const float panelHeight = 390;

    Rectangle panel = {
        WINDOW_WIDTH / 2.0f - panelWidth / 2.0f,
        WINDOW_HEIGHT / 2.0f - panelHeight / 2.0f,
        panelWidth,
        panelHeight
    };

    Rectangle usernameBox = {
        panel.x + 55,
        panel.y + 180,
        panel.width - 110,
        52
    };

    Rectangle joinButton = {
        panel.x + 55,
        panel.y + 260,
        panel.width - 110,
        55
    };

    DrawRectangleRounded(panel, 0.04f, 10, PANEL);
    DrawRectangleRoundedLinesEx(panel, 0.04f, 10, 2.0f, JENG_YELLOW);

    const char* title = "JENG CHAT";
    int titleSize = 42;
    int titleWidth = MeasureText(title, titleSize);

    DrawText(
        title,
        WINDOW_WIDTH / 2 - titleWidth / 2,
        (int)panel.y + 42,
        titleSize,
        JENG_RED
    );

    const char* subtitle = "Connect. Chat. Play.";
    int subtitleWidth = MeasureText(subtitle, 18);

    DrawText(
        subtitle,
        WINDOW_WIDTH / 2 - subtitleWidth / 2,
        (int)panel.y + 100,
        18,
        TEXT_MUTED
    );

    DrawText(
        "USERNAME",
        (int)usernameBox.x,
        (int)usernameBox.y - 28,
        18,
        JENG_YELLOW
    );

    DrawRectangleRounded(usernameBox, 0.12f, 8, PANEL_LIGHT);
    DrawRectangleRoundedLinesEx(usernameBox, 0.12f, 8, 2.0f, JENG_YELLOW);

    if (app.username.empty())
    {
        DrawText(
            "Enter username...",
            (int)usernameBox.x + 16,
            (int)usernameBox.y + 15,
            20,
            TEXT_MUTED
        );
    }
    else
    {
        DrawText(
            app.username.c_str(),
            (int)usernameBox.x + 16,
            (int)usernameBox.y + 15,
            20,
            TEXT_MAIN
        );
    }

    if (((int)(GetTime() * 2) % 2) == 0)
    {
        int textWidth = MeasureText(app.username.c_str(), 20);
        DrawRectangle(
            (int)usernameBox.x + 17 + textWidth,
            (int)usernameBox.y + 14,
            2,
            24,
            TEXT_MAIN
        );
    }

    bool hovering = IsMouseInside(joinButton);

    DrawRectangleRounded(
        joinButton,
        0.12f,
        8,
        hovering ? Color{255, 80, 80, 255} : JENG_RED
    );

    DrawCenteredText("JOIN JENG CHAT", joinButton, 20, WHITE);

    DrawText(
        "Private JENG CHAT server",
        (int)panel.x + 55,
        (int)panel.y + 335,
        15,
        TEXT_MUTED
    );

    if (!app.statusMessage.empty())
    {
        DrawText(
            app.statusMessage.c_str(),
            (int)panel.x + 55,
            (int)panel.y + 360,
            15,
            ERROR_COLOR
        );
    }

    int key = GetCharPressed();

    while (key > 0)
    {
        if (
            key >= 32 &&
            key <= 125 &&
            app.username.length() < 16 &&
            IsAllowedUsernameChar((char)key)
        )
        {
            app.username += (char)key;
        }

        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && !app.username.empty())
        app.username.pop_back();

    bool joinClicked = hovering && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool enterPressed = IsKeyPressed(KEY_ENTER);

    if (!(joinClicked || enterPressed))
        return;

    if (app.username.empty())
    {
        app.statusMessage = "Please enter a username.";
        return;
    }

    app.statusMessage = "Connecting...";

    if (NetConnect(SERVER_IP, SERVER_PORT, app.username))
    {
        app.statusMessage.clear();
        app.history.clear();
        AddChatLine(app.history, "Connected to JENG CHAT.", SUCCESS);
        app.screen = AppScreen::MAIN;
        app.gameView = GameView::HOME;
    }
    else
    {
        app.statusMessage = NetLastError();
    }
}
