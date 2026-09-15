#include "poker.h"

#include "cards/card_renderer.h"

#include "../networking.h"
#include "../theme.h"
#include "../ui/command_popup.h"
#include "../ui/ui_common.h"

#include <algorithm>
#include <string>

using namespace std;

namespace
{
    const Color FELT = {24, 83, 61, 255};
    const Color FELT_DARK = {16, 58, 44, 255};
    const Color RAIL = {113, 78, 43, 255};

    void DrawPlayerTag(
        const string& name,
        int chips,
        Vector2 position,
        bool isDealer,
        bool isTurn
    )
    {
        string line = name + "   " + to_string(chips) + " chips";

        DrawText(
            line.c_str(),
            (int)position.x,
            (int)position.y,
            15,
            isTurn ? JENG_YELLOW : TEXT_MAIN
        );

        if (isDealer)
        {
            DrawCircle(
                (int)position.x - 14,
                (int)position.y + 8,
                9,
                Color{242, 242, 238, 255}
            );

            DrawText(
                "D",
                (int)position.x - 19,
                (int)position.y + 1,
                13,
                BG
            );
        }
    }

    void DrawCardRow(
        const vector<string>& cards,
        float centerX,
        float y,
        float cardWidth,
        float cardHeight,
        float gap,
        bool backs
    )
    {
        if (cards.empty())
            return;

        float totalWidth =
            cards.size() * cardWidth +
            (cards.size() - 1) * gap;

        float x = centerX - totalWidth / 2.0f;

        for (int i = 0; i < (int)cards.size(); i++)
        {
            Rectangle card = {
                x + i * (cardWidth + gap),
                y,
                cardWidth,
                cardHeight
            };

            if (backs)
                DrawCardBack(card);
            else if (cards[i] != "--" && !cards[i].empty())
                DrawPlayingCard(cards[i], card);
            else
            {
                DrawRectangleRounded(
                    card,
                    0.06f,
                    6,
                    Color{255, 255, 255, 18}
                );

                DrawRectangleRoundedLinesEx(
                    card,
                    0.06f,
                    6,
                    1.0f,
                    Color{255, 255, 255, 45}
                );
            }
        }
    }

    void SendPokerCommand(AppState& app, const string& command)
    {
        if (!NetSendLine(command))
        {
            AddChatLine(
                app.history,
                "[!] " + NetLastError(),
                ERROR_COLOR
            );
        }
    }

    int ToCall(const PokerClientState& poker)
    {
        return max(0, poker.currentBet - poker.yourBet);
    }

    int MinRaiseTo(const PokerClientState& poker)
    {
        int raiseSize = max(poker.lastRaiseSize, poker.bigBlind);
        return poker.currentBet + raiseSize;
    }

    int MaxRaiseTo(const PokerClientState& poker)
    {
        int yourMaximum = poker.yourBet + poker.yourStack;
        int opponentMaximum = poker.opponentBet + poker.opponentStack;
        return min(yourMaximum, opponentMaximum);
    }

    void KeepRaiseTargetValid(PokerClientState& poker)
    {
        if (!poker.handActive || poker.turn.empty())
            return;

        int minimum = MinRaiseTo(poker);
        int maximum = MaxRaiseTo(poker);

        if (maximum <= poker.currentBet)
        {
            poker.raiseTarget = poker.currentBet;
            return;
        }

        // An all-in raise can be smaller than the normal minimum.
        if (maximum < minimum)
            minimum = maximum;

        if (
            poker.raiseTarget < minimum ||
            poker.raiseTarget > maximum
        )
        {
            poker.raiseTarget = minimum;
        }
    }

