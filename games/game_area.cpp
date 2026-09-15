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

        DrawText(
            title,
            (int)card.x + 20,
            (int)card.y + 22,
            24,
            accent
        );

        DrawText(
            subtitle,
            (int)card.x + 20,
            (int)card.y + 62,
            14,
            TEXT_MUTED
        );

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

        const float gap = 16.0f;
        const float cardWidth = (bounds.width - 44 - gap) / 2.0f;
        const float cardHeight = 180.0f;
        const float startX = bounds.x + 22;
        const float startY = bounds.y + 92;

        Rectangle chessCard = {
            startX,
            startY,
            cardWidth,
            cardHeight
        };

        Rectangle blackjackCard = {
            startX + cardWidth + gap,
            startY,
            cardWidth,
            cardHeight
        };

        Rectangle pokerCard = {
            startX,
            startY + cardHeight + gap,
            cardWidth,
            cardHeight
        };

        Rectangle rouletteCard = {
            startX + cardWidth + gap,
            startY + cardHeight + gap,
            cardWidth,
            cardHeight
        };

        DrawGameCard(
            app,
            chessCard,
            "CHESS",
            "Mouse-driven board",
            GameView::CHESS,
            interactionsBlocked,
            JENG_YELLOW
        );

        DrawGameCard(
            app,
            blackjackCard,
            "BLACKJACK",
            "PNG cards + dealing",
            GameView::BLACKJACK,
            interactionsBlocked,
            SUCCESS
        );

        DrawGameCard(
            app,
            pokerCard,
            "POKER",
            "Private hands + table",
            GameView::POKER,
            interactionsBlocked,
            Color{130, 180, 255, 255}
        );

        DrawGameCard(
            app,
            rouletteCard,
            "ROULETTE",
            "Animated wheel + chips",
            GameView::ROULETTE,
            interactionsBlocked,
            JENG_RED
        );
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
    }
}
