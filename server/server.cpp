#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cctype>
#include <random>
#include <csignal>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

using Socket = SOCKET;

#define CLOSE_SOCKET closesocket
#define INVALID_SOCK INVALID_SOCKET
#define SOCKET_ERR SOCKET_ERROR

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

using Socket = int;

#define CLOSE_SOCKET close
#define INVALID_SOCK -1
#define SOCKET_ERR -1

#endif

using namespace std;

const int PORT = 54000;
const int BUFFER_SIZE = 2048;
const int MAX_BLACKJACK_HANDS = 50;
const int MAX_STARTING_CHIPS = 1000000;
const int MAX_POKER_CHIPS = 1000000;


bool initializeSocketLibrary() {
#ifdef _WIN32
    WSADATA wsaData;

    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsaData
        ) != 0
    ) {
        return false;
    }
#else
    // Prevent a disconnected client from terminating the whole server
    // when send() writes to a closed socket.
    signal(SIGPIPE, SIG_IGN);
#endif

    return true;
}

void cleanupSocketLibrary() {
#ifdef _WIN32
    WSACleanup();
#endif
}

struct Client {
    Socket socket;
    string name;
    string inputBuffer;

    Socket pendingChallenge = INVALID_SOCK;
    string pendingGame;
    int pendingChips = 0;
    int pendingHands = 0;
};

struct TicTacToeGame {
    Socket playerX;
    Socket playerO;

    char board[9] = {
        ' ', ' ', ' ',
        ' ', ' ', ' ',
        ' ', ' ', ' '
    };

    Socket turn;
};

struct Card {
    string rank;
    char suit;
    int value;
};

struct BlackjackGame {
    Socket player1;
    Socket player2;

    int player1Chips;
    int player2Chips;

    int totalHands;
    int currentHand = 1;

    int player1Bet = 0;
    int player2Bet = 0;

    bool player1BetPlaced = false;
    bool player2BetPlaced = false;

    vector<Card> deck;
    vector<Card> player1Hand;
    vector<Card> player2Hand;
    vector<Card> dealerHand;

    bool player1Stood = false;
    bool player2Stood = false;

    bool player1Busted = false;
    bool player2Busted = false;

    bool handInProgress = false;
    Socket turn = INVALID_SOCK;
};

struct ChessGame {
    Socket white;
    Socket black;
    Socket turn;

    // Uppercase pieces are White, lowercase pieces are Black.
    // Rows: 0 = rank 8, 7 = rank 1.
    char board[8][8];

    bool whiteKingMoved = false;
    bool blackKingMoved = false;
    bool whiteARookMoved = false;
    bool whiteHRookMoved = false;
    bool blackARookMoved = false;
    bool blackHRookMoved = false;

    // En-passant destination square, or -1/-1 when unavailable.
    int enPassantRow = -1;
    int enPassantCol = -1;
};


enum class PokerStage {
    PREFLOP,
    FLOP,
    TURN,
    RIVER,
    SHOWDOWN
};

struct PokerGame {
    Socket player1;
    Socket player2;

    int player1Chips = 0;
    int player2Chips = 0;

    int smallBlind = 0;
    int bigBlind = 0;

    Socket dealer = INVALID_SOCK;
    Socket turn = INVALID_SOCK;

    vector<Card> deck;
    vector<Card> player1Hole;
    vector<Card> player2Hole;
    vector<Card> community;

    int pot = 0;
    int player1RoundBet = 0;
    int player2RoundBet = 0;
    int currentBet = 0;
    int lastRaiseSize = 0;

    bool player1Acted = false;
    bool player2Acted = false;

    bool handActive = false;
    int handNumber = 0;
    PokerStage stage = PokerStage::PREFLOP;
};

vector<Client> clients;
vector<TicTacToeGame> ticTacToeGames;
vector<BlackjackGame> blackjackGames;
vector<ChessGame> chessGames;
vector<PokerGame> pokerGames;

static mt19937 rng(random_device{}());


// ============================================================
// NETWORK HELPERS
// ============================================================

bool sendAll(Socket socket, const string& data) {
    int total = 0;

    while (total < (int)data.size()) {
        int sent = send(
            socket,
            data.c_str() + total,
            (int)data.size() - total,
            0
        );

        if (sent == SOCKET_ERR || sent == 0)
            return false;

        total += sent;
    }

    return true;
}

void sendPacket(
    Socket socket,
    const string& type,
    const string& text
) {
    sendAll(
        socket,
        type + "|" + text + "\n"
    );
}

void sendReady(Socket socket) {
    sendPacket(
        socket,
        "READY",
        ""
    );
}

Client* getClient(Socket socket) {
    for (Client& c : clients) {
        if (c.socket == socket)
            return &c;
    }

    return nullptr;
}

string lowerCopy(string value) {
    transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return (char)tolower(ch);
        }
    );

    return value;
}

Client* getClientByName(const string& name) {
    string wanted = lowerCopy(name);

    for (Client& c : clients) {
        if (lowerCopy(c.name) == wanted)
            return &c;
    }

    return nullptr;
}

string getName(Socket socket) {
    Client* c = getClient(socket);

    if (c)
        return c->name;

    return "Unknown";
}

void clearPendingChallenge(Client& client) {
    client.pendingChallenge = INVALID_SOCK;
    client.pendingGame.clear();
    client.pendingChips = 0;
    client.pendingHands = 0;
}

void broadcastSystem(const string& message) {
    for (Client& c : clients) {
        if (!c.name.empty()) {
            sendPacket(
                c.socket,
                "SYS",
                message
            );

            sendReady(c.socket);
        }
    }
}

void broadcastChat(
    const string& name,
    const string& message
) {
    string packet =
        "CHAT|" +
        name +
        "|" +
        message +
        "\n";

    for (Client& c : clients) {
        if (!c.name.empty()) {
            sendAll(
                c.socket,
                packet
            );

            sendReady(c.socket);
        }
    }
}


// ============================================================
// GAME LOOKUPS
// ============================================================

int findTicTacToeGame(Socket socket) {
    for (int i = 0; i < (int)ticTacToeGames.size(); i++) {
        if (
            ticTacToeGames[i].playerX == socket ||
            ticTacToeGames[i].playerO == socket
        ) {
            return i;
        }
    }

    return -1;
}

int findBlackjackGame(Socket socket) {
    for (int i = 0; i < (int)blackjackGames.size(); i++) {
        if (
            blackjackGames[i].player1 == socket ||
            blackjackGames[i].player2 == socket
        ) {
            return i;
        }
    }

    return -1;
}

int findChessGame(Socket socket) {
    for (int i = 0; i < (int)chessGames.size(); i++) {
        if (
            chessGames[i].white == socket ||
            chessGames[i].black == socket
        ) {
            return i;
        }
    }

    return -1;
}

int findPokerGame(Socket socket) {
    for (int i = 0; i < (int)pokerGames.size(); i++) {
        if (
            pokerGames[i].player1 == socket ||
            pokerGames[i].player2 == socket
        ) {
            return i;
        }
    }

    return -1;
}

bool isPlayerBusy(Socket socket) {
    return
        findTicTacToeGame(socket) != -1 ||
        findBlackjackGame(socket) != -1 ||
        findChessGame(socket) != -1 ||
        findPokerGame(socket) != -1;
}


// ============================================================
// TIC-TAC-TOE
// ============================================================

void sendTicTacToeLine(
    TicTacToeGame& game,
    const string& text
) {
    sendPacket(
        game.playerX,
        "GAME",
        text
    );

    sendPacket(
        game.playerO,
        "GAME",
        text
    );
}

char displayCell(TicTacToeGame& game, int index) {
    if (game.board[index] != ' ')
        return game.board[index];

    return '1' + index;
}

void showTicTacToeBoard(
    TicTacToeGame& game,
    bool showTurn = true
) {
    sendTicTacToeLine(game, "");
    sendTicTacToeLine(game, "========== TIC-TAC-TOE ==========");

    sendTicTacToeLine(
        game,
        getName(game.playerX) +
        " (X) vs " +
        getName(game.playerO) +
        " (O)"
    );

    sendTicTacToeLine(game, "");

    string row1;
    row1 += " ";
    row1 += displayCell(game, 0);
    row1 += " | ";
    row1 += displayCell(game, 1);
    row1 += " | ";
    row1 += displayCell(game, 2);

    string row2;
    row2 += " ";
    row2 += displayCell(game, 3);
    row2 += " | ";
    row2 += displayCell(game, 4);
    row2 += " | ";
    row2 += displayCell(game, 5);

    string row3;
    row3 += " ";
    row3 += displayCell(game, 6);
    row3 += " | ";
    row3 += displayCell(game, 7);
    row3 += " | ";
    row3 += displayCell(game, 8);

    sendTicTacToeLine(game, row1);
    sendTicTacToeLine(game, "---+---+---");
    sendTicTacToeLine(game, row2);
    sendTicTacToeLine(game, "---+---+---");
    sendTicTacToeLine(game, row3);
    sendTicTacToeLine(game, "");

    if (showTurn) {
        string symbol =
            game.turn == game.playerX
            ? "X"
            : "O";

        sendTicTacToeLine(
            game,
            "Turn: " +
            getName(game.turn) +
            " (" +
            symbol +
            ")"
        );
    }

    sendTicTacToeLine(game, "================================");
    sendTicTacToeLine(game, "");
}

bool ticTacToeWinner(
    TicTacToeGame& game,
    char symbol
) {
    int combinations[8][3] = {
        {0,1,2},
        {3,4,5},
        {6,7,8},
        {0,3,6},
        {1,4,7},
        {2,5,8},
        {0,4,8},
        {2,4,6}
    };

    for (auto& combo : combinations) {
        if (
            game.board[combo[0]] == symbol &&
            game.board[combo[1]] == symbol &&
            game.board[combo[2]] == symbol
        ) {
            return true;
        }
    }

    return false;
}

bool ticTacToeBoardFull(TicTacToeGame& game) {
    for (char cell : game.board) {
        if (cell == ' ')
            return false;
    }

    return true;
}


// ============================================================
// CHESS HELPERS
// ============================================================

bool chessInside(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

bool chessWhitePiece(char piece) {
    return piece >= 'A' && piece <= 'Z';
}

bool chessBlackPiece(char piece) {
    return piece >= 'a' && piece <= 'z';
}

bool chessSameColor(char a, char b) {
    if (a == ' ' || b == ' ')
        return false;

    return
        (chessWhitePiece(a) && chessWhitePiece(b)) ||
        (chessBlackPiece(a) && chessBlackPiece(b));
}

string chessPieceSymbol(char piece) {
    switch (piece) {
        case 'K': return "♔";
        case 'Q': return "♕";
        case 'R': return "♖";
        case 'B': return "♗";
        case 'N': return "♘";
        case 'P': return "♙";
        case 'k': return "♚";
        case 'q': return "♛";
        case 'r': return "♜";
        case 'b': return "♝";
        case 'n': return "♞";
        case 'p': return "♟";
        default:  return " ";
    }
}

bool parseChessSquare(
    string square,
    int& row,
    int& col
) {
    if (square.size() != 2)
        return false;

    char file = (char)tolower((unsigned char)square[0]);
    char rank = square[1];

    if (file < 'a' || file > 'h' || rank < '1' || rank > '8')
        return false;

    col = file - 'a';
    row = 8 - (rank - '0');

    return true;
}

string chessSquareName(int row, int col) {
    if (!chessInside(row, col))
        return "??";

    string square;
    square += (char)('a' + col);
    square += (char)('8' - row);
    return square;
}

void sendChessLine(
    ChessGame& game,
    const string& text
) {
    sendPacket(game.white, "GAME", text);
    sendPacket(game.black, "GAME", text);
}

void readyChessPlayers(ChessGame& game) {
    sendReady(game.white);
    sendReady(game.black);
}

ChessGame makeChessGame(
    Socket white,
    Socket black
) {
    ChessGame game{};
    game.white = white;
    game.black = black;
    game.turn = white;
    game.enPassantRow = -1;
    game.enPassantCol = -1;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            game.board[row][col] = ' ';
        }
    }

    const string blackBack = "rnbqkbnr";
    const string whiteBack = "RNBQKBNR";

    for (int col = 0; col < 8; col++) {
        game.board[0][col] = blackBack[col];
        game.board[1][col] = 'p';
        game.board[6][col] = 'P';
        game.board[7][col] = whiteBack[col];
    }

    return game;
}

bool findChessKing(
    const ChessGame& game,
    bool white,
    int& kingRow,
    int& kingCol
) {
    char king = white ? 'K' : 'k';

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (game.board[row][col] == king) {
                kingRow = row;
                kingCol = col;
                return true;
            }
        }
    }

    return false;
}