    void DrawWaitingPokerPanel(
        AppState& app,
        Rectangle bounds,
        bool interactionsBlocked
    )
    {
        Rectangle table = {
            bounds.x + 42,
            bounds.y + 82,
            bounds.width - 84,
            335
        };

        DrawEllipse(
            (int)(table.x + table.width / 2.0f),
            (int)(table.y + table.height / 2.0f),
            table.width / 2.0f,
            table.height / 2.0f,
            RAIL
        );

        Rectangle felt = {
            table.x + 12,
            table.y + 12,
            table.width - 24,
            table.height - 24
        };

        DrawEllipse(
            (int)(felt.x + felt.width / 2.0f),
            (int)(felt.y + felt.height / 2.0f),
            felt.width / 2.0f,
            felt.height / 2.0f,
            FELT
        );

        DrawText(
            "HEADS-UP TEXAS HOLD'EM",
            (int)(felt.x + felt.width / 2.0f - 125),
            (int)felt.y + 62,
            19,
            Color{235, 225, 185, 210}
        );

        vector<string> sample = {"AS", "KS"};
        DrawCardRow(
            sample,
            felt.x + felt.width / 2.0f,
            felt.y + 112,
            74,
            104,
            12,
            false
        );

        Rectangle createButton = {
            bounds.x + 42,
            bounds.y + 448,
            190,
            46
        };

        if (
            !interactionsBlocked &&
            DrawButton(
                createButton,
                "CREATE TABLE",
                JENG_RED,
                Color{255, 80, 80, 255},
                WHITE,
                15
            )
        )
        {
            OpenCommandPrompt(
                app.commandPopup,
                "CREATE POKER TABLE",
                "Choose your table settings first. You can invite a player after the table is created.",
                "/pokercreate",
                {
                    "Starting chips",
                    "Small blind"
                }
            );
        }

        DrawText(
            "Create the table first, then invite your opponent from the lobby.",
            (int)bounds.x + 255,
            (int)bounds.y + 454,
            13,
            TEXT_MUTED
        );

        DrawText(
            "Big blind is automatically 2x the small blind.",
            (int)bounds.x + 255,
            (int)bounds.y + 477,
            13,
            TEXT_MUTED
        );
    }

