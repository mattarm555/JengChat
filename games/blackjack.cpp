#include "blackjack.h"

#include "cards/card_renderer.h"

#include "../networking.h"
#include "../theme.h"
#include "../ui/command_popup.h"
#include "../ui/ui_common.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace std;

namespace
{
    // Larger cards for the graphical table.
    const float CARD_W = 52.0f;
    const float CARD_H = 76.0f;
    const float CARD_STEP = 33.0f;

    string CardRank(const string& code)
    {
        if (code.size() < 2 || code == "--")
            return "";

        return code.substr(0, code.size() - 1);
    }

    int VisibleCardValue(const string& code)
    {
        string rank = CardRank(code);

        if (rank.empty())
            return 0;

        if (rank == "A")
            return 11;

        if (rank == "K" || rank == "Q" || rank == "J")
            return 10;

        try
        {
            return stoi(rank);
        }
        catch (...)
        {
            return 0;
        }
    }

    int VisibleHandValue(const vector<string>& cards)
    {
        int total = 0;
        int aces = 0;

        for (const string& card : cards)
        {
            if (card == "--")
                continue;

            int value = VisibleCardValue(card);
            total += value;

            if (CardRank(card) == "A")
                aces++;
        }

        while (total > 21 && aces > 0)
        {
            total -= 10;
            aces--;
        }

        return total;
    }

    string PlayerTotalText(const BlackjackPlayerClientState& player)
    {
        if (player.hands.empty())
            return "-";

        string result;

        for (int i = 0; i < (int)player.hands.size(); i++)
        {
            if (i > 0)
                result += " / ";

            result += to_string(player.hands[i].value);
        }

        return result;
    }

    BlackjackPlayerClientState* FindPlayer(
        BlackjackClientState& bj,
        const string& name
    )
    {
        for (BlackjackPlayerClientState& player : bj.players)
        {
            if (player.name == name)
                return &player;
        }

        return nullptr;
    }

    const BlackjackPlayerClientState* FindPlayer(
        const BlackjackClientState& bj,
        const string& name
    )
    {
        for (const BlackjackPlayerClientState& player : bj.players)
        {
            if (player.name == name)
                return &player;
        }

        return nullptr;
    }

    int PlayerIndex(
        const BlackjackClientState& bj,
        const string& name
    )
    {
        for (int i = 0; i < (int)bj.players.size(); i++)
        {
            if (bj.players[i].name == name)
                return i;
        }

        return -1;
    }

    int ReservedBet(const BlackjackPlayerClientState& player)
    {
        int total = 0;

        for (const BlackjackHandClientState& hand : player.hands)
            total += hand.bet;

        return total;
    }

    Rectangle PlayerSeatRect(
        Rectangle table,
        int index
    )
    {
        const float gap = 10.0f;
        const float sidePad = 18.0f;
        const float seatWidth =
            (table.width - sidePad * 2.0f - gap * 2.0f) / 3.0f;
        const float seatHeight = 130.0f;

        int row = index / 3;
        int col = index % 3;

        return {
            table.x + sidePad + col * (seatWidth + gap),
            table.y + 142.0f + row * 140.0f,
            seatWidth,
            seatHeight
        };
    }

    Rectangle HandZone(
        Rectangle seat,
        int handIndex,
        int handCount
    )
    {
        Rectangle content = {
            seat.x + 5,
            seat.y + 30,
            seat.width - 10,
            seat.height - 34
        };

        if (handCount <= 1)
            return content;

        float gap = 5.0f;
        float width = (content.width - gap) / 2.0f;

        return {
            content.x + handIndex * (width + gap),
            content.y,
            width,
            content.height
        };
    }

    Rectangle CardRect(
        Rectangle zone,
        int cardIndex,
        int cardCount
    )
    {
        float step = CARD_STEP;

        if (cardCount > 4)
            step = 20.0f;

        float totalWidth = CARD_W;

        if (cardCount > 1)
            totalWidth += (cardCount - 1) * step;

        float startX =
            zone.x +
            zone.width / 2.0f -
            totalWidth / 2.0f;

        return {
            startX + cardIndex * step,
            zone.y + 10.0f,
            CARD_W,
            CARD_H
        };
    }

