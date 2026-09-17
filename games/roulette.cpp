#include "../ui/players.h"
#include "roulette.h"

#include "../networking.h"
#include "../theme.h"
#include "../ui/command_popup.h"
#include "../ui/ui_common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace std;

namespace
{
    constexpr float PI_F = 3.14159265358979323846f;

    const array<int, 37> WHEEL_ORDER = {
        0, 32, 15, 19, 4, 21, 2, 25, 17, 34,
        6, 27, 13, 36, 11, 30, 8, 23, 10, 5,
        24, 16, 33, 1, 20, 14, 31, 9, 22, 18,
        29, 7, 28, 12, 35, 3, 26
    };

    bool IsRedNumber(int number)
    {
        static const int redNumbers[] = {
            1,3,5,7,9,12,14,16,18,
            19,21,23,25,27,30,32,34,36
        };

        for (int red : redNumbers)
        {
            if (number == red)
                return true;
        }

        return false;
    }

    Color RouletteNumberColor(int number)
    {
        if (number == 0)
            return Color{30, 150, 85, 255};

        return IsRedNumber(number)
            ? Color{190, 50, 50, 255}
            : Color{24, 26, 30, 255};
    }

    int WheelIndexForNumber(int number)
    {
        for (int i = 0; i < (int)WHEEL_ORDER.size(); i++)
        {
            if (WHEEL_ORDER[i] == number)
                return i;
        }

        return 0;
    }

    float EaseOutCubic(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        float inv = 1.0f - t;
        return 1.0f - inv * inv * inv;
    }

    int YourPlayerIndex(const AppState& app)
    {
        for (int i = 0; i < (int)app.roulette.players.size(); i++)
        {
            if (app.roulette.players[i].name == app.username)
                return i;
        }

        return -1;
    }

    int YourChips(const AppState& app)
    {
        int index = YourPlayerIndex(app);

        if (index < 0)
            return 0;

        return app.roulette.players[index].chips;
    }

    bool YouAreReady(const AppState& app)
    {
        int index = YourPlayerIndex(app);

        if (index < 0)
            return false;

        return app.roulette.players[index].ready;
    }

    int YourTotalBet(const AppState& app)
    {
        int total = 0;

        for (const RouletteBetClientState& bet : app.roulette.bets)
            total += bet.amount;

        return total;
    }

    int BetAmount(
        const AppState& app,
        const string& type,
        int value = 0
    )
    {
        int total = 0;

        for (const RouletteBetClientState& bet : app.roulette.bets)
        {
            if (
                bet.type == type &&
                bet.value == value
            )
            {
                total += bet.amount;
            }
        }

        return total;
    }

    bool DrawAction(
        Rectangle rect,
        const char* label,
        bool enabled,
        Color normal,
        Color hover,
        Color textColor = WHITE,
        int fontSize = 13
    )
    {
        bool hovering =
            enabled &&
            IsMouseInside(rect);

        DrawRectangleRounded(
            rect,
            0.10f,
            8,
            enabled
                ? (hovering ? hover : normal)
                : Color{65, 67, 74, 255}
        );

        int textWidth =
            MeasureText(label, fontSize);

        DrawText(
            label,
            (int)(
                rect.x +
                rect.width / 2.0f -
                textWidth / 2.0f
            ),
            (int)(
                rect.y +
                rect.height / 2.0f -
                fontSize / 2.0f
            ),
            fontSize,
            enabled ? textColor : TEXT_MUTED
        );

        return
            hovering &&
            IsMouseButtonPressed(
                MOUSE_BUTTON_LEFT
            );
    }

    void AddBet(
        AppState& app,
        const string& type,
        int value = 0
    )
    {
        RouletteClientState& roulette = app.roulette;

        if (
            roulette.phase != "BETTING" ||
            roulette.animating ||
            YouAreReady(app)
        )
        {
            return;
        }

        int available =
            YourChips(app) -
            YourTotalBet(app);

        int amount =
            min(
                roulette.chipAmount,
                available
            );

        if (amount <= 0)
            return;

        NetSendLine(
            "RLT_BET|" +
            type +
            "|" +
            to_string(value) +
            "|" +
            to_string(amount)
        );
    }

