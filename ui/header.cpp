#include "header.h"
#include "../config.h"
#include "../networking.h"
#include "../theme.h"
#include "ui_common.h"
#include "audio_settings.h"
#include "appearance.h"
#include "players.h"

void DrawHeader(AppState& app, bool blocked)
{
    DrawRectangle(0, 0, WINDOW_WIDTH, 70, PANEL);
    DrawFittedText("JENG CHAT", {24, 21, 175, 30}, 28, JENG_RED);
    if (DrawActionButton({205, 16, 94, 38}, "GAMES", blocked, JENG_YELLOW))
        app.gameView = GameView::HOME;
    if (DrawActionButton({307, 16, 94, 38}, "PLAYERS", blocked, JENG_YELLOW))
        OpenOnlineUsers(app);
    if (DrawActionButton({409, 16, 94, 38}, "STYLE", blocked, JENG_RED))
        OpenAppearanceSettings();
    if (DrawActionButton({511, 16, 94, 38}, "AUDIO", blocked, JENG_YELLOW))
        OpenAudioSettings();
    if (DrawActionButton({613, 16, 94, 38}, "HELP", blocked, JENG_RED))
        app.showHelpMenu = true;
    DrawFittedText("@" + app.username, {730, 26, 260, 20}, 17, TEXT_MUTED);
    Color status = NetIsConnected() ? SUCCESS : ERROR_COLOR;
    DrawCircle(WINDOW_WIDTH - 165, 35, 7, status);
    DrawText(NetIsConnected() ? "CONNECTED" : "OFFLINE", WINDOW_WIDTH - 145, 25, 18, status);
}