    bool EventMatches(
        const BlackjackDealEvent& event,
        const string& target,
        int handIndex,
        int cardIndex
    )
    {
        return
            event.target == target &&
            event.handIndex == handIndex &&
            event.cardIndex == cardIndex;
    }

    bool CardStillAnimating(
        const BlackjackClientState& bj,
        const string& target,
        int handIndex,
        int cardIndex
    )
    {
        for (const BlackjackDealEvent& event : bj.dealQueue)
        {
            if (EventMatches(event, target, handIndex, cardIndex))
                return true;
        }

        return false;
    }

    void DrawHand(
        const BlackjackClientState& bj,
        const BlackjackHandClientState& hand,
        Rectangle zone,
        const string& target,
        int handIndex,
        bool activeHand,
        bool showHeader
    )
    {
        if (activeHand)
        {
            DrawRectangleRounded(
                zone,
                0.04f,
                8,
                Color{245, 205, 66, 24}
            );

            DrawRectangleRoundedLinesEx(
                zone,
                0.04f,
                8,
                2.0f,
                JENG_YELLOW
            );
        }

        if (showHeader)
        {
            string header =
                "H" + to_string(handIndex + 1) +
                " B" + to_string(hand.bet) +
                " T" + to_string(hand.value);

            DrawText(
                header.c_str(),
                (int)zone.x + 3,
                (int)zone.y + 1,
                11,
                activeHand ? JENG_YELLOW : TEXT_MUTED
            );
        }

        int count = (int)hand.cards.size();

        for (int i = 0; i < count; i++)
        {
            if (CardStillAnimating(bj, target, handIndex, i))
                continue;

            Rectangle cardRect = CardRect(zone, i, count);

            if (hand.cards[i] == "--")
                DrawCardBack(cardRect);
            else
                DrawPlayingCard(hand.cards[i], cardRect);
        }

        if (hand.busted)
        {
            DrawText(
                "BUST",
                (int)zone.x + 3,
                (int)(zone.y + zone.height - 13),
                10,
                JENG_RED
            );
        }
        else if (hand.doubled)
        {
            DrawText(
                "DOUBLE",
                (int)zone.x + 3,
                (int)(zone.y + zone.height - 13),
                9,
                SUCCESS
            );
        }
    }

    void DrawPlayerSeat(
        const AppState& app,
        const BlackjackPlayerClientState& player,
        Rectangle seat
    )
    {
        const BlackjackClientState& bj = app.blackjack;

        bool isYou = player.name == app.username;
        bool isTurn = bj.phase == "PLAYING" && bj.turn == player.name;

        DrawRectangleRounded(
            seat,
            0.05f,
            8,
            isYou
                ? Color{28, 76, 58, 230}
                : Color{12, 54, 39, 205}
        );

        DrawRectangleRoundedLinesEx(
            seat,
            0.05f,
            8,
            isTurn ? 2.0f : 1.0f,
            isTurn ? JENG_YELLOW : Color{80, 116, 96, 255}
        );

        string label =
            (isYou ? "YOU - " : "") +
            player.name +
            "   [" +
            PlayerTotalText(player) +
            "]   " +
            to_string(player.chips) +
            " chips";

        DrawText(
            label.c_str(),
            (int)seat.x + 8,
            (int)seat.y + 7,
            15,
            isYou ? JENG_YELLOW : TEXT_MAIN
        );

        int handCount = max(1, (int)player.hands.size());

        for (int i = 0; i < (int)player.hands.size(); i++)
        {
            Rectangle zone = HandZone(seat, i, handCount);

            bool activeHand =
                isTurn &&
                bj.turnHandIndex == i;

            DrawHand(
                bj,
                player.hands[i],
                zone,
                player.name,
                i,
                activeHand,
                handCount > 1
            );
        }

        if (bj.phase == "BETTING")
        {
            string betText = player.betPlaced
                ? "BET " + to_string(player.pendingBet)
                : (player.chips > 0 ? "WAITING FOR BET" : "OUT OF CHIPS");

            DrawText(
                betText.c_str(),
                (int)seat.x + 6,
                (int)(seat.y + seat.height - 16),
                10,
                player.betPlaced ? SUCCESS : TEXT_MUTED
            );
        }
    }