bool chessSquareAttacked(
    const ChessGame& game,
    int row,
    int col,
    bool byWhite
) {
    // Pawns.
    int pawnSourceRow = row + (byWhite ? 1 : -1);
    char pawn = byWhite ? 'P' : 'p';

    for (int dc : {-1, 1}) {
        int sourceCol = col + dc;

        if (
            chessInside(pawnSourceRow, sourceCol) &&
            game.board[pawnSourceRow][sourceCol] == pawn
        ) {
            return true;
        }
    }

    // Knights.
    const int knightMoves[8][2] = {
        {-2,-1}, {-2, 1}, {-1,-2}, {-1, 2},
        { 1,-2}, { 1, 2}, { 2,-1}, { 2, 1}
    };

    char knight = byWhite ? 'N' : 'n';

    for (const auto& move : knightMoves) {
        int r = row + move[0];
        int c = col + move[1];

        if (
            chessInside(r, c) &&
            game.board[r][c] == knight
        ) {
            return true;
        }
    }

    // Kings.
    char king = byWhite ? 'K' : 'k';

    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0)
                continue;

            int r = row + dr;
            int c = col + dc;

            if (
                chessInside(r, c) &&
                game.board[r][c] == king
            ) {
                return true;
            }
        }
    }

    // Rooks / queens.
    const int straight[4][2] = {
        {-1,0}, {1,0}, {0,-1}, {0,1}
    };

    for (const auto& direction : straight) {
        int r = row + direction[0];
        int c = col + direction[1];

        while (chessInside(r, c)) {
            char piece = game.board[r][c];

            if (piece != ' ') {
                if (
                    (byWhite && chessWhitePiece(piece)) ||
                    (!byWhite && chessBlackPiece(piece))
                ) {
                    char type = (char)tolower((unsigned char)piece);
                    if (type == 'r' || type == 'q')
                        return true;
                }

                break;
            }

            r += direction[0];
            c += direction[1];
        }
    }

    // Bishops / queens.
    const int diagonal[4][2] = {
        {-1,-1}, {-1,1}, {1,-1}, {1,1}
    };

    for (const auto& direction : diagonal) {
        int r = row + direction[0];
        int c = col + direction[1];

        while (chessInside(r, c)) {
            char piece = game.board[r][c];

            if (piece != ' ') {
                if (
                    (byWhite && chessWhitePiece(piece)) ||
                    (!byWhite && chessBlackPiece(piece))
                ) {
                    char type = (char)tolower((unsigned char)piece);
                    if (type == 'b' || type == 'q')
                        return true;
                }

                break;
            }

            r += direction[0];
            c += direction[1];
        }
    }

    return false;
}

bool chessInCheck(
    const ChessGame& game,
    bool white
) {
    int kingRow = -1;
    int kingCol = -1;

    if (!findChessKing(game, white, kingRow, kingCol))
        return true;

    return chessSquareAttacked(
        game,
        kingRow,
        kingCol,
        !white
    );
}

bool chessPathClear(
    const ChessGame& game,
    int fromRow,
    int fromCol,
    int toRow,
    int toCol
) {
    int rowStep =
        (toRow > fromRow) ? 1 :
        (toRow < fromRow) ? -1 : 0;

    int colStep =
        (toCol > fromCol) ? 1 :
        (toCol < fromCol) ? -1 : 0;

    int row = fromRow + rowStep;
    int col = fromCol + colStep;

    while (row != toRow || col != toCol) {
        if (game.board[row][col] != ' ')
            return false;

        row += rowStep;
        col += colStep;
    }

    return true;
}

bool chessPseudoLegalMove(
    const ChessGame& game,
    int fromRow,
    int fromCol,
    int toRow,
    int toCol,
    bool white
) {
    if (
        !chessInside(fromRow, fromCol) ||
        !chessInside(toRow, toCol) ||
        (fromRow == toRow && fromCol == toCol)
    ) {
        return false;
    }

    char piece = game.board[fromRow][fromCol];
    char target = game.board[toRow][toCol];

    if (piece == ' ')
        return false;

    if (white && !chessWhitePiece(piece))
        return false;

    if (!white && !chessBlackPiece(piece))
        return false;

    if (chessSameColor(piece, target))
        return false;

    // Kings are never captured directly. Check/checkmate ends the game.
    if (target == 'K' || target == 'k')
        return false;

    int dr = toRow - fromRow;
    int dc = toCol - fromCol;

    switch ((char)tolower((unsigned char)piece)) {
        case 'p': {
            int direction = white ? -1 : 1;
            int startRow = white ? 6 : 1;

            if (dc == 0 && dr == direction && target == ' ')
                return true;

            if (
                dc == 0 &&
                dr == 2 * direction &&
                fromRow == startRow &&
                target == ' ' &&
                game.board[fromRow + direction][fromCol] == ' '
            ) {
                return true;
            }

            if (abs(dc) == 1 && dr == direction) {
                if (target != ' ')
                    return true;

                if (
                    toRow == game.enPassantRow &&
                    toCol == game.enPassantCol
                ) {
                    char adjacent = game.board[fromRow][toCol];
                    return adjacent == (white ? 'p' : 'P');
                }
            }

            return false;
        }

        case 'n':
            return
                (abs(dr) == 2 && abs(dc) == 1) ||
                (abs(dr) == 1 && abs(dc) == 2);

        case 'b':
            return
                abs(dr) == abs(dc) &&
                chessPathClear(
                    game,
                    fromRow,
                    fromCol,
                    toRow,
                    toCol
                );

        case 'r':
            return
                (dr == 0 || dc == 0) &&
                chessPathClear(
                    game,
                    fromRow,
                    fromCol,
                    toRow,
                    toCol
                );

        case 'q':
            return
                (
                    dr == 0 ||
                    dc == 0 ||
                    abs(dr) == abs(dc)
                ) &&
                chessPathClear(
                    game,
                    fromRow,
                    fromCol,
                    toRow,
                    toCol
                );

        case 'k': {
            if (abs(dr) <= 1 && abs(dc) <= 1)
                return true;

            // Castling.
            int homeRow = white ? 7 : 0;

            if (
                fromRow != homeRow ||
                fromCol != 4 ||
                toRow != homeRow ||
                abs(dc) != 2
            ) {
                return false;
            }

            bool kingMoved =
                white
                ? game.whiteKingMoved
                : game.blackKingMoved;

            if (kingMoved)
                return false;

            bool kingSide = toCol == 6;
            bool queenSide = toCol == 2;

            if (!kingSide && !queenSide)
                return false;

            if (kingSide) {
                bool rookMoved =
                    white
                    ? game.whiteHRookMoved
                    : game.blackHRookMoved;

                char rook = white ? 'R' : 'r';

                if (
                    rookMoved ||
                    game.board[homeRow][7] != rook ||
                    game.board[homeRow][5] != ' ' ||
                    game.board[homeRow][6] != ' '
                ) {
                    return false;
                }

                if (
                    chessInCheck(game, white) ||
                    chessSquareAttacked(game, homeRow, 5, !white) ||
                    chessSquareAttacked(game, homeRow, 6, !white)
                ) {
                    return false;
                }

                return true;
            }

            bool rookMoved =
                white
                ? game.whiteARookMoved
                : game.blackARookMoved;

            char rook = white ? 'R' : 'r';

            if (
                rookMoved ||
                game.board[homeRow][0] != rook ||
                game.board[homeRow][1] != ' ' ||
                game.board[homeRow][2] != ' ' ||
                game.board[homeRow][3] != ' '
            ) {
                return false;
            }

            if (
                chessInCheck(game, white) ||
                chessSquareAttacked(game, homeRow, 3, !white) ||
                chessSquareAttacked(game, homeRow, 2, !white)
            ) {
                return false;
            }

            return true;
        }
    }

    return false;
}

void applyChessMoveUnchecked(
    ChessGame& game,
    int fromRow,
    int fromCol,
    int toRow,
    int toCol,
    char promotion
) {
    char piece = game.board[fromRow][fromCol];
    char captured = game.board[toRow][toCol];
    bool white = chessWhitePiece(piece);
    char type = (char)tolower((unsigned char)piece);

    // If a rook is captured on its original square, castling on that side
    // can never happen later.
    if (captured == 'R' && toRow == 7 && toCol == 0)
        game.whiteARookMoved = true;
    if (captured == 'R' && toRow == 7 && toCol == 7)
        game.whiteHRookMoved = true;
    if (captured == 'r' && toRow == 0 && toCol == 0)
        game.blackARookMoved = true;
    if (captured == 'r' && toRow == 0 && toCol == 7)
        game.blackHRookMoved = true;

    // En passant capture.
    if (
        type == 'p' &&
        fromCol != toCol &&
        captured == ' ' &&
        toRow == game.enPassantRow &&
        toCol == game.enPassantCol
    ) {
        game.board[fromRow][toCol] = ' ';
    }

    game.board[fromRow][fromCol] = ' ';
    game.board[toRow][toCol] = piece;

    // Castling rook movement.
    if (type == 'k' && abs(toCol - fromCol) == 2) {
        if (toCol == 6) {
            game.board[toRow][5] = game.board[toRow][7];
            game.board[toRow][7] = ' ';
        }
        else {
            game.board[toRow][3] = game.board[toRow][0];
            game.board[toRow][0] = ' ';
        }
    }

    // Track castling rights.
    if (piece == 'K')
        game.whiteKingMoved = true;
    else if (piece == 'k')
        game.blackKingMoved = true;
    else if (piece == 'R' && fromRow == 7 && fromCol == 0)
        game.whiteARookMoved = true;
    else if (piece == 'R' && fromRow == 7 && fromCol == 7)
        game.whiteHRookMoved = true;
    else if (piece == 'r' && fromRow == 0 && fromCol == 0)
        game.blackARookMoved = true;
    else if (piece == 'r' && fromRow == 0 && fromCol == 7)
        game.blackHRookMoved = true;

    // Reset en-passant unless this was a two-square pawn move.
    game.enPassantRow = -1;
    game.enPassantCol = -1;

    if (type == 'p' && abs(toRow - fromRow) == 2) {
        game.enPassantRow = (fromRow + toRow) / 2;
        game.enPassantCol = fromCol;
    }

    // Pawn promotion. Default is queen; q/r/b/n are accepted.
    if (
        type == 'p' &&
        (toRow == 0 || toRow == 7)
    ) {
        char promoteTo = (char)tolower((unsigned char)promotion);

        if (
            promoteTo != 'q' &&
            promoteTo != 'r' &&
            promoteTo != 'b' &&
            promoteTo != 'n'
        ) {
            promoteTo = 'q';
        }

        game.board[toRow][toCol] =
            white
            ? (char)toupper((unsigned char)promoteTo)
            : promoteTo;
    }
}

bool legalChessMove(
    const ChessGame& game,
    int fromRow,
    int fromCol,
    int toRow,
    int toCol,
    bool white,
    char promotion = 'q'
) {
    if (
        !chessPseudoLegalMove(
            game,
            fromRow,
            fromCol,
            toRow,
            toCol,
            white
        )
    ) {
        return false;
    }

    ChessGame copy = game;

    applyChessMoveUnchecked(
        copy,
        fromRow,
        fromCol,
        toRow,
        toCol,
        promotion
    );

    return !chessInCheck(copy, white);
}

