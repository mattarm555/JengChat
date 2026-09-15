#include "chess.h"
#include "chess_pieces.h"

#include "../networking.h"
#include "../theme.h"
#include "../ui/command_popup.h"
#include "../ui/ui_common.h"

#include <array>
#include <cctype>
#include <map>
#include <string>
#include <vector>

using namespace std;

namespace
{
    bool IsWhitePiece(char piece)
    {
        return piece >= 'A' && piece <= 'Z';
    }

    bool IsBlackPiece(char piece)
    {
        return piece >= 'a' && piece <= 'z';
    }

    bool IsUsersPiece(const ChessClientState& chess, char piece)
    {
        if (chess.yourColor == "WHITE")
            return IsWhitePiece(piece);

        if (chess.yourColor == "BLACK")
            return IsBlackPiece(piece);

        return false;
    }

    string SquareName(int row, int col)
    {
        string result;
        result += (char)('a' + col);
        result += (char)('8' - row);
        return result;
    }

    int BoardIndexForDisplay(
        const ChessClientState& chess,
        int displayRow,
        int displayCol
    )
    {
        bool flip = chess.yourColor == "BLACK";

        int row = flip ? 7 - displayRow : displayRow;
        int col = flip ? 7 - displayCol : displayCol;

        return row * 8 + col;
    }

    void ActualRowCol(
        const ChessClientState& chess,
        int displayRow,
        int displayCol,
        int& row,
        int& col
    )
    {
        bool flip = chess.yourColor == "BLACK";
        row = flip ? 7 - displayRow : displayRow;
        col = flip ? 7 - displayCol : displayCol;
    }

    vector<char> MissingPieces(
        const string& board,
        bool whitePieces
    )
    {
        map<char, int> starting = {
            {'P', 8}, {'N', 2}, {'B', 2}, {'R', 2}, {'Q', 1}, {'K', 1},
            {'p', 8}, {'n', 2}, {'b', 2}, {'r', 2}, {'q', 1}, {'k', 1}
        };

        map<char, int> current;

        for (char piece : board)
        {
            if (piece != '.')
                current[piece]++;
        }

        const array<char, 5> whiteOrder = {'Q', 'R', 'B', 'N', 'P'};
        const array<char, 5> blackOrder = {'q', 'r', 'b', 'n', 'p'};
        const auto& order = whitePieces ? whiteOrder : blackOrder;

        vector<char> missing;

        for (char piece : order)
        {
            int count = starting[piece] - current[piece];

            for (int i = 0; i < count; i++)
                missing.push_back(piece);
        }

        return missing;
    }

    void DrawCapturedRow(
        const vector<char>& pieces,
        float x,
        float y,
        const string& label
    )
    {
        DrawText(
            label.c_str(),
            (int)x,
            (int)y,
            13,
            TEXT_MUTED
        );

        float pieceX = x + 120.0f;

        for (char piece : pieces)
        {
            Rectangle slot = {
                pieceX,
                y - 5.0f,
                26.0f,
                26.0f
            };

            DrawChessPiece(piece, slot, 1.0f);
            pieceX += 22.0f;
        }

        if (pieces.empty())
        {
            DrawText(
                "-",
                (int)pieceX,
                (int)y,
                13,
                TEXT_MUTED
            );
        }
    }
}


