#include "raylib.h"

#include "app_state.h"
#include "config.h"
#include "networking.h"
#include "protocol.h"
#include "theme.h"

#include "games/game_area.h"
#include "games/arena.h"
#include "games/cards/card_renderer.h"

#include "ui/challenge_popup.h"
#include "ui/chat.h"
#include "ui/command_popup.h"
#include "ui/header.h"
#include "ui/help.h"
#include "ui/login.h"
#include "ui/ui_common.h"
#include "win_icon.h"
#include "mac_bundle.h"
#include "ui/appearance.h"

#include <algorithm>



using namespace std;

namespace
{
    bool HasModalOpen(const AppState& app)
{
    return
        app.showHelpMenu ||
        app.commandPopup.open ||
        app.pendingChallenge.active ||
        IsAppearanceSettingsOpen();
}

    void DrawMainApp(AppState& app)
    {
        bool blocked = HasModalOpen(app);

        DrawHeader(app, blocked);

        // Header interaction may have opened Help this frame.
        blocked = HasModalOpen(app);

        Rectangle chatPanel = {
            20,
            85,
            340,
            WINDOW_HEIGHT - 105.0f
        };

        Rectangle gamePanel = {
            375,
            85,
            WINDOW_WIDTH - 395.0f,
            WINDOW_HEIGHT - 105.0f
        };

        DrawChatPanel(app, chatPanel, blocked);
        DrawGameArea(app, gamePanel, blocked);

        // Modal UI is always drawn last so it sits over the entire app.
        if (app.showHelpMenu)
            DrawHelpMenu(app);

        if (app.commandPopup.open)
            DrawCommandPopup(app);

        if (app.pendingChallenge.active)
            DrawPendingChallengePopup(app);

        if (IsAppearanceSettingsOpen())
            DrawAppearanceSettings();
    }
}

int main()
{
    PrepareMacBundleWorkingDirectory();
    LoadAppearanceSettings();
    
    // The standard Windows maximize button works because the window is resizable.
    SetConfigFlags(
        FLAG_MSAA_4X_HINT |
        FLAG_VSYNC_HINT |
        FLAG_WINDOW_RESIZABLE
    );

    InitWindow(
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        "JENG CHAT"
    );

    SetJengTaskbarIcon(
    GetWindowHandle()
);


    // JENG CHAT handles Escape itself.
    SetExitKey(KEY_NULL);
    SetWindowMinSize(900, 520);
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture(
        WINDOW_WIDTH,
        WINDOW_HEIGHT
    );

    LoadCardAssets();

    AppState app;

    while (!WindowShouldClose())
    {
        float scaleX =
            (float)GetScreenWidth() /
            (float)WINDOW_WIDTH;

        float scaleY =
            (float)GetScreenHeight() /
            (float)WINDOW_HEIGHT;

        float scale = min(scaleX, scaleY);

        if (scale <= 0.0f)
            scale = 1.0f;

        float drawWidth = WINDOW_WIDTH * scale;
        float drawHeight = WINDOW_HEIGHT * scale;

        float offsetX =
            (GetScreenWidth() - drawWidth) / 2.0f;

        float offsetY =
            (GetScreenHeight() - drawHeight) / 2.0f;

        SetUITransform(scale, offsetX, offsetY);

        if (
            app.screen == AppScreen::MAIN &&
            NetIsConnected()
        )
        {
            ProcessIncomingMessages(app);
        }

        if (IsKeyPressed(KEY_F11))
            ToggleFullscreen();

        if (IsKeyPressed(KEY_ESCAPE))
        {
            if (
                app.screen == AppScreen::MAIN &&
                app.gameView == GameView::ARENA
            )
            {
                // Keep the application's existing fullscreen convention:
                // ESC exits fullscreen first. A second ESC is then handled
                // by Arena (match -> setup -> JENG CHAT hub).
                if (IsWindowFullscreen())
                    ToggleFullscreen();
                else
                    ArenaHandleEscape(app);
            }
            else if (IsAppearanceSettingsOpen())
            {
                CancelAppearanceSettings();
            }
            else if (app.pendingChallenge.active)
            {
                // Intentionally do nothing.
            }
            else if (app.commandPopup.open)
            {
                app.commandPopup.open = false;
                app.commandPopup.error.clear();
            }
            else if (app.showHelpMenu)
            {
                app.showHelpMenu = false;
            }
            else if (IsWindowFullscreen())
            {
                ToggleFullscreen();
            }
        }


        bool arenaActive =
            app.screen == AppScreen::MAIN &&
            app.gameView == GameView::ARENA;


        // Arena renders into its own 1280x720 target. It deliberately does
        // this OUTSIDE JENG CHAT's normal UI render texture so Raylib never
        // has to nest BeginTextureMode() calls.
        if (arenaActive)
        {
            ArenaUpdateAndRender(
                app
            );
        }
        else
        {
            BeginTextureMode(target);
            ClearBackground(BG);

            if (app.screen == AppScreen::LOGIN)
                DrawLoginScreen(app);
            else
                DrawMainApp(app);

            EndTextureMode();
        }


        BeginDrawing();
        ClearBackground(BG);

        if (arenaActive)
        {
            // Arena takes over the whole JENG CHAT window while active.
            // The same JENG CHAT window, taskbar icon, and F11 fullscreen
            // behavior remain in control.
            ArenaDrawToWindow();
        }
        else
        {
            Rectangle source = {
                0.0f,
                0.0f,
                (float)target.texture.width,
                -(float)target.texture.height
            };

            Rectangle destination = {
                offsetX,
                offsetY,
                drawWidth,
                drawHeight
            };

            DrawTexturePro(
                target.texture,
                source,
                destination,
                Vector2{0.0f, 0.0f},
                0.0f,
                WHITE
            );
        }

        EndDrawing();
    }

    NetDisconnect();

    ArenaShutdown();

    UnloadCardAssets();
    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