    void DrawPokerLobbyPanel(
        AppState& app,
        Rectangle bounds,
        bool interactionsBlocked
    )
    {
        PokerClientState& poker = app.poker;

        Rectangle table = {
            bounds.x + 35,
            bounds.y + 66,
            bounds.width - 70,
            bounds.height - 195
        };

        DrawEllipse(
            (int)(table.x + table.width / 2.0f),
            (int)(table.y + table.height / 2.0f),
            table.width / 2.0f,
            table.height / 2.0f,
            RAIL
        );

        Rectangle felt = {
            table.x + 12,
            table.y + 12,
            table.width - 24,
            table.height - 24
        };

        DrawEllipse(
            (int)(felt.x + felt.width / 2.0f),
            (int)(felt.y + felt.height / 2.0f),
            felt.width / 2.0f,
            felt.height / 2.0f,
            FELT
        );

        DrawEllipse(
            (int)(felt.x + felt.width / 2.0f),
            (int)(felt.y + felt.height / 2.0f),
            felt.width / 2.0f - 22,
            felt.height / 2.0f - 22,
            FELT_DARK
        );

        float centerX =
            felt.x +
            felt.width / 2.0f;

        bool isHost =
            poker.hostName ==
            app.username;

        string topPlayer =
            poker.opponent.empty()
                ? "OPEN SEAT"
                : poker.opponent;

        DrawPlayerTag(
            topPlayer,
            poker.opponent.empty()
                ? 0
                : poker.startingChips,
            Vector2{
                centerX - 90.0f,
                felt.y + 58.0f
            },
            false,
            false
        );

        vector<string> cardBacks = {
            "back",
            "back"
        };

        if (!poker.opponent.empty())
        {
            DrawCardRow(
                cardBacks,
                centerX,
                felt.y + 85.0f,
                62,
                88,
                9,
                true
            );
        }

        // Host / local seat.
        float yourCardsY =
            felt.y +
            felt.height -
            98.0f;

        Rectangle localInfo = {
            centerX - 240.0f,
            yourCardsY + 17.0f,
            145.0f,
            54.0f
        };

        DrawRectangleRounded(
            localInfo,
            0.08f,
            8,
            Color{12, 49, 37, 230}
        );

        DrawText(
            app.username.c_str(),
            (int)localInfo.x + 10,
            (int)localInfo.y + 8,
            15,
            JENG_YELLOW
        );

        string localChips =
            to_string(poker.startingChips) +
            " chips";

        DrawText(
            localChips.c_str(),
            (int)localInfo.x + 10,
            (int)localInfo.y + 29,
            13,
            TEXT_MAIN
        );

        DrawCardRow(
            cardBacks,
            centerX,
            yourCardsY,
            62,
            88,
            9,
            true
        );

        string settings =
            "Starting chips " +
            to_string(poker.startingChips) +
            "   Blinds " +
            to_string(poker.smallBlind) +
            "/" +
            to_string(poker.bigBlind);

        int settingsWidth =
            MeasureText(
                settings.c_str(),
                14
            );

        DrawText(
            settings.c_str(),
            (int)(
                centerX -
                settingsWidth / 2.0f
            ),
            (int)(
                felt.y +
                felt.height / 2.0f -
                10.0f
            ),
            14,
            TEXT_MUTED
        );

        DrawText(
            poker.status.c_str(),
            (int)bounds.x + 38,
            (int)(bounds.y + bounds.height - 142),
            13,
            TEXT_MAIN
        );

        Rectangle inviteButton = {
            bounds.x + 35,
            bounds.y + bounds.height - 104,
            150,
            42
        };

        Rectangle startButton = {
            bounds.x + 195,
            bounds.y + bounds.height - 104,
            140,
            42
        };

        Rectangle leaveButton = {
            bounds.x + bounds.width - 118,
            bounds.y + bounds.height - 104,
            88,
            42
        };

        if (
            isHost &&
            poker.opponent.empty() &&
            !interactionsBlocked &&
            DrawButton(
                inviteButton,
                "INVITE PLAYER",
                JENG_RED,
                Color{255,80,80,255},
                WHITE,
                13
            )
        )
        {
            OpenCommandPrompt(
                app.commandPopup,
                "POKER INVITE",
                "Invite a player to this Poker table.",
                "/poker",
                {
                    "Opponent username"
                }
            );
        }

        if (
            isHost &&
            !poker.opponent.empty() &&
            !interactionsBlocked &&
            DrawButton(
                startButton,
                "START MATCH",
                SUCCESS,
                Color{90,235,140,255},
                BG,
                13
            )
        )
        {
            SendPokerCommand(
                app,
                "/pokerstart"
            );
        }

        if (
            !interactionsBlocked &&
            DrawButton(
                leaveButton,
                isHost ? "CLOSE" : "LEAVE",
                PANEL_LIGHT,
                JENG_RED,
                TEXT_MAIN,
                12
            )
        )
        {
            SendPokerCommand(
                app,
                "/resign"
            );
        }
    }