    Rectangle TargetCardRect(
        const AppState& app,
        Rectangle table,
        const BlackjackDealEvent& event
    )
    {
        const BlackjackClientState& bj = app.blackjack;

        Rectangle dealerRow = {
            table.x + table.width / 2.0f - 130.0f,
            table.y + 22.0f,
            280.0f,
            120.0f
        };

        if (event.target == "DEALER")
        {
            return CardRect(
                dealerRow,
                event.cardIndex,
                max(event.cardIndex + 1, (int)bj.dealerCards.size())
            );
        }

        int playerIndex = PlayerIndex(bj, event.target);

        if (playerIndex < 0)
            return {table.x + table.width / 2.0f, table.y + 200.0f, CARD_W, CARD_H};

        const BlackjackPlayerClientState& player = bj.players[playerIndex];
        Rectangle seat = PlayerSeatRect(table, playerIndex);
        int handCount = max(1, (int)player.hands.size());
        int safeHand = clamp(event.handIndex, 0, handCount - 1);
        Rectangle zone = HandZone(seat, safeHand, handCount);

        int cardCount = event.cardIndex + 1;

        if (safeHand < (int)player.hands.size())
        {
            cardCount = max(
                cardCount,
                (int)player.hands[safeHand].cards.size()
            );
        }

        return CardRect(zone, event.cardIndex, cardCount);
    }

    void UpdateAndDrawDealAnimation(
        AppState& app,
        Rectangle table
    )
    {
        BlackjackClientState& bj = app.blackjack;

        if (bj.dealQueue.empty())
            return;

        BlackjackDealEvent& event = bj.dealQueue.front();

        event.progress += GetFrameTime() / 0.22f;

        float t = clamp(event.progress, 0.0f, 1.0f);
        float eased = t * t * (3.0f - 2.0f * t);

        Rectangle destination = TargetCardRect(app, table, event);

        Rectangle shoe = {
            table.x + table.width - 58.0f,
            table.y + 30.0f,
            CARD_W,
            CARD_H
        };

        Rectangle animated = {
            shoe.x + (destination.x - shoe.x) * eased,
            shoe.y + (destination.y - shoe.y) * eased,
            CARD_W,
            CARD_H
        };

        if (event.card == "--")
            DrawCardBack(animated);
        else
            DrawPlayingCard(event.card, animated);

        if (event.progress >= 1.0f)
            bj.dealQueue.erase(bj.dealQueue.begin());
    }

    void DrawDisabledButton(Rectangle rect, const char* label)
    {
        DrawRectangleRounded(rect, 0.10f, 8, Color{55, 57, 64, 255});
        DrawCenteredText(label, rect, 13, Color{115, 118, 128, 255});
    }

    bool DrawActionButton(
        Rectangle rect,
        const char* label,
        bool enabled,
        Color base,
        Color hover,
        Color textColor,
        int fontSize = 13
    )
    {
        if (!enabled)
        {
            DrawRectangleRounded(
                rect,
                0.10f,
                8,
                Color{55, 57, 64, 255}
            );

            DrawCenteredText(
                label,
                rect,
                fontSize,
                Color{115, 118, 128, 255}
            );

            return false;
        }

        return DrawButton(
            rect,
            label,
            base,
            hover,
            textColor,
            fontSize
        );
    }
}

