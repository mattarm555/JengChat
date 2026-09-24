#include "poker.h"

#include "cards/card_renderer.h"

#include "../networking.h"
#include "../theme.h"
#include "../ui/command_popup.h"
#include "../ui/players.h"
#include "../ui/ui_common.h"

#include <algorithm>
#include <string>

using namespace std;

namespace
{
    const Color FELT = {24, 83, 61, 255};
    const Color FELT_DARK = {16, 58, 44, 255};
    const Color RAIL = {113, 78, 43, 255};

    void DrawPositionChip(
        const char* label,
        Vector2 center,
        Color fill,
        Color text
    )
    {
        DrawCircle((int)center.x, (int)center.y, 10, fill);
        int width = MeasureText(label, 10);
        DrawText(
            label,
            (int)(center.x - width / 2.0f),
            (int)center.y - 5,
            10,
            text
        );
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
                DrawRectangleRounded(card, 0.06f, 6, Color{255,255,255,18});
                DrawRectangleRoundedLinesEx(
                    card, 0.06f, 6, 1.0f, Color{255,255,255,45}
                );
            }
        }
    }

    vector<PokerClientPlayer> SeatOrder(
        const PokerClientState& poker
    )
    {
        // The server keeps players in table-seat order and sends that same
        // order to every client. Preserve it so all players see one shared
        // table layout and the dealer/blind chips visibly move each hand.
        return poker.players;
    }

    struct SeatPosition
    {
        Vector2 cards;
        Vector2 badge;
    };

    SeatPosition PokerSeatPosition(Rectangle felt, int relativeSeat)
    {
        float centerX = felt.x + felt.width / 2.0f;
        float bottom = felt.y + felt.height;
        const SeatPosition seats[6] = {
            // Keep the bottom-center cards beside their badge without letting
            // that badge collide with the lower-left seat.
            {{centerX + 40, bottom - 82}, {centerX - 178, bottom - 56}},
            {{felt.x + 100, bottom - 142}, {felt.x + 24, bottom - 66}},
            {{felt.x + 105, felt.y + 68}, {felt.x + 26, felt.y + 22}},
            {{centerX, felt.y + 50}, {centerX - 72, felt.y + 12}},
            {{felt.x + felt.width - 105, felt.y + 68}, {felt.x + felt.width - 168, felt.y + 22}},
            {{felt.x + felt.width - 100, bottom - 142}, {felt.x + felt.width - 168, bottom - 66}}
        };
        return seats[max(0, min(5, relativeSeat))];
    }

    void DrawPokerSeat(
        const PokerClientPlayer& player,
        Rectangle felt,
        int relativeSeat,
        const PokerClientState& poker,
        bool local,
        bool lobby
    )
    {
        SeatPosition position = PokerSeatPosition(felt, relativeSeat);
        Rectangle badge = {position.badge.x, position.badge.y, 144, 46};
        bool isTurn = poker.turn == player.name;

        DrawRectangleRounded(badge, 0.10f, 8, Color{12,49,37,242});
        DrawRectangleRoundedLinesEx(
            badge,
            0.10f,
            8,
            isTurn ? 2.0f : 1.0f,
            isTurn ? JENG_YELLOW : Color{255,255,255,45}
        );

        string name = local ? "YOU - " + player.name : player.name;
        int nameSize = MeasureText(name.c_str(), 13) > 118 ? 11 : 13;
        DrawText(
            name.c_str(),
            (int)badge.x + 8,
            (int)badge.y + 6,
            nameSize,
            isTurn ? JENG_YELLOW : TEXT_MAIN
        );

        string stack = to_string(player.stack) + " chips";
        if (!lobby && player.bet > 0)
            stack += "  Bet " + to_string(player.bet);
        if (!lobby && player.folded)
            stack = "FOLDED";
        else if (!lobby && player.allIn)
            stack += "  ALL IN";

        DrawText(
            stack.c_str(),
            (int)badge.x + 8,
            (int)badge.y + 25,
            11,
            player.folded ? TEXT_MUTED : (local ? JENG_YELLOW : TEXT_MAIN)
        );

        float markerX = badge.x + badge.width - 12;
        float markerY = badge.y + 11;
        if (poker.dealer == player.name)
        {
            DrawPositionChip("D", {markerX, markerY}, Color{242,242,238,255}, BG);
            markerY += 23;
        }
        if (poker.smallBlindPlayer == player.name)
        {
            DrawPositionChip("SB", {markerX, markerY}, JENG_YELLOW, BG);
            markerY += 23;
        }
        if (poker.bigBlindPlayer == player.name)
            DrawPositionChip("BB", {markerX, markerY}, JENG_RED, WHITE);

        if (lobby)
            return;

        vector<string> cards;
        bool backs = false;
        if (local)
            cards = poker.holeCards;
        else if (player.revealed && player.cards.size() == 2)
            cards = player.cards;
        else if (!player.folded)
        {
            cards = {"back", "back"};
            backs = true;
        }

        if (!cards.empty())
        {
            float width = local ? 56.0f : 42.0f;
            float height = local ? 79.0f : 59.0f;
            DrawCardRow(
                cards,
                position.cards.x,
                position.cards.y,
                width,
                height,
                local ? 8.0f : 5.0f,
                backs
            );
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
        return poker.maxRaiseTo;
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
            "2-6 PLAYER TEXAS HOLD'EM",
            (int)(felt.x + felt.width / 2.0f - 128),
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
                "Choose table settings, then invite up to five players.",
                "/pokercreate",
                {
                    "Starting chips",
                    "Small blind"
                }
            );
        }

        DrawText(
            "Create a table, invite up to five players, then start with 2-6.",
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

        vector<PokerClientPlayer> seats = SeatOrder(poker);
        while (seats.size() < 6)
        {
            PokerClientPlayer open;
            open.name = "OPEN SEAT";
            seats.push_back(open);
        }
        for (int i = 0; i < 6; i++)
            DrawPokerSeat(
                seats[i],
                felt,
                i,
                poker,
                seats[i].name == app.username,
                true
            );

        float centerX = felt.x + felt.width / 2.0f;
        string settings =
            to_string(poker.players.size()) + "/6 PLAYERS   Starting chips " +
            to_string(poker.startingChips) + "   Blinds " +
            to_string(poker.smallBlind) + "/" +
            to_string(poker.bigBlind);
        int settingsWidth = MeasureText(settings.c_str(), 14);
        DrawText(
            settings.c_str(),
            (int)(centerX - settingsWidth / 2.0f),
            (int)(felt.y + felt.height / 2.0f - 8),
            14,
            JENG_YELLOW
        );

        DrawText(
            poker.status.c_str(),
            (int)bounds.x + 38,
            (int)(bounds.y + bounds.height - 142),
            13,
            TEXT_MAIN
        );

        bool isHost = poker.hostName == app.username;
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
            poker.players.size() < 6 &&
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
            OpenPlayerInvite(app, GameView::POKER);
        }

        if (
            isHost &&
            poker.players.size() >= 2 &&
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
            SendPokerCommand(app, "/pokerstart");
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
            SendPokerCommand(app, "/resign");
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
            bool isHost = poker.hostName == app.username;
            Rectangle nextButton = {
                actionArea.x + 16,
                actionArea.y + 18,
                135,
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

            bool canInvite =
                isHost &&
                poker.players.size() < 6;

            Rectangle inviteButton = {
                actionArea.x + 161,
                actionArea.y + 18,
                145,
                42
            };

            if (
                canInvite &&
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
                OpenPlayerInvite(app, GameView::POKER);
            }

            Rectangle endButton = {
                actionArea.x + (canInvite ? 316 : 161),
                actionArea.y + 18,
                110,
                42
            };

            if (
                isHost &&
                !interactionsBlocked &&
                DrawButton(
                    endButton,
                    "END GAME",
                    PANEL_LIGHT,
                    JENG_RED,
                    TEXT_MAIN,
                    13
                )
            )
            {
                SendPokerCommand(app, "/pokerend");
            }

            DrawText(
                poker.status.c_str(),
                (int)actionArea.x + (
                    isHost
                    ? (canInvite ? 440 : 285)
                    : 165
                ),
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
                (poker.turn.empty() ? string("another player") : poker.turn) +
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

    void DrawPokerLeaderboard(
        AppState& app,
        Rectangle bounds,
        bool interactionsBlocked
    )
    {
        PokerClientState& poker = app.poker;

        Rectangle panel = {
            bounds.x + 70,
            bounds.y + 76,
            bounds.width - 140,
            bounds.height - 130
        };
        DrawRectangleRounded(panel, 0.04f, 10, PANEL_ALT);
        DrawRectangleRoundedLinesEx(
            panel,
            0.04f,
            10,
            1.5f,
            Color{255,255,255,45}
        );

        DrawText(
            "FINAL CHIP LEADERBOARD",
            (int)panel.x + 24,
            (int)panel.y + 20,
            24,
            JENG_YELLOW
        );

        string starting =
            "Every player started with " +
            to_string(poker.leaderboardStartingChips) +
            " chips";
        DrawText(
            starting.c_str(),
            (int)panel.x + 25,
            (int)panel.y + 53,
            14,
            TEXT_MUTED
        );

        Rectangle list = {
            panel.x + 22,
            panel.y + 82,
            panel.width - 44,
            panel.height - 142
        };

        const int visibleRows = 6;
        int maxScroll = max(
            0,
            (int)poker.leaderboard.size() - visibleRows
        );
        if (IsMouseInside(list))
            poker.leaderboardScroll -= (int)GetMouseWheelMove();
        poker.leaderboardScroll = max(
            0,
            min(maxScroll, poker.leaderboardScroll)
        );

        int end = min(
            (int)poker.leaderboard.size(),
            poker.leaderboardScroll + visibleRows
        );
        int lastRankedIndex = -1;
        for (int i = 0; i < (int)poker.leaderboard.size(); i++)
            if (!poker.leaderboard[i].left)
                lastRankedIndex = i;

        int rankNumber = 0;
        for (int i = 0; i < poker.leaderboardScroll; i++)
            if (!poker.leaderboard[i].left)
                rankNumber++;

        for (int i = poker.leaderboardScroll; i < end; i++)
        {
            const PokerLeaderboardEntry& entry = poker.leaderboard[i];
            int rowIndex = i - poker.leaderboardScroll;
            Rectangle row = {
                list.x,
                list.y + rowIndex * 45.0f,
                list.width,
                38
            };
            DrawRectangleRounded(
                row,
                0.08f,
                6,
                i == 0 ? Color{50,57,40,255} : PANEL_LIGHT
            );

            if (!entry.left)
                rankNumber++;
            string rank = entry.left
                ? "LEFT"
                : "#" + to_string(rankNumber);
            DrawText(
                rank.c_str(),
                (int)row.x + 12,
                (int)row.y + 11,
                entry.left ? 12 : 14,
                entry.left
                    ? JENG_RED
                    : (i == 0 ? JENG_YELLOW : TEXT_MUTED)
            );
            DrawText(
                entry.name.c_str(),
                (int)row.x + 58,
                (int)row.y + 10,
                16,
                entry.name == app.username ? JENG_YELLOW : TEXT_MAIN
            );

            int difference =
                entry.chips - poker.leaderboardStartingChips;
            string chipText =
                to_string(entry.chips) + " chips  (" +
                (difference >= 0 ? "+" : "") +
                to_string(difference) + ")";
            int chipWidth = MeasureText(chipText.c_str(), 14);
            DrawText(
                chipText.c_str(),
                (int)(row.x + row.width - chipWidth - 12),
                (int)row.y + 11,
                14,
                difference >= 0 ? SUCCESS : JENG_RED
            );

            if (!entry.left && (i == 0 || i == lastRankedIndex))
            {
                const char* label =
                    i == 0 ? "MOST" : "LEAST";
                DrawText(
                    label,
                    (int)row.x + 205,
                    (int)row.y + 12,
                    12,
                    i == 0 ? JENG_YELLOW : TEXT_MUTED
                );
            }
        }

        Rectangle doneButton = {
            panel.x + panel.width - 112,
            panel.y + panel.height - 48,
            88,
            32
        };
        if (
            !interactionsBlocked &&
            DrawButton(
                doneButton,
                "DONE",
                JENG_RED,
                Color{255,80,80,255},
                WHITE,
                13
            )
        )
        {
            poker.showLeaderboard = false;
            poker.leaderboard.clear();
        }
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
        "2-6 player Texas Hold'em",
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

    if (app.poker.showLeaderboard)
    {
        DrawPokerLeaderboard(
            app,
            bounds,
            interactionsBlocked
        );
        return;
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

    vector<PokerClientPlayer> seats = SeatOrder(poker);
    for (int i = 0; i < (int)seats.size() && i < 6; i++)
        DrawPokerSeat(
            seats[i],
            felt,
            i,
            poker,
            seats[i].name == app.username,
            false
        );

    float centerX = felt.x + felt.width / 2.0f;
    float centerY = felt.y + felt.height / 2.0f;
    vector<string> board = poker.communityCards;
    while (board.size() < 5)
        board.push_back("--");

    // The community row owns the clear center lane. Player cards stay
    // around the outer rail, so all five board cards remain unobstructed.
    DrawCardRow(
        board,
        centerX,
        centerY - 39,
        48,
        67,
        6,
        false
    );

    Rectangle potBadge = {
        centerX - 43,
        centerY + 37,
        86,
        34
    };
    DrawRectangleRounded(potBadge, 0.12f, 8, Color{12,49,37,245});
    DrawRectangleRoundedLinesEx(
        potBadge,
        0.12f,
        8,
        1.5f,
        JENG_YELLOW
    );
    string potText = "POT " + to_string(poker.pot);
    DrawCenteredText(potText.c_str(), potBadge, 14, JENG_YELLOW);

    string stageLine =
        "Hand " + to_string(poker.handNumber) +
        "   " + poker.stage +
        "   Blinds " + to_string(poker.smallBlind) +
        "/" + to_string(poker.bigBlind);
    DrawText(
        stageLine.c_str(),
        (int)bounds.x + 30,
        (int)(bounds.y + bounds.height - 137),
        13,
        TEXT_MUTED
    );

    string betLine =
        "Your bet: " + to_string(poker.yourBet) +
        "   Players: " + to_string(poker.players.size());
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