    void DrawBetBadge(
        Rectangle rect,
        int amount
    )
    {
        if (amount <= 0)
            return;

        float radius = 11.0f;

        Vector2 center = {
            rect.x + rect.width - 9.0f,
            rect.y + 9.0f
        };

        DrawCircleV(
            center,
            radius,
            JENG_YELLOW
        );

        string text = to_string(amount);
        int fontSize = text.size() > 3 ? 8 : 10;
        int width = MeasureText(text.c_str(), fontSize);

        DrawText(
            text.c_str(),
            (int)(center.x - width / 2.0f),
            (int)(center.y - fontSize / 2.0f),
            fontSize,
            BG
        );
    }

    bool DrawBetSpot(
        Rectangle rect,
        const string& label,
        Color fill,
        bool enabled,
        int amount,
        int fontSize = 12
    )
    {
        bool hover =
            enabled &&
            IsMouseInside(rect);

        DrawRectangleRec(
            rect,
            hover
                ? Color{
                    (unsigned char)min(255, fill.r + 25),
                    (unsigned char)min(255, fill.g + 25),
                    (unsigned char)min(255, fill.b + 25),
                    255
                }
                : fill
        );

        DrawRectangleLinesEx(
            rect,
            hover ? 2.0f : 1.0f,
            hover ? JENG_YELLOW : Color{120, 120, 125, 255}
        );

        int width =
            MeasureText(
                label.c_str(),
                fontSize
            );

        DrawText(
            label.c_str(),
            (int)(
                rect.x +
                rect.width / 2.0f -
                width / 2.0f
            ),
            (int)(
                rect.y +
                rect.height / 2.0f -
                fontSize / 2.0f
            ),
            fontSize,
            WHITE
        );

        DrawBetBadge(
            rect,
            amount
        );

        return
            hover &&
            IsMouseButtonPressed(
                MOUSE_BUTTON_LEFT
            );
    }

    void UpdateRouletteAnimation(
        RouletteClientState& roulette
    )
    {
        // The wheel animation and final-match transition are both
        // handled here so a final round cannot skip straight to the
        // end screen.
        if (!roulette.animating)
        {
            if (
                roulette.matchEndPending &&
                roulette.resultHoldStarted &&
                (
                    (float)GetTime() -
                    roulette.resultHoldStartTime
                ) >= roulette.resultHoldDuration
            )
            {
                roulette.matchEndPending = false;
                roulette.resultHoldStarted = false;
                roulette.active = false;
                roulette.phase = "ENDED";

                if (!roulette.pendingEndStatus.empty())
                    roulette.status = roulette.pendingEndStatus;

                roulette.pendingEndStatus.clear();
            }

            return;
        }

        float elapsed =
            (float)GetTime() -
            roulette.spinStartTime;

        float t =
            elapsed /
            roulette.spinDuration;

        if (t >= 1.0f)
        {
            t = 1.0f;
            roulette.animating = false;

            // On the final round, leave the wheel sitting on the
            // winning number for a moment before opening the end panel.
            if (roulette.matchEndPending)
            {
                roulette.resultHoldStarted = true;
                roulette.resultHoldStartTime = (float)GetTime();
            }
        }

        float eased =
            EaseOutCubic(t);

        int wheelIndex =
            WheelIndexForNumber(
                roulette.lastResult
            );

        float slotAngle =
            360.0f / 37.0f;

        float desiredFinal =
            -90.0f -
            wheelIndex * slotAngle;

        float start =
            roulette.wheelStartRotation;

        while (desiredFinal < start)
            desiredFinal += 360.0f;

        desiredFinal +=
            360.0f * 6.0f;

        roulette.wheelRotation =
            start +
            (desiredFinal - start) *
            eased;
    }