    void DrawPokerActions(
        AppState& app,
        Rectangle bounds,
        bool interactionsBlocked
    )
    {
        PokerClientState& poker = app.poker;
        KeepRaiseTargetValid(poker);

        Rectangle actionArea = {
            bounds.x + 22,
            bounds.y + bounds.height - 105,
            bounds.width - 44,
            78
        };

        DrawRectangleRounded(
            actionArea,
            0.06f,
            8,
            PANEL_ALT
        );

        bool yourTurn =
            poker.handActive &&
            poker.turn == app.username;

        if (!poker.handActive)
        {
            Rectangle nextButton = {
                actionArea.x + 16,
                actionArea.y + 18,
                150,
                42
            };

            if (
                !interactionsBlocked &&
                DrawButton(
                    nextButton,
                    "NEXT HAND",
                    SUCCESS,
                    Color{90, 235, 140, 255},
                    BG,
                    15
                )
            )
            {
                SendPokerCommand(app, "/pokernext");
            }

            DrawText(
                poker.status.c_str(),
                (int)actionArea.x + 188,
                (int)actionArea.y + 29,
                14,
                TEXT_MAIN
            );

            return;
        }

        if (!yourTurn)
        {
            string waiting =
                "Waiting for " +
                (poker.turn.empty() ? string("opponent") : poker.turn) +
                "...";

            DrawText(
                waiting.c_str(),
                (int)actionArea.x + 18,
                (int)actionArea.y + 28,
                17,
                TEXT_MUTED
            );

            return;
        }

        int toCall = ToCall(poker);

        Rectangle foldButton = {
            actionArea.x + 14,
            actionArea.y + 18,
            92,
            42
        };

        Rectangle callButton = {
            actionArea.x + 116,
            actionArea.y + 18,
            118,
            42
        };

        if (
            !interactionsBlocked &&
            DrawButton(
                foldButton,
                "FOLD",
                JENG_RED,
                Color{255, 90, 90, 255},
                WHITE,
                15
            )
        )
        {
            SendPokerCommand(app, "/pokerfold");
        }

        string callLabel =
            toCall == 0
            ? "CHECK"
            : "CALL " + to_string(toCall);

        if (
            !interactionsBlocked &&
            DrawButton(
                callButton,
                callLabel.c_str(),
                PANEL_LIGHT,
                JENG_YELLOW,
                TEXT_MAIN,
                14
            )
        )
        {
            SendPokerCommand(
                app,
                toCall == 0
                    ? "/pokercheck"
                    : "/pokercall"
            );
        }

        int maximum = MaxRaiseTo(poker);
        bool canRaise = maximum > poker.currentBet;

        Rectangle minusButton = {
            actionArea.x + 248,
            actionArea.y + 18,
            34,
            42
        };

        Rectangle raiseValue = {
            actionArea.x + 288,
            actionArea.y + 18,
            82,
            42
        };

        Rectangle plusButton = {
            actionArea.x + 376,
            actionArea.y + 18,
            34,
            42
        };

        Rectangle raiseButton = {
            actionArea.x + 416,
            actionArea.y + 18,
            100,
            42
        };

        // Keep the raise-step selector on the same familiar values
        // used by Blackjack/Roulette.
        if (
            poker.raiseStep != 10 &&
            poker.raiseStep != 50 &&
            poker.raiseStep != 100
        )
        {
            poker.raiseStep = 10;
        }

        int step = poker.raiseStep;

        if (
            canRaise &&
            !interactionsBlocked &&
            DrawButton(
                minusButton,
                "-",
                PANEL_LIGHT,
                JENG_YELLOW,
                TEXT_MAIN,
                20
            )
        )
        {
            int minimum = MinRaiseTo(poker);

            if (maximum < minimum)
                minimum = maximum;

            poker.raiseTarget = max(
                minimum,
                poker.raiseTarget - step
            );
        }

        DrawRectangleRounded(
            raiseValue,
            0.08f,
            8,
            PANEL_LIGHT
        );

        DrawCenteredText(
            to_string(poker.raiseTarget).c_str(),
            raiseValue,
            16,
            canRaise ? TEXT_MAIN : TEXT_MUTED
        );

        if (
            canRaise &&
            !interactionsBlocked &&
            DrawButton(
                plusButton,
                "+",
                PANEL_LIGHT,
                JENG_YELLOW,
                TEXT_MAIN,
                20
            )
        )
        {
            poker.raiseTarget = min(
                maximum,
                poker.raiseTarget + step
            );
        }

        if (
            canRaise &&
            !interactionsBlocked &&
            DrawButton(
                raiseButton,
                "RAISE TO",
                SUCCESS,
                Color{90, 235, 140, 255},
                BG,
                14
            )
        )
        {
            SendPokerCommand(
                app,
                "/pokerraise " +
                to_string(poker.raiseTarget)
            );
        }


        // ----------------------------------------------------
        // RAISE CHIP STEP
        // ----------------------------------------------------
        // Small chip buttons sit beneath the raise controls so they
        // do not take space away from Fold / Check / Call.
        // ----------------------------------------------------

        float stepRight =
            actionArea.x +
            actionArea.width -
            14.0f;

        Rectangle step100Button = {
            stepRight - 50.0f,
            actionArea.y + 25,
            50,
            30
        };

        Rectangle step50Button = {
            step100Button.x - 48.0f,
            actionArea.y + 25,
            42,
            30
        };

        Rectangle step10Button = {
            step50Button.x - 48.0f,
            actionArea.y + 25,
            42,
            30
        };

        DrawText(
            "STEP",
            (int)step10Button.x,
            (int)actionArea.y + 8,
            10,
            TEXT_MUTED
        );

        auto drawRaiseStepButton =
            [&](Rectangle rect, const char* label, int amount)
            {
                bool selected =
                    poker.raiseStep == amount;

                if (
                    canRaise &&
                    !interactionsBlocked &&
                    DrawButton(
                        rect,
                        label,
                        selected
                            ? JENG_YELLOW
                            : PANEL_LIGHT,
                        selected
                            ? Color{255, 225, 90, 255}
                            : JENG_RED,
                        selected
                            ? BG
                            : TEXT_MAIN,
                        11
                    )
                )
                {
                    poker.raiseStep = amount;
                }
            };

        drawRaiseStepButton(
            step10Button,
            "10",
            10
        );

        drawRaiseStepButton(
            step50Button,
            "50",
            50
        );

        drawRaiseStepButton(
            step100Button,
            "100",
            100
        );
    }
}

