#include "game_area.h"

#include "blackjack.h"
#include "chess.h"
#include "poker.h"
#include "roulette.h"

#include "../theme.h"
#include "../ui/ui_common.h"

namespace
{
    void DrawGameCard(
        AppState& app,
        Rectangle card,
        const char* title,
        const char* subtitle,
        GameView target,
        bool interactionsBlocked,
        Color accent)
    {
        bool hover = !interactionsBlocked && IsMouseInside(card);

        DrawRectangleRounded(
            card,
            0.05f,
            8,
            hover ? PANEL_LIGHT : PANEL_ALT
        );

        DrawRectangleRoundedLinesEx(
            card,
            0.05f,
            8,
            hover ? 2.0f : 1.0f,
            hover ? accent : PANEL_LIGHT
        );

        DrawFittedText(title, {card.x + 18, card.y + 18, card.width - 36, 26}, 24, accent);
        DrawFittedText(subtitle, {card.x + 18, card.y + 54, card.width - 36, 18}, 14, TEXT_MUTED);

        DrawText(
            "OPEN >",
            (int)card.x + 20,
            (int)card.y + (int)card.height - 34,
            14,
            hover ? TEXT_MAIN : TEXT_MUTED
        );

        if (
            hover &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
        )
        {
            app.gameView = target;
        }
    }

    void DrawHomePanel(AppState& app, Rectangle bounds, bool interactionsBlocked)
    {
        DrawText(
            "GAME HUB",
            (int)bounds.x + 22,
            (int)bounds.y + 18,
            26,
            JENG_RED
        );

        DrawText(
            "Pick a game. Chat stays open while you play.",
            (int)bounds.x + 22,
            (int)bounds.y + 53,
            15,
            TEXT_MUTED
        );

        const Rectangle grid = {bounds.x + 22, bounds.y + 88,
            bounds.width - 44, bounds.height - 110};
        struct Entry { const char* title; const char* description; GameView view; Color color; };
        const Entry entries[] = {
            {"CHESS", "Challenge a friend on the board", GameView::CHESS, JENG_YELLOW},
            {"BLACKJACK", "Take a seat at the table", GameView::BLACKJACK, SUCCESS},
            {"POKER", "Heads-up Texas Hold'em", GameView::POKER, Color{130, 180, 255, 255}},
            {"ROULETTE", "Place your chips and spin", GameView::ROULETTE, JENG_RED},
            {"JENG ARENA", "Real-time tank combat", GameView::ARENA, JENG_YELLOW}
        };
        for (int i = 0; i < 5; ++i)
            DrawGameCard(app, UIGridCell(grid, 2, 3, i, 12), entries[i].title,
                entries[i].description, entries[i].view, interactionsBlocked, entries[i].color);

    }
}

void DrawGameArea(AppState& app, Rectangle bounds, bool interactionsBlocked)
{
    DrawRectangleRounded(bounds, 0.02f, 8, PANEL);
    DrawRectangleRoundedLinesEx(bounds, 0.02f, 8, 1.0f, PANEL_LIGHT);

    switch (app.gameView)
    {
        case GameView::HOME:
            DrawHomePanel(app, bounds, interactionsBlocked);
            break;

        case GameView::CHESS:
            DrawChessPanel(app, bounds, interactionsBlocked);
            break;

        case GameView::BLACKJACK:
            DrawBlackjackPanel(app, bounds, interactionsBlocked);
            break;

        case GameView::POKER:
            DrawPokerPanel(app, bounds, interactionsBlocked);
            break;

        case GameView::ROULETTE:
            DrawRoulettePanel(app, bounds, interactionsBlocked);
            break;

        case GameView::ARENA:
            DrawText(
                "Launching JENG ARENA...",
                (int)bounds.x + 24,
                (int)bounds.y + 24,
                22,
                JENG_YELLOW
            );
            break;
    }
}