void DrawBlackjackPanel(
    AppState& app,
    Rectangle bounds,
    bool interactionsBlocked
)
{
    BlackjackClientState& bj = app.blackjack;

    DrawText(
        "BLACKJACK",
        (int)bounds.x + 22,
        (int)bounds.y + 16,
        26,
        JENG_RED
    );

    string handCounter =
        bj.currentHand > 0
        ? "HAND " + to_string(bj.currentHand) + " / " + to_string(bj.totalHands)
        : "CASINO TABLE";

    DrawText(
        handCounter.c_str(),
        (int)bounds.x + 180,
        (int)bounds.y + 24,
        13,
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
        DrawButton(homeButton, "HOME", PANEL_LIGHT, JENG_RED, TEXT_MAIN, 14)
    )
    {
        app.gameView = GameView::HOME;
    }

    Rectangle table = {
        bounds.x + 18,
        bounds.y + 60,
        bounds.width - 36,
        430
    };

    DrawRectangleRounded(table, 0.035f, 12, Color{18, 73, 50, 255});
    DrawRectangleRoundedLinesEx(table, 0.035f, 12, 2.0f, Color{130, 96, 53, 255});

    Rectangle shoe = {
        table.x + table.width - 58.0f,
        table.y + 30.0f,
        CARD_W,
        CARD_H
    };

    DrawCardBack(shoe);
    DrawText("SHOE", (int)shoe.x + 5, (int)shoe.y + (int)shoe.height + 4, 10, TEXT_MUTED);

    // ========================================================
    // NO TABLE YET
    // ========================================================

    if (!bj.active && bj.phase == "WAITING")
    {
        DrawText(
            "Create a Blackjack table.",
            (int)table.x + 28,
            (int)table.y + 34,
            20,
            TEXT_MAIN
        );

        DrawText(
            "Choose the table settings first, then invite up to 5 other players.",
            (int)table.x + 28,
            (int)table.y + 70,
            14,
            TEXT_MUTED
        );

        DrawText(
            "Blackjack pays 3:2. Regular wins pay 1:1. Double and one split are supported.",
            (int)table.x + 28,
            (int)table.y + 94,
            13,
            TEXT_MUTED
        );

        Rectangle createButton = {
            table.x + 28,
            table.y + 140,
            180,
            44
        };

        if (
            !interactionsBlocked &&
            DrawButton(
                createButton,
                "CREATE TABLE",
                JENG_RED,
                Color{255, 80, 80, 255},
                WHITE,
                14
            )
        )
        {
            OpenCommandPrompt(
                app.commandPopup,
                "CREATE BLACKJACK",
                "Choose your Blackjack table settings.",
                "/blackjackcreate",
                {
                    "Starting chips",
                    "Number of hands"
                }
            );
        }

        DrawText(
            bj.status.c_str(),
            (int)table.x + 28,
            (int)table.y + 220,
            14,
            JENG_YELLOW
        );

        return;
    }

    // ========================================================
    // LOBBY
    // ========================================================

    if (bj.phase == "LOBBY")
    {
        bool isHost = bj.hostName == app.username;

        DrawText(
            "BLACKJACK LOBBY",
            (int)table.x + 28,
            (int)table.y + 28,
            20,
            JENG_YELLOW
        );

        string countText =
            to_string((int)bj.players.size()) +
            " / 6 PLAYERS";

        DrawText(
            countText.c_str(),
            (int)table.x + 28,
            (int)table.y + 58,
            14,
            TEXT_MUTED
        );

        int listY = (int)table.y + 94;

        for (int i = 0; i < (int)bj.players.size(); i++)
        {
            const BlackjackPlayerClientState& player = bj.players[i];
            string label =
                string(i == 0 ? "HOST  " : "      ") +
                player.name +
                "   " +
                to_string(player.chips) +
                " chips";

            DrawText(
                label.c_str(),
                (int)table.x + 40,
                listY + i * 27,
                14,
                player.name == app.username ? JENG_YELLOW : TEXT_MAIN
            );
        }

        Rectangle inviteButton = {
            table.x + 28,
            table.y + 300,
            170,
            42
        };

        Rectangle startButton = {
            table.x + 210,
            table.y + 300,
            150,
            42
        };

        if (
            isHost &&
            (int)bj.players.size() < 6 &&
            !interactionsBlocked &&
            DrawButton(
                inviteButton,
                "INVITE PLAYERS",
                JENG_RED,
                Color{255, 80, 80, 255},
                WHITE,
                13
            )
        )
        {
            OpenCommandPrompt(
                app.commandPopup,
                "BLACKJACK",
                "Invite players to a Blackjack match.",
                "/blackjack",
                {"Player username"}
            );
        }

        if (
            isHost &&
            !interactionsBlocked &&
            DrawActionButton(
                startButton,
                "START MATCH",
                bj.players.size() >= 2,
                SUCCESS,
                Color{90, 235, 140, 255},
                BG
            )
        )
        {
            NetSendLine("BJ_START");
        }

        if (!isHost)
        {
            DrawText(
                "Waiting for the host to invite players and start the match...",
                (int)table.x + 28,
                (int)table.y + 310,
                13,
                TEXT_MUTED
            );
        }

        DrawText(
            bj.status.c_str(),
            (int)table.x + 28,
            (int)table.y + 370,
            13,
            TEXT_MAIN
        );

        return;
    }

    // ========================================================
    // ACTIVE TABLE
    // ========================================================

    Rectangle dealerRow = {
        table.x + table.width / 2.0f - 130.0f,
        table.y + 22.0f,
        280.0f,
        120.0f
    };

    string dealerLabel;

    if (bj.dealerRevealed)
    {
        dealerLabel =
            "DEALER   [" +
            to_string(VisibleHandValue(bj.dealerCards)) +
            "]";
    }
    else
    {
        dealerLabel =
            "DEALER - HOLE CARD HIDDEN";
    }

    DrawText(
        dealerLabel.c_str(),
        (int)dealerRow.x,
        (int)dealerRow.y - 18,
        14,
        TEXT_MUTED
    );

    int dealerCount = (int)bj.dealerCards.size();

    for (int i = 0; i < dealerCount; i++)
    {
        if (CardStillAnimating(bj, "DEALER", 0, i))
            continue;

        Rectangle cardRect = CardRect(dealerRow, i, dealerCount);

        if (bj.dealerCards[i] == "--")
            DrawCardBack(cardRect);
        else
            DrawPlayingCard(bj.dealerCards[i], cardRect);
    }

    for (int i = 0; i < (int)bj.players.size() && i < 6; i++)
    {
        DrawPlayerSeat(
            app,
            bj.players[i],
            PlayerSeatRect(table, i)
        );
    }

    UpdateAndDrawDealAnimation(app, table);

    DrawRectangle(
        (int)table.x,
        (int)(table.y + table.height - 25),
        (int)table.width,
        25,
        Color{8, 36, 27, 220}
    );

    // During RESULT the dedicated results panel owns this information.
    // Keeping the old bottom status line would draw text behind the panel.
    if (bj.phase != "RESULT")
    {
        DrawText(
            bj.status.c_str(),
            (int)table.x + 10,
            (int)table.y + (int)table.height - 18,
            11,
            TEXT_MAIN
        );
    }

    BlackjackPlayerClientState* you = FindPlayer(bj, app.username);

    float controlsY = bounds.y + bounds.height - 58;

    Rectangle resignButton = {
        bounds.x + bounds.width - 122,
        controlsY,
        104,
        38
    };

    bool canInteract =
        !interactionsBlocked &&
        bj.dealQueue.empty();

    if (bj.phase == "BETTING" && you)
    {
        int chips = you->chips;
        int minimumBet = chips > 0 ? min(10, chips) : 0;

        bj.betAmount = clamp(
            bj.betAmount,
            minimumBet,
            max(minimumBet, chips)
        );

        if (
            bj.betStep != 10 &&
            bj.betStep != 50 &&
            bj.betStep != 100
        )
        {
            bj.betStep = 10;
        }

        DrawText(
            "BET",
            (int)bounds.x + 24,
            (int)controlsY + 11,
            14,
            JENG_YELLOW
        );

        Rectangle minusButton = {
            bounds.x + 62,
            controlsY,
            36,
            38
        };

        Rectangle amountBox = {
            bounds.x + 103,
            controlsY,
            76,
            38
        };

        Rectangle plusButton = {
            bounds.x + 184,
            controlsY,
            36,
            38
        };

        DrawText(
            "STEP",
            (int)bounds.x + 231,
            (int)controlsY + 13,
            11,
            TEXT_MUTED
        );

        Rectangle step10Button = {
            bounds.x + 270,
            controlsY,
            42,
            38
        };

        Rectangle step50Button = {
            bounds.x + 317,
            controlsY,
            42,
            38
        };

        Rectangle step100Button = {
            bounds.x + 364,
            controlsY,
            50,
            38
        };

        Rectangle betButton = {
            bounds.x + 424,
            controlsY,
            98,
            38
        };

        if (
            DrawActionButton(
                minusButton,
                "-",
                canInteract &&
                    bj.betAmount > minimumBet,
                PANEL_LIGHT,
                JENG_RED,
                TEXT_MAIN
            )
        )
        {
            bj.betAmount =
                max(
                    minimumBet,
                    bj.betAmount -
                    bj.betStep
                );
        }

        DrawRectangleRounded(
            amountBox,
            0.08f,
            8,
            PANEL_LIGHT
        );

        DrawCenteredText(
            to_string(bj.betAmount).c_str(),
            amountBox,
            15,
            TEXT_MAIN
        );

        if (
            DrawActionButton(
                plusButton,
                "+",
                canInteract &&
                    bj.betAmount < chips,
                PANEL_LIGHT,
                SUCCESS,
                TEXT_MAIN
            )
        )
        {
            bj.betAmount =
                min(
                    chips,
                    bj.betAmount +
                    bj.betStep
                );
        }

        if (
            DrawActionButton(
                step10Button,
                "10",
                canInteract,
                bj.betStep == 10
                    ? JENG_YELLOW
                    : PANEL_LIGHT,
                JENG_RED,
                bj.betStep == 10
                    ? BG
                    : TEXT_MAIN,
                11
            )
        )
        {
            bj.betStep = 10;
        }

        if (
            DrawActionButton(
                step50Button,
                "50",
                canInteract,
                bj.betStep == 50
                    ? JENG_YELLOW
                    : PANEL_LIGHT,
                JENG_RED,
                bj.betStep == 50
                    ? BG
                    : TEXT_MAIN,
                11
            )
        )
        {
            bj.betStep = 50;
        }

        if (
            DrawActionButton(
                step100Button,
                "100",
                canInteract,
                bj.betStep == 100
                    ? JENG_YELLOW
                    : PANEL_LIGHT,
                JENG_RED,
                bj.betStep == 100
                    ? BG
                    : TEXT_MAIN,
                10
            )
        )
        {
            bj.betStep = 100;
        }

        if (
            DrawActionButton(
                betButton,
                you->betPlaced
                    ? "BET SET"
                    : "PLACE BET",
                canInteract &&
                    !you->betPlaced &&
                    chips > 0,
                JENG_RED,
                Color{255, 80, 80, 255},
                WHITE
            )
        )
        {
            NetSendLine(
                "BJ_BET|" +
                to_string(bj.betAmount)
            );
        }

        string waitText =
            "Waiting for all active players to bet.";

        DrawText(
            waitText.c_str(),
            (int)bounds.x + 535,
            (int)controlsY + 12,
            11,
            TEXT_MUTED
        );
    }
    else if (bj.phase == "PLAYING" && you)
    {
        bool yourTurn =
            bj.turn == app.username &&
            bj.turnHandIndex >= 0 &&
            bj.turnHandIndex < (int)you->hands.size();

        const BlackjackHandClientState* activeHand =
            yourTurn ? &you->hands[bj.turnHandIndex] : nullptr;

        int available = you->chips - ReservedBet(*you);

        bool canDouble =
            activeHand &&
            !activeHand->done &&
            activeHand->cards.size() == 2 &&
            available >= activeHand->bet;

        bool sameRank =
            activeHand &&
            activeHand->cards.size() == 2 &&
            CardRank(activeHand->cards[0]) == CardRank(activeHand->cards[1]);

        bool canSplit =
            activeHand &&
            !activeHand->done &&
            you->hands.size() < 2 &&
            sameRank &&
            available >= activeHand->bet;

        Rectangle hitButton = {bounds.x + 20, controlsY, 78, 38};
        Rectangle standButton = {bounds.x + 106, controlsY, 82, 38};
        Rectangle doubleButton = {bounds.x + 196, controlsY, 86, 38};
        Rectangle splitButton = {bounds.x + 290, controlsY, 78, 38};

        if (DrawActionButton(hitButton, "HIT", canInteract && yourTurn, SUCCESS, Color{90, 235, 140, 255}, BG))
            NetSendLine("BJ_HIT");

        if (DrawActionButton(standButton, "STAND", canInteract && yourTurn, PANEL_LIGHT, JENG_YELLOW, TEXT_MAIN))
            NetSendLine("BJ_STAND");

        if (DrawActionButton(doubleButton, "DOUBLE", canInteract && yourTurn && canDouble, JENG_RED, Color{255, 80, 80, 255}, WHITE))
            NetSendLine("BJ_DOUBLE");

        if (DrawActionButton(splitButton, "SPLIT", canInteract && yourTurn && canSplit, Color{75, 95, 150, 255}, Color{95, 125, 190, 255}, WHITE))
            NetSendLine("BJ_SPLIT");

        string turnText = yourTurn
            ? "YOUR TURN - HAND " + to_string(bj.turnHandIndex + 1)
            : "TURN: " + bj.turn;

        DrawText(
            turnText.c_str(),
            (int)bounds.x + 382,
            (int)controlsY + 12,
            12,
            yourTurn ? JENG_YELLOW : TEXT_MUTED
        );
    }
    else if (bj.phase == "RESULT")
    {
        bool isHost = bj.hostName == app.username;

        Rectangle nextButton = {
            bounds.x + 20,
            controlsY,
            120,
            38
        };

        if (
            DrawActionButton(
                nextButton,
                "NEXT HAND",
                canInteract && bj.awaitingNextHand && isHost,
                SUCCESS,
                Color{90, 235, 140, 255},
                BG
            )
        )
        {
            NetSendLine("BJ_NEXT");
        }

        // ====================================================
        // HAND RESULT OVERLAY
        // ====================================================
        //
        // The panel has a FIXED lower-left anchor:
        //   - its left edge stays beside NEXT HAND
        //   - its bottom edge stays above the controls
        //
        // As more result rows are needed it grows UPWARD.
        // If a second column is needed it grows TO THE RIGHT.
        // ====================================================

        const int messageCount =
            (int)bj.payoutMessages.size();

        const bool twoColumns =
            messageCount > 5;

        const int firstColumnCount =
            twoColumns
                ? (messageCount + 1) / 2
                : messageCount;

        const int rows =
            twoColumns
                ? firstColumnCount
                : messageCount;

        const float panelX =
            bounds.x + 150.0f;

        const float panelBottom =
            controlsY - 8.0f;

        float panelWidth =
            twoColumns
                ? 610.0f
                : 475.0f;

        // Never run underneath the right-side CLOSE/LEAVE area.
        const float maxPanelWidth =
            bounds.x +
            bounds.width -
            132.0f -
            panelX;

        panelWidth =
            min(
                panelWidth,
                maxPanelWidth
            );

        float panelHeight =
            74.0f +
            max(1, rows) * 22.0f;

        panelHeight =
            min(
                panelHeight,
                235.0f
            );

        Rectangle resultPanel = {
            panelX,
            panelBottom - panelHeight,
            panelWidth,
            panelHeight
        };

        DrawRectangleRounded(
            resultPanel,
            0.045f,
            10,
            Color{11, 45, 35, 248}
        );

        DrawRectangleRoundedLinesEx(
            resultPanel,
            0.045f,
            10,
            2.0f,
            JENG_YELLOW
        );

        DrawText(
            "HAND RESULTS",
            (int)resultPanel.x + 18,
            (int)resultPanel.y + 12,
            18,
            JENG_YELLOW
        );

        DrawText(
            bj.status.c_str(),
            (int)resultPanel.x + 18,
            (int)resultPanel.y + 39,
            13,
            TEXT_MAIN
        );

        if (messageCount == 0)
        {
            DrawText(
                "Waiting for payout details...",
                (int)resultPanel.x + 18,
                (int)resultPanel.y + 64,
                13,
                TEXT_MUTED
            );
        }
        else
        {
            const float columnGap = 20.0f;

            const float columnWidth =
                twoColumns
                    ? (
                        resultPanel.width -
                        56.0f -
                        columnGap
                      ) / 2.0f
                    : resultPanel.width - 36.0f;

            for (int i = 0; i < messageCount; i++)
            {
                int column = 0;
                int row = i;

                if (
                    twoColumns &&
                    i >= firstColumnCount
                )
                {
                    column = 1;
                    row =
                        i -
                        firstColumnCount;
                }

                float x =
                    resultPanel.x +
                    18.0f +
                    column *
                    (
                        columnWidth +
                        columnGap
                    );

                float y =
                    resultPanel.y +
                    66.0f +
                    row * 22.0f;

                const string& line =
                    bj.payoutMessages[i];

                // ------------------------------------------------
                // RESULT COLOR
                //
                // Packet text looks like:
                //
                // jeng hand 1: +100 chips - win pays 1:1
                // bob  hand 1: -50  chips - dealer wins
                // sam  hand 1: 0    chips - push
                //
                // Parse the signed chip delta instead of looking for
                // "-" anywhere in the line. Every message contains
                // "chips - reason", so substring-based loss detection
                // incorrectly made pushes red.
                // ------------------------------------------------

                int delta = 0;

                size_t amountStart =
                    line.find(": ");

                size_t amountEnd =
                    line.find(
                        " chips",
                        amountStart == string::npos
                            ? 0
                            : amountStart + 2
                    );

                if (
                    amountStart != string::npos &&
                    amountEnd != string::npos &&
                    amountEnd > amountStart + 2
                )
                {
                    string amountText =
                        line.substr(
                            amountStart + 2,
                            amountEnd -
                            (amountStart + 2)
                        );

                    try
                    {
                        delta =
                            stoi(amountText);
                    }
                    catch (...)
                    {
                        delta = 0;
                    }
                }

                Color lineColor =
                    TEXT_MUTED;

                if (delta > 0)
                {
                    // WIN
                    lineColor =
                        SUCCESS;
                }
                else if (delta < 0)
                {
                    // LOSS
                    lineColor =
                        JENG_RED;
                }
                else
                {
                    // PUSH
                    lineColor =
                        TEXT_MUTED;
                }

                DrawText(
                    line.c_str(),
                    (int)x,
                    (int)y,
                    14,
                    lineColor
                );
            }
        }
    }

    // --------------------------------------------------------
    // LEAVE / EXIT TABLE
    // --------------------------------------------------------
    //
    // Once BJ_END arrives the server-side match is already gone,
    // so there is nothing left to resign from. In that state this
    // button simply clears the local table and returns to Game Hub.
    //
    // During a live match it still sends BJ_RESIGN/BJ_LEAVE to the
    // authoritative server.
    // --------------------------------------------------------

    bool matchEnded =
        bj.phase == "ENDED" ||
        (!bj.active && bj.currentHand > 0);

    const char* exitLabel =
        matchEnded
        ? "EXIT TABLE"
        : (bj.hostName == app.username ? "CLOSE" : "LEAVE");

    if (
        DrawActionButton(
            resignButton,
            exitLabel,
            canInteract && (bj.active || matchEnded),
            PANEL_LIGHT,
            JENG_RED,
            TEXT_MAIN
        )
    )
    {
        if (matchEnded)
        {
            app.blackjack = BlackjackClientState{};
            app.gameView = GameView::HOME;
            return;
        }

        NetSendLine("BJ_RESIGN");
    }
}