    void DrawWheel(
        RouletteClientState& roulette,
        Vector2 center,
        float radius
    )
    {
        UpdateRouletteAnimation(
            roulette
        );

        float slotAngle =
            360.0f / 37.0f;

        for (int i = 0; i < 37; i++)
        {
            int number =
                WHEEL_ORDER[i];

            float startAngle =
                roulette.wheelRotation +
                i * slotAngle -
                slotAngle / 2.0f;

            float endAngle =
                startAngle +
                slotAngle;

            DrawRing(
                center,
                radius - 44.0f,
                radius,
                startAngle,
                endAngle,
                6,
                RouletteNumberColor(number)
            );

            float middle =
                (
                    startAngle +
                    endAngle
                ) /
                2.0f;

            float radians =
                middle *
                PI_F /
                180.0f;

            Vector2 labelPosition = {
                center.x +
                    cosf(radians) *
                    (radius - 21.0f),
                center.y +
                    sinf(radians) *
                    (radius - 21.0f)
            };

            string text =
                to_string(number);

            int fontSize = 9;
            int width =
                MeasureText(
                    text.c_str(),
                    fontSize
                );

            DrawText(
                text.c_str(),
                (int)(labelPosition.x - width / 2.0f),
                (int)(labelPosition.y - fontSize / 2.0f),
                fontSize,
                WHITE
            );
        }

        DrawCircleV(
            center,
            radius - 48.0f,
            Color{42, 31, 19, 255}
        );

        DrawCircleLines(
            (int)center.x,
            (int)center.y,
            radius,
            JENG_YELLOW
        );

        DrawCircleLines(
            (int)center.x,
            (int)center.y,
            radius - 44.0f,
            JENG_YELLOW
        );

        DrawCircleV(
            center,
            18.0f,
            JENG_RED
        );

        // Fixed pointer at the top. At the end of the animation the
        // winning slot is rotated beneath this point.
        DrawTriangle(
            Vector2{center.x, center.y - radius - 6.0f},
            Vector2{center.x - 8.0f, center.y - radius - 20.0f},
            Vector2{center.x + 8.0f, center.y - radius - 20.0f},
            JENG_YELLOW
        );

        float ballAngle = -90.0f;

        if (roulette.animating)
        {
            float elapsed =
                (float)GetTime() -
                roulette.spinStartTime;

            float t =
                std::clamp(
                    elapsed /
                    roulette.spinDuration,
                    0.0f,
                    1.0f
                );

            float eased =
                EaseOutCubic(t);

            ballAngle =
                -90.0f -
                (1.0f - eased) *
                360.0f *
                8.0f;
        }

        float ballRadius =
            radius - 55.0f;

        float radians =
            ballAngle *
            PI_F /
            180.0f;

        Vector2 ball = {
            center.x +
                cosf(radians) *
                ballRadius,
            center.y +
                sinf(radians) *
                ballRadius
        };

        DrawCircleV(
            ball,
            6.0f,
            WHITE
        );

        DrawCircleLines(
            (int)ball.x,
            (int)ball.y,
            6.0f,
            Color{180, 180, 185, 255}
        );

        if (
            !roulette.animating &&
            roulette.lastResult >= 0
        )
        {
            string result =
                "WINNING NUMBER: " +
                to_string(
                    roulette.lastResult
                );

            int width =
                MeasureText(
                    result.c_str(),
                    16
                );

            DrawText(
                result.c_str(),
                (int)(
                    center.x -
                    width / 2.0f
                ),
                (int)(
                    center.y +
                    radius +
                    22.0f
                ),
                16,
                roulette.lastResult == 0
                    ? SUCCESS
                    : (
                        IsRedNumber(roulette.lastResult)
                            ? JENG_RED
                            : TEXT_MAIN
                      )
            );
        }
    }