bool chessHasLegalMove(
    const ChessGame& game,
    bool white
) {
    for (int fromRow = 0; fromRow < 8; fromRow++) {
        for (int fromCol = 0; fromCol < 8; fromCol++) {
            char piece = game.board[fromRow][fromCol];

            if (
                piece == ' ' ||
                (white && !chessWhitePiece(piece)) ||
                (!white && !chessBlackPiece(piece))
            ) {
                continue;
            }

            for (int toRow = 0; toRow < 8; toRow++) {
                for (int toCol = 0; toCol < 8; toCol++) {
                    if (
                        legalChessMove(
                            game,
                            fromRow,
                            fromCol,
                            toRow,
                            toCol,
                            white,
                            'q'
                        )
                    ) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

void showChessBoard(
    ChessGame& game,
    bool showTurn = true
) {
    sendChessLine(game, "");
    sendChessLine(game, "=============== CHESS ===============");
    sendChessLine(game, "");
    sendChessLine(game, "      a   b   c   d   e   f   g   h");
    sendChessLine(game, "    +---+---+---+---+---+---+---+---+");

    for (int row = 0; row < 8; row++) {
        string line = " " + to_string(8 - row) + "  |";

        for (int col = 0; col < 8; col++) {
            line += " " + chessPieceSymbol(game.board[row][col]) + " |";
        }

        line += "  " + to_string(8 - row);
        sendChessLine(game, line);
        sendChessLine(game, "    +---+---+---+---+---+---+---+---+");
    }

    sendChessLine(game, "      a   b   c   d   e   f   g   h");
    sendChessLine(game, "");

    sendChessLine(
        game,
        "White: " + getName(game.white)
    );

    sendChessLine(
        game,
        "Black: " + getName(game.black)
    );

    if (showTurn) {
        bool whiteTurn = game.turn == game.white;

        sendChessLine(game, "");
        sendChessLine(
            game,
            "Turn: " +
            getName(game.turn) +
            (whiteTurn ? " (White)" : " (Black)")
        );

        sendChessLine(
            game,
            "Move: /move e2 e4   Promotion: /move e7 e8 q"
        );
    }

    sendChessLine(game, "=====================================");
    sendChessLine(game, "");
}

void playChessMove(
    int gameIndex,
    Client& client,
    const string& fromText,
    const string& toText,
    const string& promotionText
) {
    if (
        gameIndex < 0 ||
        gameIndex >= (int)chessGames.size()
    ) {
        sendPacket(
            client.socket,
            "ERR",
            "You are not in a Chess game."
        );
        return;
    }

    ChessGame& game = chessGames[gameIndex];

    if (game.turn != client.socket) {
        sendPacket(
            client.socket,
            "ERR",
            "It is not your Chess turn."
        );
        return;
    }

    int fromRow, fromCol, toRow, toCol;

    if (
        !parseChessSquare(fromText, fromRow, fromCol) ||
        !parseChessSquare(toText, toRow, toCol)
    ) {
        sendPacket(
            client.socket,
            "ERR",
            "Usage: /move <from> <to>  Example: /move e2 e4"
        );
        return;
    }

    bool white = client.socket == game.white;
    char promotion = 'q';

    if (!promotionText.empty()) {
        promotion = (char)tolower(
            (unsigned char)promotionText[0]
        );

        if (
            promotion != 'q' &&
            promotion != 'r' &&
            promotion != 'b' &&
            promotion != 'n'
        ) {
            sendPacket(
                client.socket,
                "ERR",
                "Promotion must be q, r, b, or n."
            );
            return;
        }
    }

    if (
        !legalChessMove(
            game,
            fromRow,
            fromCol,
            toRow,
            toCol,
            white,
            promotion
        )
    ) {
        sendPacket(
            client.socket,
            "ERR",
            "Illegal Chess move."
        );
        return;
    }

    applyChessMoveUnchecked(
        game,
        fromRow,
        fromCol,
        toRow,
        toCol,
        promotion
    );

    sendChessLine(
        game,
        client.name +
        " moves " +
        chessSquareName(fromRow, fromCol) +
        " to " +
        chessSquareName(toRow, toCol) +
        "."
    );

    game.turn =
        game.turn == game.white
        ? game.black
        : game.white;

    bool nextWhite = game.turn == game.white;
    bool nextInCheck = chessInCheck(game, nextWhite);
    bool nextHasMove = chessHasLegalMove(game, nextWhite);

    if (!nextHasMove) {
        Socket whiteSocket = game.white;
        Socket blackSocket = game.black;

        showChessBoard(game, false);

        if (nextInCheck) {
            sendChessLine(
                game,
                "*** CHECKMATE! " +
                client.name +
                " wins! ***"
            );
        }
        else {
            sendChessLine(
                game,
                "*** STALEMATE - DRAW! ***"
            );
        }

        sendReady(whiteSocket);
        sendReady(blackSocket);

        chessGames.erase(
            chessGames.begin() + gameIndex
        );

        return;
    }

    showChessBoard(game);

    if (nextInCheck) {
        sendChessLine(
            game,
            "*** CHECK! ***"
        );
    }

    readyChessPlayers(game);
}


// ============================================================
// BLACKJACK HELPERS
// ============================================================

void sendBlackjackLine(
    BlackjackGame& game,
    const string& text
) {
    sendPacket(
        game.player1,
        "GAME",
        text
    );

    sendPacket(
        game.player2,
        "GAME",
        text
    );
}

void readyBlackjackPlayers(BlackjackGame& game) {
    sendReady(game.player1);
    sendReady(game.player2);
}

Socket otherBlackjackPlayer(
    BlackjackGame& game,
    Socket socket
) {
    return
        socket == game.player1
        ? game.player2
        : game.player1;
}

int& blackjackChips(
    BlackjackGame& game,
    Socket socket
) {
    return
        socket == game.player1
        ? game.player1Chips
        : game.player2Chips;
}

int& blackjackBet(
    BlackjackGame& game,
    Socket socket
) {
    return
        socket == game.player1
        ? game.player1Bet
        : game.player2Bet;
}

bool& blackjackBetPlaced(
    BlackjackGame& game,
    Socket socket
) {
    return
        socket == game.player1
        ? game.player1BetPlaced
        : game.player2BetPlaced;
}

vector<Card>& blackjackHand(
    BlackjackGame& game,
    Socket socket
) {
    return
        socket == game.player1
        ? game.player1Hand
        : game.player2Hand;
}

bool& blackjackStood(
    BlackjackGame& game,
    Socket socket
) {
    return
        socket == game.player1
        ? game.player1Stood
        : game.player2Stood;
}

bool& blackjackBusted(
    BlackjackGame& game,
    Socket socket
) {
    return
        socket == game.player1
        ? game.player1Busted
        : game.player2Busted;
}

vector<Card> makeBlackjackDeck() {
    vector<Card> deck;

    const char suits[] = {'S', 'H', 'D', 'C'};

    struct RankInfo {
        const char* rank;
        int value;
    };

    RankInfo ranks[] = {
        {"A", 11},
        {"2", 2},
        {"3", 3},
        {"4", 4},
        {"5", 5},
        {"6", 6},
        {"7", 7},
        {"8", 8},
        {"9", 9},
        {"10", 10},
        {"J", 10},
        {"Q", 10},
        {"K", 10}
    };

    for (char suit : suits) {
        for (const RankInfo& rank : ranks) {
            Card card;
            card.rank = rank.rank;
            card.suit = suit;
            card.value = rank.value;

            deck.push_back(card);
        }
    }

    shuffle(
        deck.begin(),
        deck.end(),
        rng
    );

    return deck;
}

string blackjackSuitSymbol(char suit) {
    switch (toupper((unsigned char)suit)) {
        case 'S': return "♠";
        case 'H': return "♥";
        case 'D': return "♦";
        case 'C': return "♣";
        default:  return "?";
    }
}

string cardText(const Card& card) {
    return "[" + card.rank + blackjackSuitSymbol(card.suit) + "]";
}

vector<string> renderBlackjackCard(const Card& card) {
    string rank = card.rank;
    string suit = blackjackSuitSymbol(card.suit);

    string topText =
        rank +
        string(5 - rank.size(), ' ');

    string bottomText =
        string(5 - rank.size(), ' ') +
        rank;

    vector<string> lines;

    lines.push_back("┌─────┐");
    lines.push_back("│" + topText + "│");
    lines.push_back("│  " + suit + "  │");
    lines.push_back("│" + bottomText + "│");
    lines.push_back("└─────┘");

    return lines;
}

vector<string> renderHiddenBlackjackCard() {
    vector<string> lines;

    lines.push_back("┌─────┐");
    lines.push_back("│░░░░░│");
    lines.push_back("│░░░░░│");
    lines.push_back("│░░░░░│");
    lines.push_back("└─────┘");

    return lines;
}

vector<string> renderBlackjackCardRow(
    const vector<Card>& cards,
    bool hideFirst = false
) {
    vector<string> row(5, "");

    if (cards.empty()) {
        row[0] = "(no cards)";
        return row;
    }

    for (int i = 0; i < (int)cards.size(); i++) {
        vector<string> cardLines;

        if (hideFirst && i == 0) {
            cardLines = renderHiddenBlackjackCard();
        }
        else {
            cardLines = renderBlackjackCard(cards[i]);
        }

        for (int line = 0; line < 5; line++) {
            if (!row[line].empty())
                row[line] += " ";

            row[line] += cardLines[line];
        }
    }

    return row;
}

void sendBlackjackCardRow(
    BlackjackGame& game,
    const vector<Card>& cards,
    bool hideFirst = false
) {
    vector<string> row =
        renderBlackjackCardRow(
            cards,
            hideFirst
        );

    for (const string& line : row) {
        sendBlackjackLine(
            game,
            line
        );
    }
}

Card drawBlackjackCard(BlackjackGame& game) {
    if (game.deck.empty()) {
        game.deck = makeBlackjackDeck();
    }

    Card card = game.deck.back();
    game.deck.pop_back();

    return card;
}

int blackjackHandValue(const vector<Card>& hand) {
    int total = 0;
    int aces = 0;

    for (const Card& card : hand) {
        total += card.value;

        if (card.rank == "A")
            aces++;
    }

    while (total > 21 && aces > 0) {
        total -= 10;
        aces--;
    }

    return total;
}

bool naturalBlackjack(const vector<Card>& hand) {
    return
        hand.size() == 2 &&
        blackjackHandValue(hand) == 21;
}

void showBlackjackTable(
    BlackjackGame& game,
    bool revealDealer = false,
    bool showTurn = true
) {
    sendBlackjackLine(game, "");
    sendBlackjackLine(game, "========== BLACKJACK ==========");

    sendBlackjackLine(
        game,
        "Hand " +
        to_string(game.currentHand) +
        " of " +
        to_string(game.totalHands)
    );

    sendBlackjackLine(game, "");

    // ----------------------------
    // DEALER
    // ----------------------------

    sendBlackjackLine(
        game,
        "Dealer:"
    );

    sendBlackjackCardRow(
        game,
        game.dealerHand,
        !revealDealer
    );

    if (revealDealer) {
        sendBlackjackLine(
            game,
            "Dealer total: " +
            to_string(
                blackjackHandValue(
                    game.dealerHand
                )
            )
        );
    }
    else {
        sendBlackjackLine(
            game,
            "Dealer total: ?"
        );
    }

    sendBlackjackLine(game, "");

    // ----------------------------
    // PLAYER 1
    // ----------------------------

    sendBlackjackLine(
        game,
        getName(game.player1) +
        "  |  Chips: " +
        to_string(game.player1Chips) +
        "  |  Bet: " +
        to_string(game.player1Bet)
    );

    sendBlackjackCardRow(
        game,
        game.player1Hand
    );

    sendBlackjackLine(
        game,
        "Total: " +
        to_string(
            blackjackHandValue(
                game.player1Hand
            )
        )
    );

    sendBlackjackLine(game, "");

    // ----------------------------
    // PLAYER 2
    // ----------------------------

    sendBlackjackLine(
        game,
        getName(game.player2) +
        "  |  Chips: " +
        to_string(game.player2Chips) +
        "  |  Bet: " +
        to_string(game.player2Bet)
    );

    sendBlackjackCardRow(
        game,
        game.player2Hand
    );

    sendBlackjackLine(
        game,
        "Total: " +
        to_string(
            blackjackHandValue(
                game.player2Hand
            )
        )
    );

    sendBlackjackLine(game, "");

    // ----------------------------
    // TURN
    // ----------------------------

    if (
        showTurn &&
        game.handInProgress &&
        game.turn != INVALID_SOCK
    ) {
        sendBlackjackLine(
            game,
            "Turn: " +
            getName(game.turn)
        );

        sendBlackjackLine(
            game,
            "Commands: /hit or /stand"
        );
    }

    sendBlackjackLine(
        game,
        "================================"
    );

    sendBlackjackLine(game, "");
}

void showBlackjackBetting(BlackjackGame& game) {
    sendBlackjackLine(game, "");
    sendBlackjackLine(game, "========== BLACKJACK ==========");

    sendBlackjackLine(
        game,
        "Hand " +
        to_string(game.currentHand) +
        " of " +
        to_string(game.totalHands)
    );

    sendBlackjackLine(game, "");

    sendBlackjackLine(
        game,
        getName(game.player1) +
        ": " +
        to_string(game.player1Chips) +
        " chips"
    );

    sendBlackjackLine(
        game,
        getName(game.player2) +
        ": " +
        to_string(game.player2Chips) +
        " chips"
    );

    sendBlackjackLine(game, "");
    sendBlackjackLine(game, "Place your bet with /bet <amount>");
    sendBlackjackLine(game, "================================");
    sendBlackjackLine(game, "");
}

void resetBlackjackHand(BlackjackGame& game) {
    game.player1Bet = 0;
    game.player2Bet = 0;

    game.player1BetPlaced = false;
    game.player2BetPlaced = false;

    game.player1Hand.clear();
    game.player2Hand.clear();
    game.dealerHand.clear();

    game.player1Stood = false;
    game.player2Stood = false;

    game.player1Busted = false;
    game.player2Busted = false;

    game.handInProgress = false;
    game.turn = INVALID_SOCK;

    game.deck.clear();
}

void resolveBlackjackHand(int gameIndex);

void advanceBlackjackTurn(int gameIndex) {
    if (
        gameIndex < 0 ||
        gameIndex >= (int)blackjackGames.size()
    ) {
        return;
    }

    BlackjackGame& game = blackjackGames[gameIndex];

    bool player1Done =
        game.player1Stood ||
        game.player1Busted ||
        blackjackHandValue(game.player1Hand) >= 21;

    bool player2Done =
        game.player2Stood ||
        game.player2Busted ||
        blackjackHandValue(game.player2Hand) >= 21;

    if (!player1Done) {
        game.turn = game.player1;
        showBlackjackTable(game);
        readyBlackjackPlayers(game);
        return;
    }

    if (!player2Done) {
        game.turn = game.player2;
        showBlackjackTable(game);
        readyBlackjackPlayers(game);
        return;
    }

    resolveBlackjackHand(gameIndex);
}

void startBlackjackHand(int gameIndex) {
    if (
        gameIndex < 0 ||
        gameIndex >= (int)blackjackGames.size()
    ) {
        return;
    }

    BlackjackGame& game = blackjackGames[gameIndex];

    game.deck = makeBlackjackDeck();
    game.player1Hand.clear();
    game.player2Hand.clear();
    game.dealerHand.clear();

    game.player1Stood = false;
    game.player2Stood = false;
    game.player1Busted = false;
    game.player2Busted = false;
    game.handInProgress = true;

    // Deal in normal table order.
    game.player1Hand.push_back(drawBlackjackCard(game));
    game.player2Hand.push_back(drawBlackjackCard(game));
    game.dealerHand.push_back(drawBlackjackCard(game));

    game.player1Hand.push_back(drawBlackjackCard(game));
    game.player2Hand.push_back(drawBlackjackCard(game));
    game.dealerHand.push_back(drawBlackjackCard(game));

    if (naturalBlackjack(game.player1Hand)) {
        game.player1Stood = true;

        sendBlackjackLine(
            game,
            "*** " +
            getName(game.player1) +
            " has BLACKJACK! ***"
        );
    }

    if (naturalBlackjack(game.player2Hand)) {
        game.player2Stood = true;

        sendBlackjackLine(
            game,
            "*** " +
            getName(game.player2) +
            " has BLACKJACK! ***"
        );
    }

    if (!game.player1Stood) {
        game.turn = game.player1;
        showBlackjackTable(game);
        readyBlackjackPlayers(game);
        return;
    }

    if (!game.player2Stood) {
        game.turn = game.player2;
        showBlackjackTable(game);
        readyBlackjackPlayers(game);
        return;
    }

    resolveBlackjackHand(gameIndex);
}

int blackjackResultDelta(
    const vector<Card>& playerHand,
    const vector<Card>& dealerHand,
    int bet
) {
    int playerValue = blackjackHandValue(playerHand);
    int dealerValue = blackjackHandValue(dealerHand);

    bool playerNatural = naturalBlackjack(playerHand);
    bool dealerNatural = naturalBlackjack(dealerHand);

    if (playerValue > 21)
        return -bet;

    if (dealerNatural && !playerNatural)
        return -bet;

    if (playerNatural && !dealerNatural) {
        // Standard 3:2 blackjack payout. Integer chips round down.
        return (bet * 3) / 2;
    }

    if (dealerValue > 21)
        return bet;

    if (playerValue > dealerValue)
        return bet;

    if (playerValue < dealerValue)
        return -bet;

    return 0;
}

string blackjackResultText(
    const string& name,
    int delta
) {
    if (delta > 0) {
        return
            name +
            " wins " +
            to_string(delta) +
            " chips.";
    }

    if (delta < 0) {
        return
            name +
            " loses " +
            to_string(-delta) +
            " chips.";
    }

    return
        name +
        " pushes. Bet returned.";
}

void finishBlackjackMatch(int gameIndex) {
    if (
        gameIndex < 0 ||
        gameIndex >= (int)blackjackGames.size()
    ) {
        return;
    }

    BlackjackGame& game = blackjackGames[gameIndex];

    Socket player1 = game.player1;
    Socket player2 = game.player2;

    sendBlackjackLine(game, "");
    sendBlackjackLine(game, "========== BLACKJACK OVER ==========");

    sendBlackjackLine(
        game,
        getName(player1) +
        ": " +
        to_string(game.player1Chips) +
        " chips"
    );

    sendBlackjackLine(
        game,
        getName(player2) +
        ": " +
        to_string(game.player2Chips) +
        " chips"
    );

    sendBlackjackLine(game, "");

    if (game.player1Chips > game.player2Chips) {
        sendBlackjackLine(
            game,
            "*** WINNER: " +
            getName(player1) +
            " ***"
        );
    }
    else if (game.player2Chips > game.player1Chips) {
        sendBlackjackLine(
            game,
            "*** WINNER: " +
            getName(player2) +
            " ***"
        );
    }
    else {
        sendBlackjackLine(
            game,
            "*** MATCH ENDS IN A TIE ***"
        );
    }

    sendBlackjackLine(game, "====================================");
    sendBlackjackLine(game, "");

    sendReady(player1);
    sendReady(player2);

    blackjackGames.erase(
        blackjackGames.begin() + gameIndex
    );
}

void resolveBlackjackHand(int gameIndex) {
    if (
        gameIndex < 0 ||
        gameIndex >= (int)blackjackGames.size()
    ) {
        return;
    }

    BlackjackGame& game = blackjackGames[gameIndex];

    // Dealer hits to 17 and stands on all 17s.
    while (blackjackHandValue(game.dealerHand) < 17) {
        game.dealerHand.push_back(
            drawBlackjackCard(game)
        );
    }

    game.handInProgress = false;
    game.turn = INVALID_SOCK;

    showBlackjackTable(
        game,
        true,
        false
    );

    int player1Delta = blackjackResultDelta(
        game.player1Hand,
        game.dealerHand,
        game.player1Bet
    );

    int player2Delta = blackjackResultDelta(
        game.player2Hand,
        game.dealerHand,
        game.player2Bet
    );

    game.player1Chips += player1Delta;
    game.player2Chips += player2Delta;

    sendBlackjackLine(
        game,
        blackjackResultText(
            getName(game.player1),
            player1Delta
        )
    );

    sendBlackjackLine(
        game,
        blackjackResultText(
            getName(game.player2),
            player2Delta
        )
    );

    sendBlackjackLine(game, "");

    sendBlackjackLine(
        game,
        getName(game.player1) +
        ": " +
        to_string(game.player1Chips) +
        " chips remaining"
    );

    sendBlackjackLine(
        game,
        getName(game.player2) +
        ": " +
        to_string(game.player2Chips) +
        " chips remaining"
    );

    bool someoneBroke =
        game.player1Chips <= 0 ||
        game.player2Chips <= 0;

    bool allHandsPlayed =
        game.currentHand >= game.totalHands;

    if (someoneBroke) {
        sendBlackjackLine(
            game,
            "Match ending early because a player is out of chips."
        );
    }

    if (someoneBroke || allHandsPlayed) {
        finishBlackjackMatch(gameIndex);
        return;
    }

    game.currentHand++;
    resetBlackjackHand(game);

    sendBlackjackLine(
        game,
        "Next hand: " +
        to_string(game.currentHand) +
        " of " +
        to_string(game.totalHands)
    );

    showBlackjackBetting(game);
    readyBlackjackPlayers(game);
}



// ============================================================
// POKER - HEADS-UP TEXAS HOLD'EM
// ============================================================

string pokerStageName(PokerStage stage) {
    switch (stage) {
        case PokerStage::PREFLOP: return "PREFLOP";
        case PokerStage::FLOP: return "FLOP";
        case PokerStage::TURN: return "TURN";
        case PokerStage::RIVER: return "RIVER";
        case PokerStage::SHOWDOWN: return "SHOWDOWN";
    }

    return "UNKNOWN";
}

Socket otherPokerPlayer(
    const PokerGame& game,
    Socket socket
) {
    return
        game.player1 == socket
        ? game.player2
        : game.player1;
}

int& pokerChips(PokerGame& game, Socket socket) {
    return
        game.player1 == socket
        ? game.player1Chips
        : game.player2Chips;
}

int& pokerRoundBet(PokerGame& game, Socket socket) {
    return
        game.player1 == socket
        ? game.player1RoundBet
        : game.player2RoundBet;
}

bool& pokerActed(PokerGame& game, Socket socket) {
    return
        game.player1 == socket
        ? game.player1Acted
        : game.player2Acted;
}

vector<Card>& pokerHole(PokerGame& game, Socket socket) {
    return
        game.player1 == socket
        ? game.player1Hole
        : game.player2Hole;
}

string pokerCardCode(const Card& card) {
    return card.rank + string(1, card.suit);
}

vector<Card> makePokerDeck() {
    vector<Card> deck;

    const vector<pair<string, int>> ranks = {
        {"2", 2}, {"3", 3}, {"4", 4}, {"5", 5},
        {"6", 6}, {"7", 7}, {"8", 8}, {"9", 9},
        {"10", 10}, {"J", 11}, {"Q", 12}, {"K", 13}, {"A", 14}
    };

    const char suits[] = {'S', 'H', 'D', 'C'};

    for (char suit : suits) {
        for (const auto& rank : ranks) {
            Card card;
            card.rank = rank.first;
            card.suit = suit;
            card.value = rank.second;
            deck.push_back(card);
        }
    }

    shuffle(deck.begin(), deck.end(), rng);
    return deck;
}

Card drawPokerCard(PokerGame& game) {
    Card card = game.deck.back();
    game.deck.pop_back();
    return card;
}

void sendPokerStateTo(
    PokerGame& game,
    Socket player
) {
    Socket opponent = otherPokerPlayer(game, player);

    string state =
        pokerStageName(game.stage) + "|" +
        getName(opponent) + "|" +
        to_string(pokerChips(game, player)) + "|" +
        to_string(pokerChips(game, opponent)) + "|" +
        to_string(game.pot) + "|" +
        to_string(pokerRoundBet(game, player)) + "|" +
        to_string(pokerRoundBet(game, opponent)) + "|" +
        to_string(game.currentBet) + "|" +
        (game.turn == INVALID_SOCK ? string("") : getName(game.turn)) + "|" +
        getName(game.dealer) + "|" +
        to_string(game.smallBlind) + "|" +
        to_string(game.bigBlind) + "|" +
        (game.handActive ? "1" : "0") + "|" +
        to_string(game.handNumber) + "|" +
        to_string(game.lastRaiseSize);

    sendPacket(player, "POKER_STATE", state);

    vector<Card>& hole = pokerHole(game, player);

    string holeData = "--|--";

    if (hole.size() >= 2) {
        holeData =
            pokerCardCode(hole[0]) + "|" +
            pokerCardCode(hole[1]);
    }

    sendPacket(player, "POKER_HOLE", holeData);

    string boardData;

    for (int i = 0; i < 5; i++) {
        if (i > 0)
            boardData += "|";

        boardData +=
            i < (int)game.community.size()
            ? pokerCardCode(game.community[i])
            : "--";
    }

    sendPacket(player, "POKER_BOARD", boardData);
}

void sendPokerState(PokerGame& game) {
    sendPokerStateTo(game, game.player1);
    sendPokerStateTo(game, game.player2);
    sendReady(game.player1);
    sendReady(game.player2);
}

void sendPokerNotice(PokerGame& game, const string& text) {
    sendPacket(game.player1, "POKER_NOTICE", text);
    sendPacket(game.player2, "POKER_NOTICE", text);
}

void sendPokerResult(PokerGame& game, const string& text) {
    sendPacket(game.player1, "POKER_RESULT", text);
    sendPacket(game.player2, "POKER_RESULT", text);
}

void sendPokerReveal(PokerGame& game) {
    if (
        game.player1Hole.size() < 2 ||
        game.player2Hole.size() < 2
    ) {
        return;
    }

    sendPacket(
        game.player1,
        "POKER_REVEAL",
        getName(game.player2) + "|" +
        pokerCardCode(game.player2Hole[0]) + "|" +
        pokerCardCode(game.player2Hole[1])
    );

    sendPacket(
        game.player2,
        "POKER_REVEAL",
        getName(game.player1) + "|" +
        pokerCardCode(game.player1Hole[0]) + "|" +
        pokerCardCode(game.player1Hole[1])
    );
}

uint64_t packPokerScore(
    int category,
    const vector<int>& kickers
) {
    uint64_t score = ((uint64_t)category) << 24;
    const int shifts[5] = {20, 16, 12, 8, 4};

    for (
        int i = 0;
        i < (int)kickers.size() && i < 5;
        i++
    ) {
        score |= ((uint64_t)kickers[i]) << shifts[i];
    }

    return score;
}

uint64_t evaluatePokerFive(const array<Card, 5>& cards) {
    int counts[15] = {};
    vector<int> ranks;

    bool flush = true;
    char firstSuit = cards[0].suit;

    for (const Card& card : cards) {
        counts[card.value]++;
        ranks.push_back(card.value);

        if (card.suit != firstSuit)
            flush = false;
    }

    sort(ranks.begin(), ranks.end(), greater<int>());

    vector<int> uniqueRanks = ranks;
    uniqueRanks.erase(
        unique(uniqueRanks.begin(), uniqueRanks.end()),
        uniqueRanks.end()
    );

    int straightHigh = 0;

    if (uniqueRanks.size() == 5) {
        if (uniqueRanks[0] - uniqueRanks[4] == 4) {
            straightHigh = uniqueRanks[0];
        }
        else if (
            uniqueRanks[0] == 14 &&
            uniqueRanks[1] == 5 &&
            uniqueRanks[2] == 4 &&
            uniqueRanks[3] == 3 &&
            uniqueRanks[4] == 2
        ) {
            straightHigh = 5;
        }
    }

    vector<int> quads;
    vector<int> trips;
    vector<int> pairs;
    vector<int> singles;

    for (int rank = 14; rank >= 2; rank--) {
        if (counts[rank] == 4)
            quads.push_back(rank);
        else if (counts[rank] == 3)
            trips.push_back(rank);
        else if (counts[rank] == 2)
            pairs.push_back(rank);
        else if (counts[rank] == 1)
            singles.push_back(rank);
    }

    if (flush && straightHigh)
        return packPokerScore(8, {straightHigh});

    if (!quads.empty())
        return packPokerScore(7, {quads[0], singles[0]});

    if (!trips.empty() && !pairs.empty())
        return packPokerScore(6, {trips[0], pairs[0]});

    if (flush)
        return packPokerScore(5, ranks);

    if (straightHigh)
        return packPokerScore(4, {straightHigh});

    if (!trips.empty()) {
        vector<int> values = {trips[0]};
        values.insert(values.end(), singles.begin(), singles.end());
        return packPokerScore(3, values);
    }

    if (pairs.size() >= 2) {
        return packPokerScore(
            2,
            {pairs[0], pairs[1], singles[0]}
        );
    }

    if (pairs.size() == 1) {
        vector<int> values = {pairs[0]};
        values.insert(values.end(), singles.begin(), singles.end());
        return packPokerScore(1, values);
    }

    return packPokerScore(0, ranks);
}

uint64_t evaluatePokerBest(
    const vector<Card>& hole,
    const vector<Card>& community
) {
    vector<Card> all = hole;
    all.insert(all.end(), community.begin(), community.end());

    if (all.size() < 5)
        return 0;

    uint64_t best = 0;
    int n = (int)all.size();

    for (int a = 0; a < n - 4; a++)
    for (int b = a + 1; b < n - 3; b++)
    for (int c = b + 1; c < n - 2; c++)
    for (int d = c + 1; d < n - 1; d++)
    for (int e = d + 1; e < n; e++) {
        array<Card, 5> five = {
            all[a], all[b], all[c], all[d], all[e]
        };

        best = max(best, evaluatePokerFive(five));
    }

    return best;
}

string pokerHandName(uint64_t score) {
    int category = (int)(score >> 24);

    switch (category) {
        case 8: return "Straight Flush";
        case 7: return "Four of a Kind";
        case 6: return "Full House";
        case 5: return "Flush";
        case 4: return "Straight";
        case 3: return "Three of a Kind";
        case 2: return "Two Pair";
        case 1: return "Pair";
        default: return "High Card";
    }
}

void normalizePokerUncalledBet(PokerGame& game) {
    if (
        game.player1Chips == 0 &&
        game.player2RoundBet > game.player1RoundBet
    ) {
        int refund =
            game.player2RoundBet -
            game.player1RoundBet;

        game.player2RoundBet -= refund;
        game.player2Chips += refund;
        game.pot -= refund;
    }

    if (
        game.player2Chips == 0 &&
        game.player1RoundBet > game.player2RoundBet
    ) {
        int refund =
            game.player1RoundBet -
            game.player2RoundBet;

        game.player1RoundBet -= refund;
        game.player1Chips += refund;
        game.pot -= refund;
    }

    game.currentBet = max(
        game.player1RoundBet,
        game.player2RoundBet
    );
}

void pokerShowdown(PokerGame& game) {
    while (game.community.size() < 5)
        game.community.push_back(drawPokerCard(game));

    normalizePokerUncalledBet(game);

    uint64_t player1Score = evaluatePokerBest(
        game.player1Hole,
        game.community
    );

    uint64_t player2Score = evaluatePokerBest(
        game.player2Hole,
        game.community
    );

    sendPokerReveal(game);

    string result;

    if (player1Score > player2Score) {
        game.player1Chips += game.pot;
        result =
            getName(game.player1) +
            " wins " +
            to_string(game.pot) +
            " chips with " +
            pokerHandName(player1Score) +
            ".";
    }
    else if (player2Score > player1Score) {
        game.player2Chips += game.pot;
        result =
            getName(game.player2) +
            " wins " +
            to_string(game.pot) +
            " chips with " +
            pokerHandName(player2Score) +
            ".";
    }
    else {
        int half = game.pot / 2;
        int oddChip = game.pot % 2;

        game.player1Chips += half;
        game.player2Chips += half;
        pokerChips(game, game.dealer) += oddChip;

        result =
            "Split pot - both players have " +
            pokerHandName(player1Score) +
            ".";
    }

    game.pot = 0;
    game.handActive = false;
    game.turn = INVALID_SOCK;
    game.stage = PokerStage::SHOWDOWN;

    sendPokerState(game);
    sendPokerResult(game, result);
}

void runOutPokerBoard(PokerGame& game) {
    while (game.community.size() < 5)
        game.community.push_back(drawPokerCard(game));

    pokerShowdown(game);
}

void startPokerHand(PokerGame& game) {
    game.handNumber++;
    game.stage = PokerStage::PREFLOP;
    game.handActive = true;
    game.turn = INVALID_SOCK;

    game.deck = makePokerDeck();
    game.player1Hole.clear();
    game.player2Hole.clear();
    game.community.clear();

    game.pot = 0;
    game.player1RoundBet = 0;
    game.player2RoundBet = 0;
    game.currentBet = 0;
    game.lastRaiseSize = game.bigBlind;
    game.player1Acted = false;
    game.player2Acted = false;

    game.player1Hole.push_back(drawPokerCard(game));
    game.player2Hole.push_back(drawPokerCard(game));
    game.player1Hole.push_back(drawPokerCard(game));
    game.player2Hole.push_back(drawPokerCard(game));

    Socket smallBlindPlayer = game.dealer;
    Socket bigBlindPlayer = otherPokerPlayer(
        game,
        game.dealer
    );

    int smallAmount = min(
        game.smallBlind,
        pokerChips(game, smallBlindPlayer)
    );

    pokerChips(game, smallBlindPlayer) -= smallAmount;
    pokerRoundBet(game, smallBlindPlayer) += smallAmount;
    game.pot += smallAmount;

    int bigAmount = min(
        game.bigBlind,
        pokerChips(game, bigBlindPlayer)
    );

    pokerChips(game, bigBlindPlayer) -= bigAmount;
    pokerRoundBet(game, bigBlindPlayer) += bigAmount;
    game.pot += bigAmount;

    normalizePokerUncalledBet(game);

    game.currentBet = max(
        game.player1RoundBet,
        game.player2RoundBet
    );

    game.player1Acted = game.player1Chips == 0;
    game.player2Acted = game.player2Chips == 0;

    if (
        game.player1Chips == 0 ||
        game.player2Chips == 0
    ) {
        runOutPokerBoard(game);
        return;
    }

    // Heads-up: dealer / small blind acts first preflop.
    game.turn = game.dealer;

    sendPokerState(game);
    sendPokerNotice(
        game,
        "Hand " +
        to_string(game.handNumber) +
        " - cards dealt."
    );
}

void advancePokerStreet(PokerGame& game) {
    game.player1RoundBet = 0;
    game.player2RoundBet = 0;
    game.currentBet = 0;
    game.lastRaiseSize = game.bigBlind;
    game.player1Acted = game.player1Chips == 0;
    game.player2Acted = game.player2Chips == 0;

    if (game.stage == PokerStage::PREFLOP) {
        game.stage = PokerStage::FLOP;
        game.community.push_back(drawPokerCard(game));
        game.community.push_back(drawPokerCard(game));
        game.community.push_back(drawPokerCard(game));
    }
    else if (game.stage == PokerStage::FLOP) {
        game.stage = PokerStage::TURN;
        game.community.push_back(drawPokerCard(game));
    }
    else if (game.stage == PokerStage::TURN) {
        game.stage = PokerStage::RIVER;
        game.community.push_back(drawPokerCard(game));
    }
    else if (game.stage == PokerStage::RIVER) {
        pokerShowdown(game);
        return;
    }

    if (
        game.player1Chips == 0 ||
        game.player2Chips == 0
    ) {
        runOutPokerBoard(game);
        return;
    }

    // Heads-up: the non-dealer acts first after the flop.
    game.turn = otherPokerPlayer(
        game,
        game.dealer
    );

    sendPokerState(game);
}

void finishPokerAction(
    PokerGame& game,
    Socket actor
) {
    normalizePokerUncalledBet(game);

    if (
        game.player1RoundBet == game.player2RoundBet &&
        (
            game.player1Chips == 0 ||
            game.player2Chips == 0
        )
    ) {
        runOutPokerBoard(game);
        return;
    }

    bool roundComplete =
        game.player1Acted &&
        game.player2Acted &&
        game.player1RoundBet == game.player2RoundBet;

    if (roundComplete) {
        advancePokerStreet(game);
        return;
    }

    Socket next = otherPokerPlayer(game, actor);

    if (pokerChips(game, next) == 0) {
        game.turn = actor;
    }
    else {
        game.turn = next;
    }

    sendPokerState(game);
}

// ============================================================
// COMMAND HANDLER
// ============================================================

void handleCommand(Client& client, const string& line) {
    if (line.empty())
        return;

    // --------------------------------------------------------
    // NORMAL CHAT
    // --------------------------------------------------------

    if (line[0] != '/') {
        broadcastChat(
            client.name,
            line
        );

        return;
    }

    stringstream ss(line);
    string command;
    ss >> command;

    command = lowerCopy(command);


    // --------------------------------------------------------
    // /users
    // --------------------------------------------------------

    if (command == "/users") {
        string result = "Online users: ";
        bool first = true;

        for (Client& c : clients) {
            if (c.name.empty())
                continue;

            if (!first)
                result += ", ";

            result += c.name;
            first = false;
        }

        sendPacket(
            client.socket,
            "SYS",
            result
        );
    }


    // --------------------------------------------------------
    // /ttt <username>
    // --------------------------------------------------------

    else if (command == "/ttt") {
        string targetName;
        ss >> targetName;

        if (targetName.empty()) {
            sendPacket(
                client.socket,
                "ERR",
                "Usage: /ttt <username>"
            );
            return;
        }

        if (isPlayerBusy(client.socket)) {
            sendPacket(
                client.socket,
                "ERR",
                "You are already in a game."
            );
            return;
        }

        Client* target = getClientByName(targetName);

        if (!target) {
            sendPacket(
                client.socket,
                "ERR",
                "User not found."
            );
            return;
        }

        if (target->socket == client.socket) {
            sendPacket(
                client.socket,
                "ERR",
                "You cannot challenge yourself."
            );
            return;
        }

        if (isPlayerBusy(target->socket)) {
            sendPacket(
                client.socket,
                "ERR",
                target->name +
                " is already playing."
            );
            return;
        }

        if (target->pendingChallenge != INVALID_SOCK) {
            sendPacket(
                client.socket,
                "ERR",
                target->name +
                " already has a pending challenge."
            );
            return;
        }

        target->pendingChallenge = client.socket;
        target->pendingGame = "ttt";
        target->pendingChips = 0;
        target->pendingHands = 0;

        sendPacket(
            client.socket,
            "GAME",
            "Tic-Tac-Toe challenge sent to " +
            target->name +
            "."
        );

        sendPacket(
            target->socket,
            "GAME",
            "*** " +
            client.name +
            " challenged you to Tic-Tac-Toe! ***"
        );

        sendPacket(
            target->socket,
            "GAME",
            "Type /accept or /decline"
        );

        sendReady(target->socket);
    }


    // --------------------------------------------------------
    // /chess <username>
    // --------------------------------------------------------

    else if (command == "/chess") {
        string targetName;
        ss >> targetName;

        if (targetName.empty()) {
            sendPacket(
                client.socket,
                "ERR",
                "Usage: /chess <username>"
            );
            return;
        }

        if (isPlayerBusy(client.socket)) {
            sendPacket(
                client.socket,
                "ERR",
                "You are already in a game."
            );
            return;
        }

        Client* target = getClientByName(targetName);

        if (!target) {
            sendPacket(
                client.socket,
                "ERR",
                "User not found."
            );
            return;
        }

        if (target->socket == client.socket) {
            sendPacket(
                client.socket,
                "ERR",
                "You cannot challenge yourself."
            );
            return;
        }

        if (isPlayerBusy(target->socket)) {
            sendPacket(
                client.socket,
                "ERR",
                target->name + " is already playing."
            );
            return;
        }

        if (target->pendingChallenge != INVALID_SOCK) {
            sendPacket(
                client.socket,
                "ERR",
                target->name +
                " already has a pending challenge."
            );
            return;
        }

        target->pendingChallenge = client.socket;
        target->pendingGame = "chess";
        target->pendingChips = 0;
        target->pendingHands = 0;

        sendPacket(
            client.socket,
            "GAME",
            "Chess challenge sent to " +
            target->name +
            ". You will play White."
        );

        sendPacket(
            target->socket,
            "GAME",
            "*** " +
            client.name +
            " challenged you to Chess! ***"
        );

        sendPacket(
            target->socket,
            "GAME",
            client.name +
            " will play White; you will play Black."
        );

        sendPacket(
            target->socket,
            "GAME",
            "Type /accept or /decline"
        );

        sendReady(target->socket);
    }


    // --------------------------------------------------------
    // /blackjack <username> <starting_chips> <hands>
    // --------------------------------------------------------

    else if (
        command == "/blackjack" ||
        command == "/bj"
    ) {
        string targetName;
        int startingChips = 0;
        int hands = 0;

        if (!(ss >> targetName >> startingChips >> hands)) {
            sendPacket(
                client.socket,
                "ERR",
                "Usage: /blackjack <username> <starting_chips> <hands>"
            );
            return;
        }

        if (
            startingChips <= 0 ||
            startingChips > MAX_STARTING_CHIPS
        ) {
            sendPacket(
                client.socket,
                "ERR",
                "Starting chips must be between 1 and " +
                to_string(MAX_STARTING_CHIPS) +
                "."
            );
            return;
        }

        if (
            hands < 1 ||
            hands > MAX_BLACKJACK_HANDS
        ) {
            sendPacket(
                client.socket,
                "ERR",
                "Hands must be between 1 and " +
                to_string(MAX_BLACKJACK_HANDS) +
                "."
            );
            return;
        }

        if (isPlayerBusy(client.socket)) {
            sendPacket(
                client.socket,
                "ERR",
                "You are already in a game."
            );
            return;
        }

        Client* target = getClientByName(targetName);

        if (!target) {
            sendPacket(
                client.socket,
                "ERR",
                "User not found."
            );
            return;
        }

        if (target->socket == client.socket) {
            sendPacket(
                client.socket,
                "ERR",
                "You cannot challenge yourself."
            );
            return;
        }

        if (isPlayerBusy(target->socket)) {
            sendPacket(
                client.socket,
                "ERR",
                target->name +
                " is already playing."
            );
            return;
        }

        if (target->pendingChallenge != INVALID_SOCK) {
            sendPacket(
                client.socket,
                "ERR",
                target->name +
                " already has a pending challenge."
            );
            return;
        }

        target->pendingChallenge = client.socket;
        target->pendingGame = "blackjack";
        target->pendingChips = startingChips;
        target->pendingHands = hands;

        sendPacket(
            client.socket,
            "GAME",
            "Blackjack challenge sent to " +
            target->name +
            "."
        );

        sendPacket(
            target->socket,
            "GAME",
            "*** " +
            client.name +
            " challenged you to Blackjack! ***"
        );

        sendPacket(
            target->socket,
            "GAME",
            "Starting chips: " +
            to_string(startingChips)
        );

        sendPacket(
            target->socket,
            "GAME",
            "Hands: " +
            to_string(hands)
        );

        sendPacket(
            target->socket,
            "GAME",
            "Type /accept or /decline"
        );

        sendReady(target->socket);
    }


    // --------------------------------------------------------
    // /poker <username> <starting_chips> <small_blind>
    // --------------------------------------------------------

    else if (command == "/poker") {
        string targetName;
        int startingChips = 0;
        int smallBlind = 0;

        if (!(ss >> targetName >> startingChips >> smallBlind)) {
            sendPacket(
                client.socket,
                "ERR",
                "Usage: /poker <username> <starting_chips> <small_blind>"
            );
            return;
        }

        int bigBlind = smallBlind * 2;

        if (
            smallBlind <= 0 ||
            startingChips < bigBlind ||
            startingChips > MAX_POKER_CHIPS
        ) {
            sendPacket(
                client.socket,
                "ERR",
                "Poker requires positive blinds and starting chips at least equal to the big blind."
            );
            return;
        }

        if (isPlayerBusy(client.socket)) {
            sendPacket(
                client.socket,
                "ERR",
                "You are already in a game."
            );
            return;
        }

        Client* target = getClientByName(targetName);

        if (!target) {
            sendPacket(client.socket, "ERR", "User not found.");
            return;
        }

        if (target->socket == client.socket) {
            sendPacket(
                client.socket,
                "ERR",
                "You cannot challenge yourself."
            );
            return;
        }

        if (isPlayerBusy(target->socket)) {
            sendPacket(
                client.socket,
                "ERR",
                target->name + " is already playing."
            );
            return;
        }

        if (target->pendingChallenge != INVALID_SOCK) {
            sendPacket(
                client.socket,
                "ERR",
                target->name +
                " already has a pending challenge."
            );
            return;
        }

        target->pendingChallenge = client.socket;
        target->pendingGame = "poker";
        target->pendingChips = startingChips;
        target->pendingHands = smallBlind;

        sendPacket(
            client.socket,
            "GAME",
            "Poker challenge sent to " +
            target->name +
            "."
        );

        sendPacket(
            target->socket,
            "GAME",
            "*** " +
            client.name +
            " challenged you to Poker! ***"
        );

        sendPacket(
            target->socket,
            "GAME",
            "Starting chips: " +
            to_string(startingChips) +
            " | Blinds: " +
            to_string(smallBlind) +
            "/" +
            to_string(bigBlind)
        );

        sendPacket(
            target->socket,
            "GAME",
            "Type /accept or /decline"
        );

        sendReady(target->socket);
    }


    // --------------------------------------------------------
    // /accept
    // --------------------------------------------------------

    else if (command == "/accept") {
        if (client.pendingChallenge == INVALID_SOCK) {
            sendPacket(
                client.socket,
                "ERR",
                "You have no pending challenge."
            );
            return;
        }

        Client* challenger = getClient(
            client.pendingChallenge
        );

        if (!challenger) {
            clearPendingChallenge(client);

            sendPacket(
                client.socket,
                "ERR",
                "That player disconnected."
            );
            return;
        }

        if (
            isPlayerBusy(client.socket) ||
            isPlayerBusy(challenger->socket)
        ) {
            clearPendingChallenge(client);

            sendPacket(
                client.socket,
                "ERR",
                "One of the players is already in a game."
            );
            return;
        }

        string gameType = client.pendingGame;
        int startingChips = client.pendingChips;
        int hands = client.pendingHands;
        int pokerSmallBlind = client.pendingHands;

        Socket challengerSocket = challenger->socket;
        Socket accepterSocket = client.socket;

        clearPendingChallenge(client);

        if (gameType == "ttt") {
            TicTacToeGame game;
            game.playerX = challengerSocket;
            game.playerO = accepterSocket;
            game.turn = challengerSocket;

            ticTacToeGames.push_back(game);

            sendPacket(
                challengerSocket,
                "GAME",
                client.name +
                " accepted your Tic-Tac-Toe challenge!"
            );

            sendPacket(
                accepterSocket,
                "GAME",
                "Challenge accepted!"
            );

            showTicTacToeBoard(
                ticTacToeGames.back()
            );

            sendReady(challengerSocket);
            sendReady(accepterSocket);
        }
        else if (gameType == "chess") {
            ChessGame game = makeChessGame(
                challengerSocket,
                accepterSocket
            );

            chessGames.push_back(game);

            ChessGame& created = chessGames.back();

            sendChessLine(
                created,
                "*** Chess challenge accepted! ***"
            );

            sendChessLine(
                created,
                getName(challengerSocket) +
                " is White. " +
                getName(accepterSocket) +
                " is Black."
            );

            showChessBoard(created);
            readyChessPlayers(created);
        }
        else if (gameType == "blackjack") {
            BlackjackGame game;
            game.player1 = challengerSocket;
            game.player2 = accepterSocket;
            game.player1Chips = startingChips;
            game.player2Chips = startingChips;
            game.totalHands = hands;
            game.currentHand = 1;

            blackjackGames.push_back(game);

            BlackjackGame& created = blackjackGames.back();

            sendBlackjackLine(
                created,
                "*** Blackjack challenge accepted! ***"
            );

            sendBlackjackLine(
                created,
                "Both players start with " +
                to_string(startingChips) +
                " chips."
            );

            sendBlackjackLine(
                created,
                "Match length: " +
                to_string(hands) +
                " hands."
            );

            showBlackjackBetting(created);
            readyBlackjackPlayers(created);
        }
        else if (gameType == "poker") {
            PokerGame game;
            game.player1 = challengerSocket;
            game.player2 = accepterSocket;
            game.player1Chips = startingChips;
            game.player2Chips = startingChips;
            game.smallBlind = pokerSmallBlind;
            game.bigBlind = pokerSmallBlind * 2;
            game.dealer = challengerSocket;

            pokerGames.push_back(game);
            PokerGame& created = pokerGames.back();

            sendPacket(
                challengerSocket,
                "GAME",
                client.name +
                " accepted your Poker challenge!"
            );

            sendPacket(
                accepterSocket,
                "GAME",
                "Poker challenge accepted!"
            );

            startPokerHand(created);
        }
        else {
            sendPacket(
                accepterSocket,
                "ERR",
                "The pending challenge type was invalid."
            );
        }
    }


    // --------------------------------------------------------
    // /decline
    // --------------------------------------------------------

    else if (command == "/decline") {
        if (client.pendingChallenge == INVALID_SOCK) {
            sendPacket(
                client.socket,
                "ERR",
                "You have no pending challenge."
            );
            return;
        }

        Client* challenger = getClient(
            client.pendingChallenge
        );

        string gameName;

        if (client.pendingGame == "blackjack")
            gameName = "Blackjack";
        else if (client.pendingGame == "chess")
            gameName = "Chess";
        else if (client.pendingGame == "poker")
            gameName = "Poker";
        else
            gameName = "Tic-Tac-Toe";

        if (challenger) {
            sendPacket(
                challenger->socket,
                "GAME",
                client.name +
                " declined your " +
                gameName +
                " challenge."
            );

            sendReady(challenger->socket);
        }

        clearPendingChallenge(client);

        sendPacket(
            client.socket,
            "GAME",
            "Challenge declined."
        );
    }


    // --------------------------------------------------------
    // /move
    // Tic-Tac-Toe: /move <1-9>
    // Chess:       /move <from> <to> [promotion]
    // --------------------------------------------------------

    else if (command == "/move") {
        int chessIndex = findChessGame(
            client.socket
        );

        if (chessIndex != -1) {
            string fromSquare;
            string toSquare;
            string promotion;

            ss >> fromSquare >> toSquare >> promotion;

            if (fromSquare.empty() || toSquare.empty()) {
                sendPacket(
                    client.socket,
                    "ERR",
                    "Usage: /move <from> <to>  Example: /move e2 e4"
                );
                return;
            }

            playChessMove(
                chessIndex,
                client,
                fromSquare,
                toSquare,
                promotion
            );

            return;
        }

        int position;

        if (!(ss >> position)) {
            sendPacket(
                client.socket,
                "ERR",
                "Tic-Tac-Toe: /move <1-9> | Chess: /move e2 e4"
            );
            return;
        }

        int gameIndex = findTicTacToeGame(
            client.socket
        );

        if (gameIndex == -1) {
            sendPacket(
                client.socket,
                "ERR",
                "You are not in a Tic-Tac-Toe or Chess game."
            );
            return;
        }

        TicTacToeGame& game =
            ticTacToeGames[gameIndex];

        if (game.turn != client.socket) {
            sendPacket(
                client.socket,
                "ERR",
                "It is not your turn."
            );
            return;
        }

        if (position < 1 || position > 9) {
            sendPacket(
                client.socket,
                "ERR",
                "Position must be 1 through 9."
            );
            return;
        }

        int index = position - 1;

        if (game.board[index] != ' ') {
            sendPacket(
                client.socket,
                "ERR",
                "That square is already taken."
            );
            return;
        }

        char symbol =
            client.socket == game.playerX
            ? 'X'
            : 'O';

        game.board[index] = symbol;

        if (ticTacToeWinner(game, symbol)) {
            Socket playerX = game.playerX;
            Socket playerO = game.playerO;

            showTicTacToeBoard(
                game,
                false
            );

            sendTicTacToeLine(
                game,
                "*** " +
                client.name +
                " WINS! ***"
            );

            sendReady(playerX);
            sendReady(playerO);

            ticTacToeGames.erase(
                ticTacToeGames.begin() + gameIndex
            );

            return;
        }

        if (ticTacToeBoardFull(game)) {
            Socket playerX = game.playerX;
            Socket playerO = game.playerO;

            showTicTacToeBoard(
                game,
                false
            );

            sendTicTacToeLine(
                game,
                "*** DRAW! ***"
            );

            sendReady(playerX);
            sendReady(playerO);

            ticTacToeGames.erase(
                ticTacToeGames.begin() + gameIndex
            );

            return;
        }

        game.turn =
            game.turn == game.playerX
            ? game.playerO
            : game.playerX;

        showTicTacToeBoard(game);
        sendReady(game.playerX);
        sendReady(game.playerO);
    }

    // --------------------------------------------------------
    // /board
    // --------------------------------------------------------

    else if (command == "/board") {
        int chessIndex = findChessGame(
            client.socket
        );

        if (chessIndex != -1) {
            ChessGame& game = chessGames[chessIndex];
            showChessBoard(game);
            readyChessPlayers(game);
            return;
        }

        int gameIndex = findTicTacToeGame(
            client.socket
        );

        if (gameIndex == -1) {
            sendPacket(
                client.socket,
                "ERR",
                "You are not currently playing Tic-Tac-Toe or Chess."
            );
            return;
        }

        TicTacToeGame& game = ticTacToeGames[gameIndex];
        showTicTacToeBoard(game);
        sendReady(game.playerX);
        sendReady(game.playerO);
    }

    // --------------------------------------------------------
    // /bet <amount>
    // --------------------------------------------------------

    else if (command == "/bet") {
        int amount = 0;

        if (!(ss >> amount)) {
            sendPacket(
                client.socket,
                "ERR",
                "Usage: /bet <amount>"
            );
            return;
        }

        int gameIndex = findBlackjackGame(
            client.socket
        );

        if (gameIndex == -1) {
            sendPacket(
                client.socket,
                "ERR",
                "You are not in a Blackjack game."
            );
            return;
        }

        BlackjackGame& game =
            blackjackGames[gameIndex];

        if (game.handInProgress) {
            sendPacket(
                client.socket,
                "ERR",
                "The hand has already started."
            );
            return;
        }

        if (blackjackBetPlaced(game, client.socket)) {
            sendPacket(
                client.socket,
                "ERR",
                "You already placed a bet for this hand."
            );
            return;
        }

        int chips = blackjackChips(
            game,
            client.socket
        );

        if (amount <= 0) {
            sendPacket(
                client.socket,
                "ERR",
                "Your bet must be greater than 0."
            );
            return;
        }

        if (amount > chips) {
            sendPacket(
                client.socket,
                "ERR",
                "You only have " +
                to_string(chips) +
                " chips."
            );
            return;
        }

        blackjackBet(game, client.socket) = amount;
        blackjackBetPlaced(game, client.socket) = true;

        sendBlackjackLine(
            game,
            client.name +
            " bets " +
            to_string(amount) +
            " chips."
        );

        if (
            game.player1BetPlaced &&
            game.player2BetPlaced
        ) {
            startBlackjackHand(gameIndex);
        }
        else {
            sendBlackjackLine(
                game,
                "Waiting for " +
                getName(otherBlackjackPlayer(game, client.socket)) +
                " to place a bet..."
            );

            readyBlackjackPlayers(game);
        }
    }


    // --------------------------------------------------------
    // /hit
    // --------------------------------------------------------

    else if (command == "/hit") {
        int gameIndex = findBlackjackGame(
            client.socket
        );

        if (gameIndex == -1) {
            sendPacket(
                client.socket,
                "ERR",
                "You are not in a Blackjack game."
            );
            return;
        }

        BlackjackGame& game =
            blackjackGames[gameIndex];

        if (!game.handInProgress) {
            sendPacket(
                client.socket,
                "ERR",
                "The hand has not started yet. Place your bet first."
            );
            return;
        }

        if (game.turn != client.socket) {
            sendPacket(
                client.socket,
                "ERR",
                "It is not your Blackjack turn."
            );
            return;
        }

        Card card = drawBlackjackCard(game);
        blackjackHand(game, client.socket).push_back(card);

        sendBlackjackLine(
            game,
            client.name +
            " draws " +
            cardText(card) +
            "."
        );

        int value = blackjackHandValue(
            blackjackHand(game, client.socket)
        );

        if (value > 21) {
            blackjackBusted(game, client.socket) = true;

            sendBlackjackLine(
                game,
                "*** " +
                client.name +
                " BUSTS with " +
                to_string(value) +
                "! ***"
            );

            advanceBlackjackTurn(gameIndex);
            return;
        }

        if (value == 21) {
            blackjackStood(game, client.socket) = true;

            sendBlackjackLine(
                game,
                client.name +
                " has 21 and automatically stands."
            );

            advanceBlackjackTurn(gameIndex);
            return;
        }

        showBlackjackTable(game);
        readyBlackjackPlayers(game);
    }


    // --------------------------------------------------------
    // /stand
    // --------------------------------------------------------

    else if (command == "/stand") {
        int gameIndex = findBlackjackGame(
            client.socket
        );

        if (gameIndex == -1) {
            sendPacket(
                client.socket,
                "ERR",
                "You are not in a Blackjack game."
            );
            return;
        }

        BlackjackGame& game =
            blackjackGames[gameIndex];

        if (!game.handInProgress) {
            sendPacket(
                client.socket,
                "ERR",
                "The hand has not started yet."
            );
            return;
        }

        if (game.turn != client.socket) {
            sendPacket(
                client.socket,
                "ERR",
                "It is not your Blackjack turn."
            );
            return;
        }

        blackjackStood(game, client.socket) = true;

        sendBlackjackLine(
            game,
            client.name +
            " stands on " +
            to_string(
                blackjackHandValue(
                    blackjackHand(game, client.socket)
                )
            ) +
            "."
        );

        advanceBlackjackTurn(gameIndex);
    }


    // --------------------------------------------------------
    // /bjstatus
    // --------------------------------------------------------

    else if (
        command == "/bjstatus" ||
        command == "/blackjackstatus"
    ) {
        int gameIndex = findBlackjackGame(
            client.socket
        );

        if (gameIndex == -1) {
            sendPacket(
                client.socket,
                "ERR",
                "You are not in a Blackjack game."
            );
            return;
        }

        BlackjackGame& game =
            blackjackGames[gameIndex];

        if (game.handInProgress)
            showBlackjackTable(game);
        else
            showBlackjackBetting(game);

        readyBlackjackPlayers(game);
    }


    // --------------------------------------------------------
    // POKER ACTIONS
    // --------------------------------------------------------

    else if (command == "/pokercheck") {
        int gameIndex = findPokerGame(client.socket);

        if (gameIndex == -1) {
            sendPacket(client.socket, "ERR", "You are not in a Poker game.");
            return;
        }

        PokerGame& game = pokerGames[gameIndex];

        if (!game.handActive || game.turn != client.socket) {
            sendPacket(client.socket, "ERR", "It is not your Poker turn.");
            return;
        }

        if (pokerRoundBet(game, client.socket) != game.currentBet) {
            sendPacket(client.socket, "ERR", "You cannot check while facing a bet.");
            return;
        }

        pokerActed(game, client.socket) = true;
        sendPokerNotice(game, client.name + " checks.");
        finishPokerAction(game, client.socket);
    }

    else if (command == "/pokercall") {
        int gameIndex = findPokerGame(client.socket);

        if (gameIndex == -1) {
            sendPacket(client.socket, "ERR", "You are not in a Poker game.");
            return;
        }

        PokerGame& game = pokerGames[gameIndex];

        if (!game.handActive || game.turn != client.socket) {
            sendPacket(client.socket, "ERR", "It is not your Poker turn.");
            return;
        }

        int amount =
            game.currentBet -
            pokerRoundBet(game, client.socket);

        if (amount <= 0) {
            sendPacket(client.socket, "ERR", "There is nothing to call.");
            return;
        }

        int paid = min(
            amount,
            pokerChips(game, client.socket)
        );

        pokerChips(game, client.socket) -= paid;
        pokerRoundBet(game, client.socket) += paid;
        game.pot += paid;
        pokerActed(game, client.socket) = true;

        normalizePokerUncalledBet(game);

        sendPokerNotice(
            game,
            client.name +
            " calls " +
            to_string(paid) +
            "."
        );

        finishPokerAction(game, client.socket);
    }

    else if (command == "/pokerraise") {
        int gameIndex = findPokerGame(client.socket);
        int target = 0;

        if (gameIndex == -1) {
            sendPacket(client.socket, "ERR", "You are not in a Poker game.");
            return;
        }

        if (!(ss >> target)) {
            sendPacket(client.socket, "ERR", "Usage: /pokerraise <total_bet>");
            return;
        }

        PokerGame& game = pokerGames[gameIndex];

        if (!game.handActive || game.turn != client.socket) {
            sendPacket(client.socket, "ERR", "It is not your Poker turn.");
            return;
        }

        Socket opponent = otherPokerPlayer(game, client.socket);

        int maximum = min(
            pokerRoundBet(game, client.socket) +
                pokerChips(game, client.socket),
            pokerRoundBet(game, opponent) +
                pokerChips(game, opponent)
        );

        if (maximum <= game.currentBet) {
            sendPacket(client.socket, "ERR", "No further raise is possible.");
            return;
        }

        if (target > maximum || target <= game.currentBet) {
            sendPacket(
                client.socket,
                "ERR",
                "Raise target must be above the current bet and no more than " +
                to_string(maximum) +
                "."
            );
            return;
        }

        int minimum =
            game.currentBet +
            max(game.lastRaiseSize, game.bigBlind);

        bool allInRaise = target == maximum;

        if (target < minimum && !allInRaise) {
            sendPacket(
                client.socket,
                "ERR",
                "Minimum raise-to amount is " +
                to_string(minimum) +
                "."
            );
            return;
        }

        int oldCurrentBet = game.currentBet;
        int payment =
            target -
            pokerRoundBet(game, client.socket);

        pokerChips(game, client.socket) -= payment;
        pokerRoundBet(game, client.socket) = target;
        game.pot += payment;
        game.currentBet = target;

        int raiseSize = target - oldCurrentBet;

        if (raiseSize >= game.lastRaiseSize)
            game.lastRaiseSize = raiseSize;

        pokerActed(game, client.socket) = true;
        pokerActed(game, opponent) = pokerChips(game, opponent) == 0;

        sendPokerNotice(
            game,
            client.name +
            " raises to " +
            to_string(target) +
            "."
        );

        finishPokerAction(game, client.socket);
    }

    else if (command == "/pokerfold") {
        int gameIndex = findPokerGame(client.socket);

        if (gameIndex == -1) {
            sendPacket(client.socket, "ERR", "You are not in a Poker game.");
            return;
        }

        PokerGame& game = pokerGames[gameIndex];

        if (!game.handActive || game.turn != client.socket) {
            sendPacket(client.socket, "ERR", "It is not your Poker turn.");
            return;
        }

        Socket winner = otherPokerPlayer(game, client.socket);
        int won = game.pot;

        pokerChips(game, winner) += game.pot;
        game.pot = 0;
        game.handActive = false;
        game.turn = INVALID_SOCK;
        game.stage = PokerStage::SHOWDOWN;

        sendPokerState(game);
        sendPokerResult(
            game,
            client.name +
            " folds. " +
            getName(winner) +
            " wins " +
            to_string(won) +
            " chips."
        );
    }

    else if (command == "/pokernext") {
        int gameIndex = findPokerGame(client.socket);

        if (gameIndex == -1) {
            sendPacket(client.socket, "ERR", "You are not in a Poker game.");
            return;
        }

        PokerGame& game = pokerGames[gameIndex];

        if (game.handActive) {
            sendPacket(client.socket, "ERR", "The current Poker hand is still active.");
            return;
        }

        if (game.player1Chips <= 0 || game.player2Chips <= 0) {
            Socket winner =
                game.player1Chips > game.player2Chips
                ? game.player1
                : game.player2;

            string result =
                getName(winner) +
                " wins the Poker match!";

            sendPacket(game.player1, "POKER_END", result);
            sendPacket(game.player2, "POKER_END", result);
            sendReady(game.player1);
            sendReady(game.player2);

            pokerGames.erase(
                pokerGames.begin() + gameIndex
            );

            return;
        }

        game.dealer = otherPokerPlayer(
            game,
            game.dealer
        );

        startPokerHand(game);
    }


    // --------------------------------------------------------
    // /resign
    // --------------------------------------------------------

    else if (command == "/resign") {
        int ticTacToeIndex = findTicTacToeGame(
            client.socket
        );

        if (ticTacToeIndex != -1) {
            TicTacToeGame& game =
                ticTacToeGames[ticTacToeIndex];

            Socket opponent =
                game.playerX == client.socket
                ? game.playerO
                : game.playerX;

            Socket playerX = game.playerX;
            Socket playerO = game.playerO;

            sendTicTacToeLine(
                game,
                client.name +
                " resigned."
            );

            sendTicTacToeLine(
                game,
                getName(opponent) +
                " wins!"
            );

            sendReady(playerX);
            sendReady(playerO);

            ticTacToeGames.erase(
                ticTacToeGames.begin() + ticTacToeIndex
            );

            return;
        }

        int chessIndex = findChessGame(
            client.socket
        );

        if (chessIndex != -1) {
            ChessGame& game = chessGames[chessIndex];

            Socket opponent =
                game.white == client.socket
                ? game.black
                : game.white;

            Socket white = game.white;
            Socket black = game.black;

            sendChessLine(
                game,
                client.name +
                " resigned from Chess."
            );

            sendChessLine(
                game,
                "*** " +
                getName(opponent) +
                " wins by resignation. ***"
            );

            sendReady(white);
            sendReady(black);

            chessGames.erase(
                chessGames.begin() + chessIndex
            );

            return;
        }

        int blackjackIndex = findBlackjackGame(
            client.socket
        );

        if (blackjackIndex != -1) {
            BlackjackGame& game =
                blackjackGames[blackjackIndex];

            Socket opponent = otherBlackjackPlayer(
                game,
                client.socket
            );

            Socket player1 = game.player1;
            Socket player2 = game.player2;

            sendBlackjackLine(
                game,
                client.name +
                " resigned from Blackjack."
            );

            sendBlackjackLine(
                game,
                "*** " +
                getName(opponent) +
                " wins the match by resignation. ***"
            );

            sendReady(player1);
            sendReady(player2);

            blackjackGames.erase(
                blackjackGames.begin() + blackjackIndex
            );

            return;
        }

        int pokerIndex = findPokerGame(
            client.socket
        );

        if (pokerIndex != -1) {
            PokerGame& game = pokerGames[pokerIndex];
            Socket opponent = otherPokerPlayer(
                game,
                client.socket
            );

            string result =
                client.name +
                " resigned from Poker. " +
                getName(opponent) +
                " wins the match.";

            sendPacket(game.player1, "POKER_END", result);
            sendPacket(game.player2, "POKER_END", result);
            sendReady(game.player1);
            sendReady(game.player2);

            pokerGames.erase(
                pokerGames.begin() + pokerIndex
            );

            return;
        }

        sendPacket(
            client.socket,
            "ERR",
            "You are not currently in a game."
        );
    }


    // --------------------------------------------------------
    // /help
    // --------------------------------------------------------

    else if (command == "/help") {
        sendPacket(client.socket, "SYS", "========== JENG CHAT COMMANDS ==========");
        sendPacket(client.socket, "SYS", "CHAT");
        sendPacket(client.socket, "SYS", "/users");
        sendPacket(client.socket, "SYS", "/quit");
        sendPacket(client.socket, "SYS", "");
        sendPacket(client.socket, "SYS", "TIC-TAC-TOE");
        sendPacket(client.socket, "SYS", "/ttt <username>");
        sendPacket(client.socket, "SYS", "/move <1-9>");
        sendPacket(client.socket, "SYS", "/board");
        sendPacket(client.socket, "SYS", "/resign");
        sendPacket(client.socket, "SYS", "");
        sendPacket(client.socket, "SYS", "CHESS");
        sendPacket(client.socket, "SYS", "/chess <username>");
        sendPacket(client.socket, "SYS", "/move e2 e4");
        sendPacket(client.socket, "SYS", "/move e7 e8 q   (promotion: q/r/b/n)");
        sendPacket(client.socket, "SYS", "/board");
        sendPacket(client.socket, "SYS", "/resign");
        sendPacket(client.socket, "SYS", "");
        sendPacket(client.socket, "SYS", "BLACKJACK");
        sendPacket(client.socket, "SYS", "/blackjack <username> <starting_chips> <hands>");
        sendPacket(client.socket, "SYS", "/bet <amount>");
        sendPacket(client.socket, "SYS", "/hit");
        sendPacket(client.socket, "SYS", "/stand");
        sendPacket(client.socket, "SYS", "/bjstatus");
        sendPacket(client.socket, "SYS", "/resign");
        sendPacket(client.socket, "SYS", "");
        sendPacket(client.socket, "SYS", "POKER");
        sendPacket(client.socket, "SYS", "/poker <username> <starting_chips> <small_blind>");
        sendPacket(client.socket, "SYS", "/pokercheck");
        sendPacket(client.socket, "SYS", "/pokercall");
        sendPacket(client.socket, "SYS", "/pokerraise <total_bet>");
        sendPacket(client.socket, "SYS", "/pokerfold");
        sendPacket(client.socket, "SYS", "/pokernext");
        sendPacket(client.socket, "SYS", "/resign");
        sendPacket(client.socket, "SYS", "");
        sendPacket(client.socket, "SYS", "CHALLENGES");
        sendPacket(client.socket, "SYS", "/accept");
        sendPacket(client.socket, "SYS", "/decline");
        sendPacket(client.socket, "SYS", "========================================");
    }

    else {
        sendPacket(
            client.socket,
            "ERR",
            "Unknown command. Type /help"
        );
    }
}


// ============================================================
// DISCONNECT
// ============================================================

void disconnectClient(int index) {
    Socket socket = clients[index].socket;
    string name = clients[index].name;

    // If this user was the target of a pending challenge,
    // tell the challenger that it is gone.
    if (clients[index].pendingChallenge != INVALID_SOCK) {
        Client* challenger = getClient(
            clients[index].pendingChallenge
        );

        if (challenger) {
            sendPacket(
                challenger->socket,
                "GAME",
                "Challenge cancelled because " +
                name +
                " disconnected."
            );

            sendReady(challenger->socket);
        }
    }

    int ticTacToeIndex = findTicTacToeGame(socket);

    if (ticTacToeIndex != -1) {
        TicTacToeGame game =
            ticTacToeGames[ticTacToeIndex];

        Socket opponent =
            game.playerX == socket
            ? game.playerO
            : game.playerX;

        sendPacket(
            opponent,
            "GAME",
            name +
            " disconnected. Tic-Tac-Toe ended."
        );

        sendReady(opponent);

        ticTacToeGames.erase(
            ticTacToeGames.begin() + ticTacToeIndex
        );
    }

    int chessIndex = findChessGame(socket);

    if (chessIndex != -1) {
        ChessGame game = chessGames[chessIndex];

        Socket opponent =
            game.white == socket
            ? game.black
            : game.white;

        sendPacket(
            opponent,
            "GAME",
            name +
            " disconnected. Chess game ended."
        );

        sendReady(opponent);

        chessGames.erase(
            chessGames.begin() + chessIndex
        );
    }

    int blackjackIndex = findBlackjackGame(socket);

    if (blackjackIndex != -1) {
        BlackjackGame game =
            blackjackGames[blackjackIndex];

        Socket opponent =
            game.player1 == socket
            ? game.player2
            : game.player1;

        sendPacket(
            opponent,
            "GAME",
            name +
            " disconnected. Blackjack match ended."
        );

        sendReady(opponent);

        blackjackGames.erase(
            blackjackGames.begin() + blackjackIndex
        );
    }

    int pokerIndex = findPokerGame(socket);

    if (pokerIndex != -1) {
        PokerGame game = pokerGames[pokerIndex];
        Socket opponent = otherPokerPlayer(game, socket);

        string result =
            name +
            " disconnected. Poker match ended.";

        sendPacket(opponent, "POKER_END", result);
        sendReady(opponent);

        pokerGames.erase(
            pokerGames.begin() + pokerIndex
        );
    }

    // Cancel any challenges that this user had sent.
    for (Client& c : clients) {
        if (c.pendingChallenge == socket) {
            clearPendingChallenge(c);

            sendPacket(
                c.socket,
                "GAME",
                "Challenge cancelled because the challenger disconnected."
            );

            sendReady(c.socket);
        }
    }

    CLOSE_SOCKET(socket);

    clients.erase(
        clients.begin() + index
    );

    if (!name.empty()) {
        cout
            << name
            << " disconnected.\n";

        broadcastSystem(
            "*** " +
            name +
            " left the chat ***"
        );
    }
}


// ============================================================
// MAIN
// ============================================================

int main() {
    if (!initializeSocketLibrary()) {
        cout << "Socket initialization failed.\n";
        return 1;
    }

    Socket serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (serverSocket == INVALID_SOCK) {
        cout << "Could not create server socket.\n";
        cleanupSocketLibrary();
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    if (
        bind(
            serverSocket,
            (sockaddr*)&address,
            sizeof(address)
        ) == SOCKET_ERR
    ) {
        cout << "Bind failed.\n";
        CLOSE_SOCKET(serverSocket);
        cleanupSocketLibrary();
        return 1;
    }

    if (
        listen(
            serverSocket,
            SOMAXCONN
        ) == SOCKET_ERR
    ) {
        cout << "Listen failed.\n";
        CLOSE_SOCKET(serverSocket);
        cleanupSocketLibrary();
        return 1;
    }

    cout << "================================\n";
    cout << "        JENG CHAT SERVER\n";
    cout << "================================\n";
    cout << "Port: " << PORT << "\n";
    cout << "Games: Tic-Tac-Toe + Blackjack + Chess\n";
    cout << "Waiting for players...\n\n";

    while (true) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverSocket, &readSet);

        Socket maxSocket = serverSocket;

        for (Client& c : clients) {
            FD_SET(
                c.socket,
                &readSet
            );

            if (c.socket > maxSocket)
                maxSocket = c.socket;
        }

#ifdef _WIN32
        int nfds = 0; // Ignored by Winsock.
#else
        int nfds = maxSocket + 1;
#endif

        if (
            select(
                nfds,
                &readSet,
                nullptr,
                nullptr,
                nullptr
            ) == SOCKET_ERR
        ) {
            break;
        }

        // New connection.
        if (FD_ISSET(serverSocket, &readSet)) {
            Socket newClient = accept(
                serverSocket,
                nullptr,
                nullptr
            );

            if (newClient != INVALID_SOCK) {
                Client c;
                c.socket = newClient;

                clients.push_back(c);

                cout << "New connection received.\n";
            }
        }

        // Existing clients.
        for (int i = 0; i < (int)clients.size();) {
            if (!FD_ISSET(clients[i].socket, &readSet)) {
                i++;
                continue;
            }

            char buffer[BUFFER_SIZE];

            int received = recv(
                clients[i].socket,
                buffer,
                BUFFER_SIZE,
                0
            );

            if (received <= 0) {
                disconnectClient(i);
                continue;
            }

            clients[i].inputBuffer.append(
                buffer,
                received
            );

            while (true) {
                size_t newline =
                    clients[i]
                    .inputBuffer
                    .find('\n');

                if (newline == string::npos)
                    break;

                string line =
                    clients[i]
                    .inputBuffer
                    .substr(
                        0,
                        newline
                    );

                clients[i]
                .inputBuffer
                .erase(
                    0,
                    newline + 1
                );

                if (
                    !line.empty() &&
                    line.back() == '\r'
                ) {
                    line.pop_back();
                }

                // First line from a connection is the username.
                if (clients[i].name.empty()) {
                    clients[i].name = line;

                    cout
                        << line
                        << " connected.\n";

                    sendPacket(
                        clients[i].socket,
                        "SYS",
                        "*** Welcome to JENG CHAT, " +
                        line +
                        "! ***"
                    );

                    sendPacket(
                        clients[i].socket,
                        "SYS",
                        "Type /help for commands."
                    );

                    broadcastSystem(
                        "*** " +
                        line +
                        " joined the chat ***"
                    );

                    continue;
                }

                handleCommand(
                    clients[i],
                    line
                );

                // The sender can type again after the response.
                // Some game helpers also READY both players;
                // duplicate READY packets are harmless because
                // the client suppresses duplicate prompts.
                sendReady(
                    clients[i].socket
                );
            }

            i++;
        }
    }

    for (Client& c : clients) {
        CLOSE_SOCKET(c.socket);
    }

    CLOSE_SOCKET(serverSocket);
    cleanupSocketLibrary();

    return 0;
}