void DrawPokerPanel(AppState& app, Rectangle bounds, bool interactionsBlocked)
{
    DrawText(
        "POKER",
        (int)bounds.x + 22,
        (int)bounds.y + 18,
        26,
        JENG_RED
    );

    DrawText(
        "heads-up Texas Hold'em",
        (int)bounds.x + 112,
        (int)bounds.y + 25,
        14,
        TEXT_MUTED
    );

    Rectangle homeButton = {
        bounds.x + bounds.width - 105,
        bounds.y + 14,
        83,
        34
    };

    if (
        !interactionsBlocked &&
        DrawButton(
            homeButton,
            "HOME",
            PANEL_LIGHT,
            JENG_RED,
            TEXT_MAIN,
            14
        )
    )
    {
        app.gameView = GameView::HOME;
    }

    if (!app.poker.tableActive)
    {
        DrawWaitingPokerPanel(
            app,
            bounds,
            interactionsBlocked
        );
        return;
    }

    PokerClientState& poker = app.poker;

    if (poker.tablePhase == "LOBBY")
    {
        DrawPokerLobbyPanel(
            app,
            bounds,
            interactionsBlocked
        );
        return;
    }

    Rectangle table = {
        bounds.x + 35,
        bounds.y + 66,
        bounds.width - 70,
        bounds.height - 195
    };

    DrawEllipse(
        (int)(table.x + table.width / 2.0f),
        (int)(table.y + table.height / 2.0f),
        table.width / 2.0f,
        table.height / 2.0f,
        RAIL
    );

    Rectangle felt = {
        table.x + 12,
        table.y + 12,
        table.width - 24,
        table.height - 24
    };

    DrawEllipse(
        (int)(felt.x + felt.width / 2.0f),
        (int)(felt.y + felt.height / 2.0f),
        felt.width / 2.0f,
        felt.height / 2.0f,
        FELT
    );

    DrawEllipse(
        (int)(felt.x + felt.width / 2.0f),
        (int)(felt.y + felt.height / 2.0f),
        felt.width / 2.0f - 22,
        felt.height / 2.0f - 22,
        FELT_DARK
    );

    float centerX = felt.x + felt.width / 2.0f;

    DrawPlayerTag(
        poker.opponent.empty() ? "OPPONENT" : poker.opponent,
        poker.opponentStack,
        Vector2{centerX - 95, felt.y + 26},
        poker.dealer == poker.opponent,
        poker.turn == poker.opponent
    );

    vector<string> opponentCards =
        poker.opponentRevealed && poker.opponentCards.size() == 2
        ? poker.opponentCards
        : vector<string>{"back", "back"};

    DrawCardRow(
        opponentCards,
        centerX,
        felt.y + 50,
        62,
        88,
        9,
        !poker.opponentRevealed
    );

    // Compact POT badge centered between the two players.
    // It sits directly above the community-card row so it stays readable
    // without covering either player's chip count.
    Rectangle potBadge = {
        centerX - 46.0f,
        felt.y + 135.0f,
        92.0f,
        42.0f
    };

    DrawRectangleRounded(
        potBadge,
        0.12f,
        8,
        Color{12, 49, 37, 245}
    );

    DrawRectangleRoundedLinesEx(
        potBadge,
        0.12f,
        8,
        1.5f,
        JENG_YELLOW
    );

    DrawText(
        "POT",
        (int)potBadge.x + 10,
        (int)potBadge.y + 6,
        11,
        TEXT_MUTED
    );

    string potText =
        to_string(poker.pot);

    DrawText(
        potText.c_str(),
        (int)potBadge.x + 10,
        (int)potBadge.y + 20,
        17,
        JENG_YELLOW
    );

    vector<string> board = poker.communityCards;

    while (board.size() < 5)
        board.push_back("--");

    DrawCardRow(
        board,
        centerX,
        felt.y + 177,
        54,
        76,
        7,
        false
    );

    float localCardsY =
        felt.y +
        felt.height -
        88.0f;

    Rectangle localPlayerBadge = {
        centerX - 245.0f,
        localCardsY + 12.0f,
        145.0f,
        58.0f
    };

    DrawRectangleRounded(
        localPlayerBadge,
        0.08f,
        8,
        Color{12, 49, 37, 238}
    );

    DrawRectangleRoundedLinesEx(
        localPlayerBadge,
        0.08f,
        8,
        1.0f,
        poker.turn == app.username
            ? JENG_YELLOW
            : Color{255,255,255,45}
    );

    string localName =
        "YOU - " +
        app.username;

    DrawText(
        localName.c_str(),
        (int)localPlayerBadge.x + 10,
        (int)localPlayerBadge.y + 8,
        14,
        poker.turn == app.username
            ? JENG_YELLOW
            : TEXT_MAIN
    );

    string localStack =
        to_string(poker.yourStack) +
        " chips";

    DrawText(
        localStack.c_str(),
        (int)localPlayerBadge.x + 10,
        (int)localPlayerBadge.y + 31,
        15,
        JENG_YELLOW
    );

    if (poker.dealer == app.username)
    {
        DrawCircle(
            (int)localPlayerBadge.x - 12,
            (int)localPlayerBadge.y + 18,
            9,
            Color{242,242,238,255}
        );

        DrawText(
            "D",
            (int)localPlayerBadge.x - 17,
            (int)localPlayerBadge.y + 11,
            13,
            BG
        );
    }

    DrawCardRow(
        poker.holeCards,
        centerX,
        localCardsY,
        62,
        88,
        9,
        false
    );

    string stageLine =
        "Hand " + to_string(poker.handNumber) +
        "   " + poker.stage +
        "   Blinds " +
        to_string(poker.smallBlind) +
        "/" +
        to_string(poker.bigBlind);

    DrawText(
        stageLine.c_str(),
        (int)bounds.x + 30,
        (int)(bounds.y + bounds.height - 137),
        13,
        TEXT_MUTED
    );

    string betLine =
        "Your bet: " + to_string(poker.yourBet) +
        "   Opponent bet: " + to_string(poker.opponentBet);

    DrawText(
        betLine.c_str(),
        (int)bounds.x + 335,
        (int)(bounds.y + bounds.height - 137),
        13,
        TEXT_MUTED
    );

    DrawPokerActions(
        app,
        bounds,
        interactionsBlocked
    );

    Rectangle resignButton = {
        bounds.x + bounds.width - 100,
        bounds.y + bounds.height - 40,
        78,
        26
    };

    if (
        !interactionsBlocked &&
        DrawButton(
            resignButton,
            "RESIGN",
            PANEL_LIGHT,
            JENG_RED,
            TEXT_MUTED,
            12
        )
    )
    {
        SendPokerCommand(app, "/resign");
    }
}