void DrawChessPanel(
    AppState& app,
    Rectangle bounds,
    bool interactionsBlocked
)
{
    ChessClientState& chess = app.chess;

    DrawText(
        "CHESS",
        (int)bounds.x + 22,
        (int)bounds.y + 15,
        26,
        JENG_RED
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

    const float square = 48.0f;
    const float boardSize = square * 8.0f;

    Rectangle board = {
        bounds.x + 28,
        bounds.y + 108,
        boardSize,
        boardSize
    };

    string opponentName;

    if (chess.yourColor == "WHITE")
        opponentName = chess.blackPlayer;
    else if (chess.yourColor == "BLACK")
        opponentName = chess.whitePlayer;

    vector<char> whiteMissing = MissingPieces(chess.board, true);
    vector<char> blackMissing = MissingPieces(chess.board, false);

    vector<char> youCaptured;
    vector<char> opponentCaptured;

    if (chess.yourColor == "WHITE")
    {
        youCaptured = blackMissing;
        opponentCaptured = whiteMissing;
    }
    else if (chess.yourColor == "BLACK")
    {
        youCaptured = whiteMissing;
        opponentCaptured = blackMissing;
    }

    DrawCapturedRow(
        opponentCaptured,
        board.x,
        board.y - 31,
        opponentName.empty() ? "OPPONENT WON" : opponentName + " WON"
    );

    // ========================================================
    // MOUSE-DRIVEN MOVE INPUT
    // ========================================================

    if (
        chess.active &&
        !interactionsBlocked &&
        IsMouseInside(board) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
    )
    {
        Vector2 mouse = GetUIMousePosition();

        int displayCol = (int)((mouse.x - board.x) / square);
        int displayRow = (int)((mouse.y - board.y) / square);

        if (
            displayRow >= 0 && displayRow < 8 &&
            displayCol >= 0 && displayCol < 8
        )
        {
            int row, col;
            ActualRowCol(chess, displayRow, displayCol, row, col);

            int clickedIndex = row * 8 + col;
            char clickedPiece = chess.board[clickedIndex];

            if (chess.selectedSquare < 0)
            {
                if (chess.turn != app.username)
                {
                    chess.status = "Wait for your turn.";
                }
                else if (IsUsersPiece(chess, clickedPiece))
                {
                    chess.selectedSquare = clickedIndex;
                    chess.status = "Choose a destination square.";
                }
            }
            else
            {
                if (IsUsersPiece(chess, clickedPiece))
                {
                    chess.selectedSquare = clickedIndex;
                    chess.status = "Choose a destination square.";
                }
                else
                {
                    int fromRow = chess.selectedSquare / 8;
                    int fromCol = chess.selectedSquare % 8;

                    string from = SquareName(fromRow, fromCol);
                    string to = SquareName(row, col);

                    string packet = "CHESS_MOVE|" + from + "|" + to;

                    // Queen promotion by default for the first graphical pass.
                    char movingPiece = chess.board[chess.selectedSquare];

                    if (
                        (movingPiece == 'P' && row == 0) ||
                        (movingPiece == 'p' && row == 7)
                    )
                    {
                        packet += "|q";
                    }

                    if (!NetSendLine(packet))
                        chess.status = NetLastError();
                    else
                        chess.status = "Move sent...";

                    chess.selectedSquare = -1;
                }
            }
        }
    }

    // ========================================================
    // BOARD
    // ========================================================

    for (int displayRow = 0; displayRow < 8; displayRow++)
    {
        for (int displayCol = 0; displayCol < 8; displayCol++)
        {
            Rectangle cell = {
                board.x + displayCol * square,
                board.y + displayRow * square,
                square,
                square
            };

            Color color = ((displayRow + displayCol) % 2 == 0)
                ? BOARD_LIGHT
                : BOARD_DARK;

            DrawRectangleRec(cell, color);

            int actualIndex = BoardIndexForDisplay(
                chess,
                displayRow,
                displayCol
            );

            if (actualIndex == chess.selectedSquare)
            {
                DrawRectangle(
                    (int)cell.x,
                    (int)cell.y,
                    (int)cell.width,
                    (int)cell.height,
                    Color{245, 205, 66, 55}
                );

                DrawRectangleLinesEx(cell, 3.0f, JENG_YELLOW);
            }

            if (actualIndex >= 0 && actualIndex < (int)chess.board.size())
            {
                char piece = chess.board[actualIndex];

                if (piece != '.')
                    DrawChessPiece(piece, cell, 4.0f);
            }
        }
    }

    // Coordinates follow board orientation.
    bool flip = chess.yourColor == "BLACK";

    for (int displayCol = 0; displayCol < 8; displayCol++)
    {
        int actualCol = flip ? 7 - displayCol : displayCol;
        char label[2] = {(char)('a' + actualCol), '\0'};

        DrawText(
            label,
            (int)(board.x + displayCol * square + square / 2 - 4),
            (int)board.y + (int)board.height + 4,
            13,
            TEXT_MUTED
        );
    }

    for (int displayRow = 0; displayRow < 8; displayRow++)
    {
        int actualRow = flip ? 7 - displayRow : displayRow;
        char label[2] = {(char)('8' - actualRow), '\0'};

        DrawText(
            label,
            (int)board.x - 15,
            (int)(board.y + displayRow * square + square / 2 - 7),
            13,
            TEXT_MUTED
        );
    }

    DrawCapturedRow(
        youCaptured,
        board.x,
        board.y + board.height + 27,
        "YOU WON"
    );

    // ========================================================
    // RIGHT-SIDE MATCH INFORMATION
    // ========================================================

    float infoX = board.x + board.width + 28;

    DrawText("MATCH", (int)infoX, (int)board.y, 16, JENG_YELLOW);

    if (!chess.whitePlayer.empty())
    {
        string whiteText = "White: " + chess.whitePlayer;
        string blackText = "Black: " + chess.blackPlayer;

        DrawText(whiteText.c_str(), (int)infoX, (int)board.y + 29, 14, TEXT_MAIN);
        DrawText(blackText.c_str(), (int)infoX, (int)board.y + 50, 14, TEXT_MAIN);
    }
    else
    {
        DrawText("No active match.", (int)infoX, (int)board.y + 29, 14, TEXT_MUTED);
    }

    DrawText("TURN", (int)infoX, (int)board.y + 91, 15, JENG_YELLOW);

    string turnText = chess.turn.empty() ? "-" : chess.turn;
    Color turnColor = chess.turn == app.username ? SUCCESS : TEXT_MAIN;

    DrawText(turnText.c_str(), (int)infoX, (int)board.y + 116, 17, turnColor);

    DrawText("STATUS", (int)infoX, (int)board.y + 158, 15, JENG_YELLOW);
    DrawText(chess.status.c_str(), (int)infoX, (int)board.y + 183, 14, TEXT_MUTED);

    if (!chess.active)
    {
        Rectangle challengeButton = {
            infoX,
            board.y + 235,
            170,
            44
        };

        if (
            !interactionsBlocked &&
            DrawButton(
                challengeButton,
                "CHALLENGE",
                JENG_RED,
                Color{255, 80, 80, 255},
                WHITE,
                15
            )
        )
        {
            OpenCommandPrompt(
                app.commandPopup,
                "CHESS",
                "Who do you want to challenge?",
                "/chess",
                {"Opponent username"}
            );
        }
    }
    else
    {
        Rectangle resignButton = {
            infoX,
            board.y + 235,
            170,
            44
        };

        if (
            !interactionsBlocked &&
            DrawButton(
                resignButton,
                "RESIGN",
                PANEL_LIGHT,
                JENG_RED,
                TEXT_MAIN,
                15
            )
        )
        {
            NetSendLine("/resign");
        }
    }
}