    void DrawPlayerStrip(
        const AppState& app,
        Rectangle bounds
    )
    {
        const RouletteClientState& roulette =
            app.roulette;

        float x =
            bounds.x + 18.0f;

        for (
            int i = 0;
            i < (int)roulette.players.size();
            i++
        )
        {
            const RoulettePlayerClientState& player =
                roulette.players[i];

            Rectangle seat = {
                x,
                bounds.y + 53.0f,
                112.0f,
                42.0f
            };

            bool you =
                player.name ==
                app.username;

            DrawRectangleRounded(
                seat,
                0.08f,
                6,
                you
                    ? Color{45, 47, 58, 255}
                    : PANEL_ALT
            );

            DrawRectangleRoundedLinesEx(
                seat,
                0.08f,
                6,
                1.0f,
                player.ready
                    ? SUCCESS
                    : PANEL_LIGHT
            );

            DrawText(
                player.name.c_str(),
                (int)seat.x + 7,
                (int)seat.y + 6,
                12,
                you
                    ? JENG_YELLOW
                    : TEXT_MAIN
            );

            string info =
                to_string(player.chips) +
                " | bet " +
                to_string(player.totalBet);

            DrawText(
                info.c_str(),
                (int)seat.x + 7,
                (int)seat.y + 24,
                10,
                TEXT_MUTED
            );

            x +=
                seat.width +
                7.0f;
        }
    }
}

