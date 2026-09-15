#include "help.h"

#include "../config.h"
#include "../networking.h"
#include "../theme.h"

#include "appearance.h"
#include "command_popup.h"
#include "ui_common.h"

using namespace std;

namespace
{
    void DrawMenuCard(
        Rectangle rect,
        const char* title,
        const char* subtitle,
        Color accent,
        bool hover
    )
    {
        DrawRectangleRounded(
            rect,
            0.06f,
            8,
            hover ? PANEL_LIGHT : PANEL_ALT
        );

        DrawRectangleRoundedLinesEx(
            rect,
            0.06f,
            8,
            hover ? 2.0f : 1.0f,
            hover ? accent : PANEL_LIGHT
        );

        DrawText(
            title,
            (int)rect.x + 16,
            (int)rect.y + 13,
            18,
            accent
        );

        DrawText(
            subtitle,
            (int)rect.x + 16,
            (int)rect.y + 38,
            13,
            TEXT_MUTED
        );
    }


    bool CardClicked(
        Rectangle rect,
        bool interactionsBlocked = false
    )
    {
        return
            !interactionsBlocked &&
            IsMouseInside(rect) &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }


    void CloseHelp(AppState& app)
    {
        app.showHelpMenu = false;
    }
}


void DrawHelpMenu(AppState& app)
{
    if (!app.showHelpMenu)
        return;


    // ========================================================
    // BACKDROP
    // ========================================================

    DrawRectangle(
        0,
        0,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        Color{0, 0, 0, 205}
    );


    // ========================================================
    // MAIN PANEL
    // ========================================================

    const float panelWidth = 760.0f;
    const float panelHeight = 520.0f;

    Rectangle panel = {
        WINDOW_WIDTH / 2.0f - panelWidth / 2.0f,
        WINDOW_HEIGHT / 2.0f - panelHeight / 2.0f,
        panelWidth,
        panelHeight
    };

    DrawRectangleRounded(
        panel,
        0.035f,
        10,
        PANEL
    );

    DrawRectangleRoundedLinesEx(
        panel,
        0.035f,
        10,
        2.0f,
        JENG_YELLOW
    );


    DrawText(
        "JENG CHAT",
        (int)panel.x + 28,
        (int)panel.y + 22,
        29,
        JENG_RED
    );

    DrawText(
        "QUICK MENU",
        (int)panel.x + 197,
        (int)panel.y + 28,
        18,
        JENG_YELLOW
    );

    DrawText(
        "Navigate the app here. Game actions now belong inside the game panels.",
        (int)panel.x + 28,
        (int)panel.y + 62,
        15,
        TEXT_MUTED
    );


    // ========================================================
    // CLOSE BUTTON
    // ========================================================

    Rectangle closeButton = {
        panel.x + panel.width - 112,
        panel.y + 20,
        84,
        34
    };

    if (
        DrawButton(
            closeButton,
            "CLOSE",
            PANEL_LIGHT,
            JENG_RED,
            TEXT_MAIN,
            15
        )
    )
    {
        CloseHelp(app);
        return;
    }


    // ========================================================
    // MENU CARDS
    // ========================================================

    const float cardWidth = 330.0f;
    const float cardHeight = 67.0f;
    const float gapX = 18.0f;
    const float gapY = 14.0f;

    const float startX = panel.x + 31.0f;
    const float startY = panel.y + 103.0f;


    Rectangle onlineUsers = {
        startX,
        startY,
        cardWidth,
        cardHeight
    };

    Rectangle gameHub = {
        startX + cardWidth + gapX,
        startY,
        cardWidth,
        cardHeight
    };

    Rectangle chess = {
        startX,
        startY + cardHeight + gapY,
        cardWidth,
        cardHeight
    };

    Rectangle blackjack = {
        startX + cardWidth + gapX,
        startY + cardHeight + gapY,
        cardWidth,
        cardHeight
    };

    Rectangle poker = {
        startX,
        startY + (cardHeight + gapY) * 2,
        cardWidth,
        cardHeight
    };

    Rectangle roulette = {
        startX + cardWidth + gapX,
        startY + (cardHeight + gapY) * 2,
        cardWidth,
        cardHeight
    };

    Rectangle ticTacToe = {
        startX,
        startY + (cardHeight + gapY) * 3,
        cardWidth,
        cardHeight
    };

    Rectangle controlsInfo = {
        startX + cardWidth + gapX,
        startY + (cardHeight + gapY) * 3,
        cardWidth,
        cardHeight
    };


    bool onlineHover = IsMouseInside(onlineUsers);
    bool hubHover = IsMouseInside(gameHub);
    bool chessHover = IsMouseInside(chess);
    bool blackjackHover = IsMouseInside(blackjack);
    bool pokerHover = IsMouseInside(poker);
    bool rouletteHover = IsMouseInside(roulette);
    bool tttHover = IsMouseInside(ticTacToe);
    bool appearanceHover = IsMouseInside(controlsInfo);


    DrawMenuCard(
        onlineUsers,
        "ONLINE USERS",
        "See who is connected",
        SUCCESS,
        onlineHover
    );

    DrawMenuCard(
        gameHub,
        "GAME HUB",
        "Return to game selection",
        JENG_YELLOW,
        hubHover
    );

    DrawMenuCard(
        chess,
        "CHESS",
        "Open the chess workspace",
        JENG_YELLOW,
        chessHover
    );

    DrawMenuCard(
        blackjack,
        "BLACKJACK",
        "Open the blackjack table",
        SUCCESS,
        blackjackHover
    );

    DrawMenuCard(
        poker,
        "POKER",
        "Open the poker table",
        Color{130, 180, 255, 255},
        pokerHover
    );

    DrawMenuCard(
        roulette,
        "ROULETTE",
        "Open the roulette table",
        JENG_RED,
        rouletteHover
    );

    DrawMenuCard(
        ticTacToe,
        "TIC-TAC-TOE",
        "Challenge a player",
        Color{190, 150, 255, 255},
        tttHover
    );


    DrawMenuCard(
        controlsInfo,
        "APPEARANCE",
        "Customize client colors",
        JENG_RED,
        appearanceHover
    );


    // ========================================================
    // ACTIONS
    // ========================================================

    if (CardClicked(onlineUsers))
    {
        if (!NetSendLine("/users"))
        {
            AddChatLine(
                app.history,
                "[!] " + NetLastError(),
                ERROR_COLOR
            );
        }

        CloseHelp(app);
        return;
    }


    if (CardClicked(gameHub))
    {
        app.gameView = GameView::HOME;
        CloseHelp(app);
        return;
    }


    if (CardClicked(chess))
    {
        app.gameView = GameView::CHESS;
        CloseHelp(app);
        return;
    }


    if (CardClicked(blackjack))
    {
        app.gameView = GameView::BLACKJACK;
        CloseHelp(app);
        return;
    }


    if (CardClicked(poker))
    {
        app.gameView = GameView::POKER;
        CloseHelp(app);
        return;
    }


    if (CardClicked(roulette))
    {
        app.gameView = GameView::ROULETTE;
        CloseHelp(app);
        return;
    }


    if (CardClicked(ticTacToe))
    {
        OpenCommandPrompt(
            app.commandPopup,
            "TIC-TAC-TOE",
            "Who do you want to challenge?",
            "/ttt",
            {"Opponent username"}
        );

        CloseHelp(app);
        return;
    }


    if (CardClicked(controlsInfo))
    {
        OpenAppearanceSettings();
        CloseHelp(app);
        return;
    }


    // ========================================================
    // FOOTER
    // ========================================================

    DrawText(
        "In-game controls such as Move, Hit, Stand, Bet and Resign will live inside each game.",
        (int)panel.x + 31,
        (int)panel.y + (int)panel.height - 29,
        13,
        TEXT_MUTED
    );
}