void DrawRoulettePanel(
    AppState& app,
    Rectangle bounds,
    bool interactionsBlocked
)
{
    RouletteClientState& roulette =
        app.roulette;

    DrawText(
        "ROULETTE",
        (int)bounds.x + 22,
        (int)bounds.y + 15,
        25,
        JENG_RED
    );

    DrawText(
        "EUROPEAN TABLE",
        (int)bounds.x + 160,
        (int)bounds.y + 22,
        13,
        TEXT_MUTED
    );

    Rectangle homeButton = {
        bounds.x + bounds.width - 103,
        bounds.y + 12,
        82,
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
        app.gameView =
            GameView::HOME;
    }

    // ========================================================
    // NO TABLE YET / FINISHED TABLE
    // ========================================================

    if (
        roulette.phase == "WAITING" ||
        (
            !roulette.active &&
            roulette.phase != "ENDED"
        )
    )
    {
        Rectangle intro = {
            bounds.x + 28,
            bounds.y + 82,
            bounds.width - 56,
            390
        };

        DrawRectangleRounded(
            intro,
            0.025f,
            8,
            Color{18, 78, 54, 255}
        );

        DrawRectangleRoundedLinesEx(
            intro,
            0.025f,
            8,
            1.0f,
            JENG_YELLOW
        );

        DrawText(
            "Create a Roulette table.",
            (int)intro.x + 28,
            (int)intro.y + 32,
            20,
            TEXT_MAIN
        );

        DrawText(
            "European wheel: 0-36. The server selects every winning number.",
            (int)intro.x + 28,
            (int)intro.y + 67,
            13,
            TEXT_MUTED
        );

        DrawText(
            "Invite up to 5 other players, place bets with the mouse, then spin.",
            (int)intro.x + 28,
            (int)intro.y + 90,
            13,
            TEXT_MUTED
        );

        Rectangle createButton = {
            intro.x + 28,
            intro.y + 134,
            176,
            45
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
                "CREATE ROULETTE",
                "Choose your table settings.",
                "/roulettecreate",
                {
                    "Starting chips",
                    "Number of rounds"
                }
            );
        }

        DrawText(
            "Payouts",
            (int)intro.x + 28,
            (int)intro.y + 220,
            16,
            JENG_YELLOW
        );

        DrawText(
            "Straight number 35:1  |  Red/Black, Odd/Even, Low/High 1:1  |  Dozens 2:1",
            (int)intro.x + 28,
            (int)intro.y + 248,
            12,
            TEXT_MUTED
        );

        return;
    }

    if (roulette.phase == "ENDED")
    {
        Rectangle endPanel = {
            bounds.x + 50,
            bounds.y + 100,
            bounds.width - 100,
            300
        };

        DrawRectangleRounded(
            endPanel,
            0.03f,
            8,
            PANEL_ALT
        );

        DrawText(
            "ROULETTE TABLE COMPLETE",
            (int)endPanel.x + 26,
            (int)endPanel.y + 25,
            21,
            JENG_YELLOW
        );

        DrawText(
            roulette.status.c_str(),
            (int)endPanel.x + 26,
            (int)endPanel.y + 67,
            14,
            TEXT_MAIN
        );

        if (roulette.lastResult >= 0)
        {
            string result =
                "Final winning number: " +
                to_string(roulette.lastResult);

            DrawText(
                result.c_str(),
                (int)endPanel.x + 26,
                (int)endPanel.y + 100,
                17,
                JENG_RED
            );
        }

        Rectangle exitButton = {
            endPanel.x + 26,
            endPanel.y + 188,
            142,
            42
        };

        if (
            !interactionsBlocked &&
            DrawButton(
                exitButton,
                "EXIT TABLE",
                PANEL_LIGHT,
                JENG_RED,
                TEXT_MAIN,
                14
            )
        )
        {
            app.roulette =
                RouletteClientState{};

            app.gameView =
                GameView::HOME;
        }

        return;
    }

    // ========================================================
    // ACTIVE TABLE HEADER
    // ========================================================

    DrawPlayerStrip(
        app,
        bounds
    );

    // ========================================================
    // LOBBY
    // ========================================================

    if (roulette.phase == "LOBBY")
    {
        Rectangle lobby = {
            bounds.x + 28,
            bounds.y + 115,
            bounds.width - 56,
            360
        };

        DrawRectangleRounded(
            lobby,
            0.025f,
            8,
            PANEL_ALT
        );

        DrawText(
            "ROULETTE LOBBY",
            (int)lobby.x + 24,
            (int)lobby.y + 20,
            19,
            JENG_YELLOW
        );

        string countText =
            to_string(
                roulette.players.size()
            ) +
            " / 6 PLAYERS";

        DrawText(
            countText.c_str(),
            (int)lobby.x + 24,
            (int)lobby.y + 51,
            13,
            TEXT_MUTED
        );

        DrawText(
            roulette.status.c_str(),
            (int)lobby.x + 24,
            (int)lobby.y + 83,
            14,
            TEXT_MAIN
        );

        bool host =
            roulette.hostName ==
            app.username;

        Rectangle inviteButton = {
            lobby.x + 24,
            lobby.y + 126,
            155,
            42
        };

        Rectangle startButton = {
            lobby.x + 193,
            lobby.y + 126,
            145,
            42
        };

        Rectangle leaveButton = {
            lobby.x + 352,
            lobby.y + 126,
            120,
            42
        };

        if (
            host &&
            roulette.players.size() < 6 &&
            !interactionsBlocked &&
            DrawButton(
                inviteButton,
                "INVITE PLAYERS",
                JENG_RED,
                Color{255,80,80,255},
                WHITE,
                13
            )
        )
        {
            OpenPlayerInvite(app, GameView::ROULETTE);
        }

        if (
            host &&
            !interactionsBlocked &&
            DrawButton(
                startButton,
                "START TABLE",
                SUCCESS,
                Color{90,235,140,255},
                BG,
                13
            )
        )
        {
            NetSendLine("RLT_START");
        }

        if (
            !interactionsBlocked &&
            DrawButton(
                leaveButton,
                host ? "CLOSE" : "LEAVE",
                PANEL_LIGHT,
                JENG_RED,
                TEXT_MAIN,
                13
            )
        )
        {
            NetSendLine("RLT_LEAVE");
        }

        return;
    }

    // ========================================================
    // BETTING / RESULT TABLE
    // ========================================================

    Vector2 wheelCenter = {
        bounds.x + 165.0f,
        bounds.y + 285.0f
    };

    float wheelRadius =
        122.0f;

    DrawWheel(
        roulette,
        wheelCenter,
        wheelRadius
    );

    Rectangle board = {
        bounds.x + 320.0f,
        bounds.y + 118.0f,
        bounds.width - 342.0f,
        292.0f
    };

    DrawRectangleRounded(
        board,
        0.025f,
        8,
        Color{18, 78, 54, 255}
    );

    bool canBet =
        !interactionsBlocked &&
        roulette.phase == "BETTING" &&
        !roulette.animating &&
        !YouAreReady(app);

    // 0 cell
    const float zeroWidth = 35.0f;
    const float cellW =
        (board.width - zeroWidth - 12.0f) /
        12.0f;

    const float cellH = 39.0f;

    Rectangle zeroRect = {
        board.x + 6.0f,
        board.y + 8.0f,
        zeroWidth,
        cellH * 3.0f
    };

    if (
        DrawBetSpot(
            zeroRect,
            "0",
            Color{30,150,85,255},
            canBet,
            BetAmount(app, "NUMBER", 0),
            14
        )
    )
    {
        AddBet(app, "NUMBER", 0);
    }

    float numberStartX =
        zeroRect.x +
        zeroRect.width +
        5.0f;

    for (int col = 0; col < 12; col++)
    {
        for (int row = 0; row < 3; row++)
        {
            int number =
                col * 3 +
                (3 - row);

            Rectangle cell = {
                numberStartX +
                    col * cellW,
                board.y +
                    8.0f +
                    row * cellH,
                cellW,
                cellH
            };

            if (
                DrawBetSpot(
                    cell,
                    to_string(number),
                    RouletteNumberColor(number),
                    canBet,
                    BetAmount(
                        app,
                        "NUMBER",
                        number
                    ),
                    11
                )
            )
            {
                AddBet(
                    app,
                    "NUMBER",
                    number
                );
            }
        }
    }

    float outsideY =
        board.y +
        8.0f +
        cellH * 3.0f +
        12.0f;

    const float outsideGap = 5.0f;
    const float outsideW =
        (
            board.width -
            12.0f -
            outsideGap * 5.0f
        ) /
        6.0f;

    struct OutsideBet
    {
        const char* type;
        const char* label;
        Color color;
    };

    OutsideBet outsideBets[] = {
        {"LOW", "1-18", PANEL_LIGHT},
        {"EVEN", "EVEN", PANEL_LIGHT},
        {"RED", "RED", Color{190,50,50,255}},
        {"BLACK", "BLACK", Color{24,26,30,255}},
        {"ODD", "ODD", PANEL_LIGHT},
        {"HIGH", "19-36", PANEL_LIGHT}
    };

    for (int i = 0; i < 6; i++)
    {
        Rectangle spot = {
            board.x +
                6.0f +
                i *
                (
                    outsideW +
                    outsideGap
                ),
            outsideY,
            outsideW,
            37.0f
        };

        if (
            DrawBetSpot(
                spot,
                outsideBets[i].label,
                outsideBets[i].color,
                canBet,
                BetAmount(
                    app,
                    outsideBets[i].type,
                    0
                ),
                10
            )
        )
        {
            AddBet(
                app,
                outsideBets[i].type,
                0
            );
        }
    }

    float dozenY =
        outsideY +
        46.0f;

    const float dozenW =
        (
            board.width -
            22.0f
        ) /
        3.0f;

    const char* dozenTypes[] = {
        "DOZEN1",
        "DOZEN2",
        "DOZEN3"
    };

    const char* dozenLabels[] = {
        "1ST 12",
        "2ND 12",
        "3RD 12"
    };

    for (int i = 0; i < 3; i++)
    {
        Rectangle spot = {
            board.x +
                6.0f +
                i *
                (
                    dozenW +
                    5.0f
                ),
            dozenY,
            dozenW,
            38.0f
        };

        if (
            DrawBetSpot(
                spot,
                dozenLabels[i],
                Color{32, 92, 66, 255},
                canBet,
                BetAmount(
                    app,
                    dozenTypes[i],
                    0
                ),
                11
            )
        )
        {
            AddBet(
                app,
                dozenTypes[i],
                0
            );
        }
    }

    // ========================================================
    // CHIP SELECTION + ACTIONS
    // ========================================================

    float controlsY =
        bounds.y +
        bounds.height -
        76.0f;

    DrawText(
        "CHIP",
        (int)bounds.x + 24,
        (int)controlsY - 21,
        11,
        TEXT_MUTED
    );

    int chipValues[] = {
        10,
        25,
        50,
        100
    };

    for (int i = 0; i < 4; i++)
    {
        Rectangle chipButton = {
            bounds.x +
                24.0f +
                i * 56.0f,
            controlsY,
            49.0f,
            34.0f
        };

        bool selected =
            roulette.chipAmount ==
            chipValues[i];

        if (
            DrawAction(
                chipButton,
                to_string(chipValues[i]).c_str(),
                canBet,
                selected
                    ? JENG_YELLOW
                    : PANEL_LIGHT,
                selected
                    ? Color{255,220,90,255}
                    : JENG_RED,
                selected ? BG : TEXT_MAIN,
                11
            )
        )
        {
            roulette.chipAmount =
                chipValues[i];
        }
    }

    int chips =
        YourChips(app);

    int totalBet =
        YourTotalBet(app);

    string bankroll =
        "CHIPS " +
        to_string(chips) +
        "   BET " +
        to_string(totalBet);

    DrawText(
        bankroll.c_str(),
        (int)bounds.x + 262,
        (int)controlsY + 10,
        12,
        TEXT_MAIN
    );

    Rectangle clearButton = {
        bounds.x + bounds.width - 270,
        controlsY,
        75,
        34
    };

    Rectangle readyButton = {
        bounds.x + bounds.width - 185,
        controlsY,
        75,
        34
    };

    Rectangle leaveButton = {
        bounds.x + bounds.width - 100,
        controlsY,
        78,
        34
    };

    if (
        DrawAction(
            clearButton,
            "CLEAR",
            canBet &&
                totalBet > 0,
            PANEL_LIGHT,
            JENG_RED,
            TEXT_MAIN,
            11
        )
    )
    {
        NetSendLine(
            "RLT_CLEAR"
        );
    }

    if (
        DrawAction(
            readyButton,
            YouAreReady(app)
                ? "READY"
                : "LOCK",
            canBet &&
                totalBet > 0,
            YouAreReady(app)
                ? SUCCESS
                : JENG_RED,
            Color{90,235,140,255},
            YouAreReady(app)
                ? BG
                : WHITE,
            11
        )
    )
    {
        NetSendLine(
            "RLT_READY"
        );
    }

    if (
        DrawAction(
            leaveButton,
            roulette.hostName == app.username
                ? "CLOSE"
                : "LEAVE",
            !interactionsBlocked &&
                !roulette.animating,
            PANEL_LIGHT,
            JENG_RED,
            TEXT_MAIN,
            11
        )
    )
    {
        NetSendLine(
            "RLT_LEAVE"
        );
    }

    // ========================================================
    // STATUS + NEXT ROUND
    // ========================================================

    string roundText =
        "ROUND " +
        to_string(
            roulette.currentRound
        ) +
        " / " +
        to_string(
            roulette.totalRounds
        );

    DrawText(
        roundText.c_str(),
        (int)bounds.x + 24,
        (int)bounds.y + 104,
        12,
        JENG_YELLOW
    );

    DrawText(
        roulette.status.c_str(),
        (int)bounds.x + 24,
        (int)(controlsY - 50),
        12,
        TEXT_MUTED
    );

    if (
        roulette.phase == "RESULT" &&
        !roulette.animating &&
        roulette.hostName == app.username &&
        roulette.currentRound <
            roulette.totalRounds
    )
    {
        Rectangle nextButton = {
            bounds.x + bounds.width - 188,
            controlsY - 45,
            166,
            34
        };

        if (
            !interactionsBlocked &&
            DrawButton(
                nextButton,
                "NEXT ROUND",
                SUCCESS,
                Color{90,235,140,255},
                BG,
                12
            )
        )
        {
            NetSendLine(
                "RLT_NEXT"
            );
        }
    }

    if (
        roulette.phase == "RESULT" &&
        !roulette.payoutMessages.empty()
    )
    {
        float y =
            board.y +
            board.height +
            10.0f;

        for (
            int i = 0;
            i < (int)roulette.payoutMessages.size() &&
            i < 3;
            i++
        )
        {
            DrawText(
                roulette.payoutMessages[i].c_str(),
                (int)board.x,
                (int)y,
                10,
                TEXT_MUTED
            );

            y += 15.0f;
        }
    }
}
