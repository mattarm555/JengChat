#include "protocol.h"

#include "networking.h"
#include "theme.h"
#include "ui/ui_common.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace
{
    bool Contains(const string& text, const string& needle)
    {
        return text.find(needle) != string::npos;
    }

    vector<string> Split(const string& text, char delimiter = '|')
    {
        vector<string> parts;
        string current;
        istringstream stream(text);

        while (getline(stream, current, delimiter))
            parts.push_back(current);

        return parts;
    }

    int ToInt(const string& value, int fallback = 0)
    {
        try
        {
            return stoi(value);
        }
        catch (...)
        {
            return fallback;
        }
    }

    float ToFloat(const string& value, float fallback = 0.0f)
    {
        try
        {
            return stof(value);
        }
        catch (...)
        {
            return fallback;
        }
    }

    GameView DetectChallengeGame(const string& text)
    {
        if (Contains(text, "Chess") || Contains(text, "CHESS"))
            return GameView::CHESS;

        if (Contains(text, "Blackjack") || Contains(text, "BLACKJACK"))
            return GameView::BLACKJACK;

        if (Contains(text, "Poker") || Contains(text, "POKER"))
            return GameView::POKER;

        if (Contains(text, "Roulette") || Contains(text, "ROULETTE"))
            return GameView::ROULETTE;

        if (Contains(text, "Arena") || Contains(text, "ARENA"))
            return GameView::ARENA;

        return GameView::HOME;
    }

    string ChallengeTitle(GameView game)
    {
        if (game == GameView::CHESS)
            return "CHESS CHALLENGE";

        if (game == GameView::BLACKJACK)
            return "BLACKJACK CHALLENGE";

        if (game == GameView::POKER)
            return "POKER CHALLENGE";

        if (game == GameView::ROULETTE)
            return "ROULETTE INVITE";

        if (game == GameView::ARENA)
            return "JENG ARENA INVITE";

        return "GAME CHALLENGE";
    }

    int ChessSquareIndex(const string& square)
    {
        if (square.size() != 2)
            return -1;

        char file = square[0];

        if (file >= 'A' && file <= 'H')
            file = (char)(file - 'A' + 'a');

        char rank = square[1];

        if (
            file < 'a' || file > 'h' ||
            rank < '1' || rank > '8'
        )
        {
            return -1;
        }

        int col = file - 'a';
        int row = 8 - (rank - '0');

        return row * 8 + col;
    }


    void HandleChessPacket(AppState& app, const NetMessage& msg)
    {
        ChessClientState& chess = app.chess;

        if (msg.type == "CHESS_LEGAL")
        {
            // fromSquare|destination1,destination2,...
            // A dash means the selected piece has no legal destinations.
            vector<string> fields = Split(msg.data);

            if (fields.size() < 2)
                return;

            int sourceIndex = ChessSquareIndex(fields[0]);

            // Ignore an old response if the user selected another piece
            // before this response arrived.
            if (
                sourceIndex < 0 ||
                chess.selectedSquare != sourceIndex
            )
            {
                return;
            }

            chess.legalMoves.clear();
            chess.legalMoveSource = sourceIndex;
            chess.legalMovesLoaded = true;

            if (fields[1] != "-")
            {
                vector<string> destinations =
                    Split(fields[1], ',');

                for (const string& square : destinations)
                {
                    int index = ChessSquareIndex(square);

                    if (index >= 0)
                        chess.legalMoves.push_back(index);
                }
            }

            if (chess.legalMoves.empty())
            {
                chess.status = "This piece has no legal moves.";
            }
            else
            {
                chess.status =
                    to_string(chess.legalMoves.size()) +
                    " legal move" +
                    (chess.legalMoves.size() == 1 ? "" : "s") +
                    ".";
            }

            return;
        }

        if (msg.type == "CHESS_STATE")
        {
            vector<string> fields = Split(msg.data);

            // board64|whitePlayer|blackPlayer|turn|yourColor
            if (fields.size() < 5)
                return;

            if (fields[0].size() != 64)
                return;

            chess.board = fields[0];
            chess.whitePlayer = fields[1];
            chess.blackPlayer = fields[2];
            chess.turn = fields[3];
            chess.yourColor = fields[4];
            chess.active = true;
            chess.selectedSquare = -1;
            chess.legalMoves.clear();
            chess.legalMoveSource = -1;
            chess.legalMovesLoaded = false;

            if (chess.turn == app.username)
                chess.status = "Your turn.";
            else
                chess.status = chess.turn + "'s turn.";

            app.gameView = GameView::CHESS;
            return;
        }

        if (msg.type == "CHESS_NOTICE")
        {
            chess.status = msg.data;
            app.gameView = GameView::CHESS;
            return;
        }

        if (msg.type == "CHESS_ERROR")
        {
            chess.status = msg.data;
            app.gameView = GameView::CHESS;
            return;
        }

        if (msg.type == "CHESS_END")
        {
            chess.status = msg.data;
            chess.active = false;
            chess.selectedSquare = -1;
            chess.legalMoves.clear();
            chess.legalMoveSource = -1;
            chess.legalMovesLoaded = false;
            app.gameView = GameView::CHESS;
            return;
        }
    }


    vector<string> SplitNonEmpty(const string& text, char delimiter)
    {
        vector<string> parts;
        string current;
        istringstream stream(text);

        while (getline(stream, current, delimiter))
        {
            if (!current.empty())
                parts.push_back(current);
        }

        return parts;
    }

    BlackjackHandClientState ParseBlackjackHand(const string& encoded)
    {
        BlackjackHandClientState hand;
        vector<string> fields = Split(encoded, '~');

        // bet~value~done~busted~doubled~fromSplit~cardsCSV
        if (fields.size() < 7)
            return hand;

        hand.bet = ToInt(fields[0]);
        hand.value = ToInt(fields[1]);
        hand.done = fields[2] == "1";
        hand.busted = fields[3] == "1";
        hand.doubled = fields[4] == "1";
        hand.fromSplit = fields[5] == "1";

        if (!fields[6].empty() && fields[6] != "-")
            hand.cards = SplitNonEmpty(fields[6], ',');

        return hand;
    }

    vector<BlackjackHandClientState> ParseBlackjackHands(const string& encoded)
    {
        vector<BlackjackHandClientState> hands;

        if (encoded.empty() || encoded == "-")
            return hands;

        vector<string> encodedHands = SplitNonEmpty(encoded, ';');

        for (const string& handText : encodedHands)
            hands.push_back(ParseBlackjackHand(handText));

        return hands;
    }

    BlackjackPlayerClientState ParseBlackjackPlayer(const string& encoded)
    {
        BlackjackPlayerClientState player;
        vector<string> fields = Split(encoded, '^');

        // name^chips^betPlaced^pendingBet^handsEncoded
        if (fields.size() < 5)
            return player;

        player.name = fields[0];
        player.chips = ToInt(fields[1]);
        player.betPlaced = fields[2] == "1";
        player.pendingBet = ToInt(fields[3]);
        player.hands = ParseBlackjackHands(fields[4]);
        return player;
    }

    void HandleBlackjackPacket(AppState& app, const NetMessage& msg)
    {
        BlackjackClientState& bj = app.blackjack;

        if (msg.type == "BJ_CHALLENGE")
        {
            vector<string> fields = Split(msg.data);

            // host|startingChips|hands|acceptedPlayers
            if (fields.size() >= 3)
            {
                app.pendingChallenge.active = true;
                app.pendingChallenge.error.clear();
                app.pendingChallenge.game = GameView::BLACKJACK;
                app.pendingChallenge.title = "BLACKJACK INVITE";

                string playerCount = fields.size() >= 4 ? fields[3] : "1";

                app.pendingChallenge.message =
                    fields[0] +
                    " invited you to a Blackjack match - " +
                    fields[1] +
                    " starting chips, " +
                    fields[2] +
                    " hands. Players currently accepted: " +
                    playerCount + "/6.";

                app.showHelpMenu = false;
                app.commandPopup.open = false;
            }

            return;
        }

        if (msg.type == "BJ_NEW_HAND")
        {
            bj.dealQueue.clear();
            bj.payoutMessages.clear();
            bj.status = "Dealing cards...";
            bj.active = true;
            app.gameView = GameView::BLACKJACK;
            return;
        }

        if (msg.type == "BJ_SPLIT_RESET")
        {
            // The state packet redraws the two separated hands.
            // Newly dealt cards still animate in normally.
            bj.dealQueue.clear();
            return;
        }

        if (msg.type == "BJ_CARD")
        {
            vector<string> fields = Split(msg.data);

            // targetName|handIndex|cardIndex|cardCode
            if (fields.size() >= 4)
            {
                BlackjackDealEvent event;
                event.target = fields[0];
                event.handIndex = ToInt(fields[1]);
                event.cardIndex = ToInt(fields[2]);
                event.card = fields[3];
                event.progress = 0.0f;
                bj.dealQueue.push_back(event);
            }

            app.gameView = GameView::BLACKJACK;
            return;
        }

        if (msg.type == "BJ_STATE")
        {
            vector<string> fields = Split(msg.data);

            // phase|currentHand|totalHands|startingChips|hostName|turnName|
            // turnHand|dealerRevealed|dealerCards|awaitingNextHand|status|
            // playerCount|player1Encoded|player2Encoded|...
            if (fields.size() < 12)
                return;

            bj.phase = fields[0];
            bj.currentHand = ToInt(fields[1]);
            bj.totalHands = ToInt(fields[2]);
            bj.startingChips = ToInt(fields[3]);
            bj.hostName = fields[4];
            bj.turn = fields[5];
            bj.turnHandIndex = ToInt(fields[6]);
            bj.dealerRevealed = fields[7] == "1";
            bj.dealerCards = fields[8] == "-"
                ? vector<string>{}
                : SplitNonEmpty(fields[8], ',');
            bj.awaitingNextHand = fields[9] == "1";
            bj.status = fields[10];

            int playerCount = ToInt(fields[11]);
            bj.players.clear();

            for (int i = 0; i < playerCount; i++)
            {
                int fieldIndex = 12 + i;

                if (fieldIndex >= (int)fields.size())
                    break;

                bj.players.push_back(
                    ParseBlackjackPlayer(fields[fieldIndex])
                );
            }

            bj.active = true;

            int yourChips = 0;

            for (const BlackjackPlayerClientState& player : bj.players)
            {
                if (player.name == app.username)
                {
                    yourChips = player.chips;
                    break;
                }
            }

            if (bj.betAmount <= 0)
                bj.betAmount = 10;

            if (yourChips > 0)
                bj.betAmount = min(bj.betAmount, yourChips);

            app.gameView = GameView::BLACKJACK;
            return;
        }

        if (msg.type == "BJ_PAYOUT")
        {
            // name|handIndex|delta|reason
            vector<string> fields = Split(msg.data);

            if (fields.size() >= 4)
            {
                int delta = ToInt(fields[2]);
                string amount = delta > 0
                    ? "+" + to_string(delta)
                    : to_string(delta);

                string message =
                    fields[0] +
                    " hand " +
                    to_string(ToInt(fields[1]) + 1) +
                    ": " +
                    amount +
                    " chips - " +
                    fields[3];

                bj.payoutMessages.push_back(message);

                if (bj.payoutMessages.size() > 12)
                    bj.payoutMessages.erase(bj.payoutMessages.begin());
            }

            app.gameView = GameView::BLACKJACK;
            return;
        }

        if (msg.type == "BJ_NOTICE")
        {
            bj.status = msg.data;
            app.gameView = GameView::BLACKJACK;
            return;
        }

        if (msg.type == "BJ_ERROR")
        {
            bj.status = msg.data;
            app.gameView = GameView::BLACKJACK;
            return;
        }

        if (msg.type == "BJ_END")
        {
            // Keep the final hand and HAND RESULTS panel visible.
            bj.status = msg.data;
            bj.active = false;
            bj.phase = "RESULT";
            bj.turn.clear();
            bj.awaitingNextHand = false;
            bj.dealQueue.clear();

            app.gameView = GameView::BLACKJACK;
            return;
        }
    }


    RoulettePlayerClientState ParseRoulettePlayer(const string& encoded)
    {
        RoulettePlayerClientState player;
        vector<string> fields = Split(encoded, '^');

        // name^chips^ready^totalBet
        if (fields.size() < 4)
            return player;

        player.name = fields[0];
        player.chips = ToInt(fields[1]);
        player.ready = fields[2] == "1";
        player.totalBet = ToInt(fields[3]);
        return player;
    }

    void HandleRoulettePacket(AppState& app, const NetMessage& msg)
    {
        RouletteClientState& roulette = app.roulette;

        if (msg.type == "RLT_CHALLENGE")
        {
            vector<string> fields = Split(msg.data);

            // host|startingChips|rounds|acceptedPlayers
            if (fields.size() >= 3)
            {
                app.pendingChallenge.active = true;
                app.pendingChallenge.error.clear();
                app.pendingChallenge.game = GameView::ROULETTE;
                app.pendingChallenge.title = "ROULETTE INVITE";

                string playerCount = fields.size() >= 4 ? fields[3] : "1";

                app.pendingChallenge.message =
                    fields[0] +
                    " invited you to Roulette - " +
                    fields[1] +
                    " starting chips, " +
                    fields[2] +
                    " rounds. Players currently accepted: " +
                    playerCount + "/6.";

                app.showHelpMenu = false;
                app.commandPopup.open = false;
            }

            return;
        }

        if (msg.type == "RLT_STATE")
        {
            vector<string> fields = Split(msg.data);

            // phase|currentRound|totalRounds|startingChips|hostName|
            // lastResult|status|playerCount|playerEncoded...
            if (fields.size() < 8)
                return;

            string incomingPhase = fields[0];

            // If a final-state packet arrives while the wheel is still
            // animating, defer the visual transition. This prevents the
            // end screen from replacing the wheel in the same frame.
            if (
                incomingPhase == "ENDED" &&
                roulette.animating
            )
            {
                roulette.matchEndPending = true;
                roulette.pendingEndStatus = fields[6];
                roulette.phase = "RESULT";
            }
            else
            {
                roulette.phase = incomingPhase;
                roulette.status = fields[6];
            }

            roulette.currentRound = ToInt(fields[1]);
            roulette.totalRounds = ToInt(fields[2]);
            roulette.startingChips = ToInt(fields[3]);
            roulette.hostName = fields[4];
            roulette.lastResult = ToInt(fields[5], -1);

            if (
                roulette.animating &&
                incomingPhase == "RESULT"
            )
            {
                roulette.status = "Wheel spinning...";
            }

            int playerCount = ToInt(fields[7]);
            roulette.players.clear();

            for (int i = 0; i < playerCount; i++)
            {
                int fieldIndex = 8 + i;

                if (fieldIndex >= (int)fields.size())
                    break;

                roulette.players.push_back(
                    ParseRoulettePlayer(fields[fieldIndex])
                );
            }

            roulette.active = roulette.phase != "ENDED";
            app.gameView = GameView::ROULETTE;
            return;
        }

        if (msg.type == "RLT_BETS")
        {
            roulette.bets.clear();

            if (!msg.data.empty() && msg.data != "-")
            {
                vector<string> encodedBets = SplitNonEmpty(msg.data, ';');

                for (const string& encoded : encodedBets)
                {
                    vector<string> fields = Split(encoded, '~');

                    if (fields.size() < 3)
                        continue;

                    RouletteBetClientState bet;
                    bet.type = fields[0];
                    bet.value = ToInt(fields[1]);
                    bet.amount = ToInt(fields[2]);
                    roulette.bets.push_back(bet);
                }
            }

            return;
        }

        if (msg.type == "RLT_SPIN")
        {
            roulette.lastResult = ToInt(msg.data, -1);
            roulette.animating = true;
            roulette.matchEndPending = false;
            roulette.resultHoldStarted = false;
            roulette.pendingEndStatus.clear();
            roulette.resultHoldStartTime = 0.0f;

            roulette.spinStartTime = (float)GetTime();
            roulette.wheelStartRotation = roulette.wheelRotation;
            roulette.payoutMessages.clear();
            roulette.phase = "RESULT";
            roulette.status = "Wheel spinning...";
            app.gameView = GameView::ROULETTE;
            return;
        }

        if (msg.type == "RLT_PAYOUT")
        {
            // name|delta|chips|summary
            vector<string> fields = Split(msg.data);

            if (fields.size() >= 4)
            {
                int delta = ToInt(fields[1]);
                string deltaText =
                    delta > 0
                    ? "+" + to_string(delta)
                    : to_string(delta);

                roulette.payoutMessages.push_back(
                    fields[0] +
                    ": " +
                    deltaText +
                    " chips - " +
                    fields[3]
                );

                for (RoulettePlayerClientState& player : roulette.players)
                {
                    if (player.name == fields[0])
                    {
                        player.chips = ToInt(fields[2], player.chips);
                        break;
                    }
                }

                if (roulette.payoutMessages.size() > 12)
                    roulette.payoutMessages.erase(
                        roulette.payoutMessages.begin()
                    );
            }

            return;
        }

        if (msg.type == "RLT_NOTICE")
        {
            roulette.status = msg.data;
            app.gameView = GameView::ROULETTE;
            return;
        }

        if (msg.type == "RLT_ERROR")
        {
            roulette.status = msg.data;
            app.gameView = GameView::ROULETTE;
            return;
        }

        if (msg.type == "RLT_FINAL")
        {
            roulette.matchEndPending = true;
            roulette.pendingEndStatus = msg.data;

            if (!roulette.animating)
            {
                roulette.resultHoldStarted = true;
                roulette.resultHoldStartTime = (float)GetTime();
            }

            app.gameView = GameView::ROULETTE;
            return;
        }

        if (msg.type == "RLT_END")
        {
            // Normal table closes/leaves can end immediately. If an END
            // arrives while a result animation is active, preserve the
            // animation first.
            if (roulette.animating)
            {
                roulette.matchEndPending = true;
                roulette.pendingEndStatus = msg.data;
            }
            else
            {
                roulette.status = msg.data;
                roulette.phase = "ENDED";
                roulette.active = false;
            }

            app.gameView = GameView::ROULETTE;
            return;
        }
    }


    ArenaLobbyPlayerClientState ParseArenaLobbyPlayer(
        const string& encoded)
    {
        ArenaLobbyPlayerClientState player;
        vector<string> fields = Split(encoded, '^');

        // name^ready^colorIndex^team
        if (fields.size() < 4)
            return player;

        player.name = fields[0];
        player.ready = fields[1] == "1";
        player.colorIndex = ToInt(fields[2], -1);
        player.team = ToInt(fields[3], -1);
        return player;
    }


    ArenaWorldPlayerClientState ParseArenaWorldPlayer(
        const string& encoded)
    {
        ArenaWorldPlayerClientState player;
        vector<string> fields = Split(encoded, '^');

        // name^x^z^bodyYaw^aimYaw^colorIndex^team^health^alive^kills^deaths^damageDealt^damageTaken^respawn
        if (fields.size() < 7)
            return player;

        player.name = fields[0];
        player.x = ToFloat(fields[1]);
        player.z = ToFloat(fields[2]);
        player.bodyYaw = ToFloat(fields[3], 180.0f);
        player.aimYaw = ToFloat(fields[4], 180.0f);
        player.colorIndex = ToInt(fields[5], -1);
        player.team = ToInt(fields[6], -1);

        if (fields.size() >= 14)
        {
            player.health = ToInt(fields[7], 100);
            player.alive = ToInt(fields[8], 1) != 0;
            player.kills = ToInt(fields[9], 0);
            player.deaths = ToInt(fields[10], 0);
            player.damageDealt = ToInt(fields[11], 0);
            player.damageTaken = ToInt(fields[12], 0);
            player.respawnTimer = ToFloat(fields[13], 0.0f);
        }

        return player;
    }


    ArenaProjectileClientState ParseArenaProjectile(
        const string& encoded)
    {
        ArenaProjectileClientState projectile;
        vector<string> fields = Split(encoded, '^');

        // x^y^z
        if (fields.size() < 3)
            return projectile;

        projectile.x = ToFloat(fields[0], 0.0f);
        projectile.y = ToFloat(fields[1], 1.10f);
        projectile.z = ToFloat(fields[2], 0.0f);
        return projectile;
    }


    ArenaMineClientState ParseArenaMine(
        const string& encoded)
    {
        ArenaMineClientState mine;
        vector<string> fields = Split(encoded, '^');

        // x^z^colorIndex^team
        if (fields.size() < 4)
            return mine;

        mine.x = ToFloat(fields[0], 0.0f);
        mine.z = ToFloat(fields[1], 0.0f);
        mine.colorIndex = ToInt(fields[2], -1);
        mine.team = ToInt(fields[3], -1);
        return mine;
    }


    void HandleArenaPacket(AppState& app, const NetMessage& msg)
    {
        ArenaClientState& arena = app.arena;

        if (msg.type == "ARENA_CHALLENGE")
        {
            vector<string> fields = Split(msg.data);

            // host|mode|maxPlayers|scoreLimit|timeLimit|currentPlayers
            if (fields.size() >= 6)
            {
                app.pendingChallenge.active = true;
                app.pendingChallenge.error.clear();
                app.pendingChallenge.game = GameView::ARENA;
                app.pendingChallenge.title = "JENG ARENA INVITE";

                app.pendingChallenge.message =
                    fields[0] +
                    " invited you to JENG Arena - " +
                    fields[1] +
                    ", " +
                    fields[5] +
                    "/" +
                    fields[2] +
                    " players.";

                arena.mode = fields[1];
                arena.maxPlayers = ToInt(fields[2]);
                arena.scoreLimit = ToInt(fields[3], 5);
                arena.timeLimitSeconds = ToInt(fields[4], 180);
                arena.status = "Arena invitation received.";

                app.showHelpMenu = false;
                app.commandPopup.open = false;
            }

            return;
        }

        if (msg.type == "ARENA_STATE")
        {
            vector<string> fields = Split(msg.data);

            // phase|mode|maxPlayers|scoreLimit|timeLimit|host|status|
            // playerCount|playerEncoded...
            if (fields.size() < 8)
                return;

            arena.phase = fields[0];
            arena.mode = fields[1];
            arena.maxPlayers = ToInt(fields[2]);
            arena.scoreLimit = ToInt(fields[3], 5);
            arena.timeLimitSeconds = ToInt(fields[4], 180);
            arena.hostName = fields[5];
            arena.status = fields[6];

            int playerCount = ToInt(fields[7]);
            arena.players.clear();

            for (int i = 0; i < playerCount; i++)
            {
                int fieldIndex = 8 + i;

                if (fieldIndex >= (int)fields.size())
                    break;

                arena.players.push_back(
                    ParseArenaLobbyPlayer(
                        fields[fieldIndex]
                    )
                );
            }

            arena.active = true;
            arena.startSignalReceived =
                arena.phase == "STARTING" ||
                arena.phase == "PLAYING";

            if (
                arena.phase != "PLAYING" &&
                arena.phase != "POSTGAME"
            )
            {
                arena.matchActive = false;
                arena.worldPlayers.clear();
                arena.worldProjectiles.clear();
                arena.worldMines.clear();
                arena.timeRemainingSeconds = 0.0f;
            }

            if (arena.phase == "POSTGAME")
            {
                arena.matchActive = false;
                arena.startSignalReceived = false;
            }

            app.gameView = GameView::ARENA;
            return;
        }

        if (msg.type == "ARENA_WORLD")
        {
            vector<string> fields = Split(msg.data);

            // worldSequence|timeRemaining|playerCount|projectileCount|mineCount|playerEncoded...|projectileEncoded...|mineEncoded...
            if (fields.size() < 5)
                return;

            arena.worldSequence = ToInt(
                fields[0],
                arena.worldSequence
            );

            arena.timeRemainingSeconds = ToFloat(
                fields[1],
                arena.timeRemainingSeconds
            );

            int playerCount = ToInt(fields[2]);
            int projectileCount = ToInt(fields[3]);
            int mineCount = ToInt(fields[4]);

            arena.worldPlayers.clear();
            arena.worldProjectiles.clear();
            arena.worldMines.clear();

            int fieldIndex = 5;

            for (int i = 0; i < playerCount; i++)
            {
                if (fieldIndex >= (int)fields.size())
                    break;

                arena.worldPlayers.push_back(
                    ParseArenaWorldPlayer(
                        fields[fieldIndex]
                    )
                );

                fieldIndex++;
            }

            for (int i = 0; i < projectileCount; i++)
            {
                if (fieldIndex >= (int)fields.size())
                    break;

                arena.worldProjectiles.push_back(
                    ParseArenaProjectile(
                        fields[fieldIndex]
                    )
                );

                fieldIndex++;
            }

            for (int i = 0; i < mineCount; i++)
            {
                if (fieldIndex >= (int)fields.size())
                    break;

                arena.worldMines.push_back(
                    ParseArenaMine(
                        fields[fieldIndex]
                    )
                );

                fieldIndex++;
            }

            arena.active = true;
            arena.matchActive = !arena.worldPlayers.empty();
            arena.phase = "PLAYING";
            arena.startSignalReceived = true;
            arena.status =
                "Online combat synchronized.";

            app.gameView = GameView::ARENA;
            return;
        }

        if (msg.type == "ARENA_NOTICE")
        {
            arena.status = msg.data;
            return;
        }

        if (msg.type == "ARENA_ERROR")
        {
            arena.status = msg.data;
            app.gameView = GameView::ARENA;
            return;
        }

        if (msg.type == "ARENA_START")
        {
            arena.phase = "STARTING";
            arena.startSignalReceived = true;
            arena.matchActive = false;
            arena.status =
                "Starting synchronized Arena match...";
            app.gameView = GameView::ARENA;
            return;
        }

        if (msg.type == "ARENA_END")
        {
            arena.status = msg.data;
            arena.phase = "ENDED";
            arena.active = false;
            arena.startSignalReceived = false;
            arena.matchActive = false;
            arena.players.clear();
            arena.worldPlayers.clear();
            arena.worldProjectiles.clear();
            arena.worldMines.clear();
            arena.timeRemainingSeconds = 0.0f;
            return;
        }
    }


    void HandlePokerPacket(AppState& app, const NetMessage& msg)
    {
        PokerClientState& poker = app.poker;

        if (msg.type == "POKER_CHALLENGE")
        {
            vector<string> fields = Split(msg.data);

            // host|startingChips|smallBlind|bigBlind
            if (fields.size() >= 4)
            {
                app.pendingChallenge.active = true;
                app.pendingChallenge.error.clear();
                app.pendingChallenge.game = GameView::POKER;
                app.pendingChallenge.title = "POKER INVITE";
                app.pendingChallenge.message =
                    fields[0] +
                    " invited you to a Poker table - " +
                    fields[1] +
                    " starting chips, blinds " +
                    fields[2] +
                    "/" +
                    fields[3] +
                    ".";

                app.showHelpMenu = false;
                app.commandPopup.open = false;
            }

            return;
        }

        if (msg.type == "POKER_LOBBY")
        {
            vector<string> fields = Split(msg.data);

            // host|startingChips|smallBlind|bigBlind|player1|player2|status
            if (fields.size() < 7)
                return;

            poker.tableActive = true;
            poker.handActive = false;
            poker.tablePhase = "LOBBY";
            poker.hostName = fields[0];
            poker.startingChips = ToInt(fields[1]);
            poker.smallBlind = ToInt(fields[2]);
            poker.bigBlind = ToInt(fields[3]);

            string player1 = fields[4];
            string player2 = fields[5];

            if (app.username == player1)
                poker.opponent = player2;
            else
                poker.opponent = player1;

            poker.yourStack = poker.startingChips;
            poker.opponentStack =
                poker.opponent.empty()
                ? 0
                : poker.startingChips;

            poker.stage = "WAITING";
            poker.turn.clear();
            poker.dealer.clear();
            poker.pot = 0;
            poker.yourBet = 0;
            poker.opponentBet = 0;
            poker.currentBet = 0;
            poker.status = fields[6];

            poker.holeCards.clear();
            poker.communityCards.clear();
            poker.opponentCards.clear();
            poker.opponentRevealed = false;

            app.gameView = GameView::POKER;
            return;
        }

        if (msg.type == "POKER_STATE")
        {
            vector<string> fields = Split(msg.data);

            // stage|opponent|yourStack|opponentStack|pot|yourBet|
            // opponentBet|currentBet|turn|dealer|smallBlind|bigBlind|
            // handActive|handNumber|lastRaiseSize
            if (fields.size() < 15)
                return;

            int incomingHandNumber = ToInt(fields[13]);

            if (incomingHandNumber != poker.handNumber)
            {
                poker.opponentRevealed = false;
                poker.opponentCards.clear();
                poker.status = "New hand dealt.";
            }

            poker.tableActive = true;
            poker.stage = fields[0];
            poker.opponent = fields[1];
            poker.yourStack = ToInt(fields[2]);
            poker.opponentStack = ToInt(fields[3]);
            poker.pot = ToInt(fields[4]);
            poker.yourBet = ToInt(fields[5]);
            poker.opponentBet = ToInt(fields[6]);
            poker.currentBet = ToInt(fields[7]);
            poker.turn = fields[8];
            poker.dealer = fields[9];
            poker.smallBlind = ToInt(fields[10]);
            poker.bigBlind = ToInt(fields[11]);
            poker.handActive = fields[12] == "1";
            poker.tablePhase =
                poker.handActive
                ? "PLAYING"
                : "RESULT";
            poker.handNumber = incomingHandNumber;
            poker.lastRaiseSize = ToInt(fields[14]);

            int minRaise =
                poker.currentBet +
                max(poker.lastRaiseSize, poker.bigBlind);

            int maxRaise = min(
                poker.yourBet + poker.yourStack,
                poker.opponentBet + poker.opponentStack
            );

            if (maxRaise < minRaise)
                minRaise = maxRaise;

            if (poker.raiseTarget <= poker.currentBet)
                poker.raiseTarget = max(poker.currentBet, minRaise);

            if (maxRaise >= poker.currentBet)
                poker.raiseTarget = min(poker.raiseTarget, maxRaise);

            app.gameView = GameView::POKER;
            return;
        }

        if (msg.type == "POKER_HOLE")
        {
            vector<string> fields = Split(msg.data);

            poker.holeCards.clear();

            for (const string& card : fields)
            {
                if (!card.empty() && card != "--")
                    poker.holeCards.push_back(card);
            }

            return;
        }

        if (msg.type == "POKER_BOARD")
        {
            vector<string> fields = Split(msg.data);
            poker.communityCards.clear();

            for (const string& card : fields)
            {
                if (!card.empty())
                    poker.communityCards.push_back(card);
            }

            return;
        }

        if (msg.type == "POKER_REVEAL")
        {
            vector<string> fields = Split(msg.data);

            if (fields.size() >= 3)
            {
                poker.opponent = fields[0];
                poker.opponentCards = {fields[1], fields[2]};
                poker.opponentRevealed = true;
            }

            return;
        }

        if (msg.type == "POKER_RESULT")
        {
            poker.status = msg.data;
            poker.handActive = false;
            poker.tablePhase = "RESULT";
            return;
        }

        if (msg.type == "POKER_NOTICE")
        {
            poker.status = msg.data;
            return;
        }

        if (msg.type == "POKER_END")
        {
            poker.status = msg.data;
            poker.tableActive = false;
            poker.handActive = false;
            poker.tablePhase = "ENDED";
            poker.turn.clear();
            return;
        }
    }
}

void ProcessIncomingMessages(AppState& app)
{
    int oldHistorySize = (int)app.history.size();

    for (const NetMessage& msg : NetPollMessages())
    {
        if (msg.type == "READY")
            continue;

        if (msg.type.rfind("CHESS_", 0) == 0)
        {
            HandleChessPacket(app, msg);
            continue;
        }

        if (msg.type.rfind("BJ_", 0) == 0)
        {
            HandleBlackjackPacket(app, msg);
            continue;
        }

        if (msg.type.rfind("POKER_", 0) == 0)
        {
            HandlePokerPacket(app, msg);
            continue;
        }

        if (msg.type.rfind("RLT_", 0) == 0)
        {
            HandleRoulettePacket(app, msg);
            continue;
        }

        if (msg.type.rfind("ARENA_", 0) == 0)
        {
            HandleArenaPacket(app, msg);
            continue;
        }

        if (msg.type == "USERS_LIST")
        {
            app.onlineUsers.clear();
            std::istringstream roster(msg.data);
            std::string name;
            while (std::getline(roster, name, '|'))
                if (!name.empty()) app.onlineUsers.push_back(name);
            std::sort(app.onlineUsers.begin(), app.onlineUsers.end());
            app.onlineUsersStatus = "";
            continue;
        }

        if (msg.type == "CHAT")
        {
            size_t split = msg.data.find('|');

            if (split != string::npos)
            {
                string sender = msg.data.substr(0, split);
                string message = msg.data.substr(split + 1);

                AddChatLine(
                    app.history,
                    "[" + CurrentTime() + "] " + sender + ": " + message,
                    CHAT_TEXT
                );
            }

            continue;
        }

        if (msg.type == "SYS")
        {
            AddChatLine(app.history, msg.data, CHAT_SYSTEM);
            continue;
        }

        if (msg.type == "GAME")
        {
            // Blackjack is fully graphical now. If an older server sends
            // legacy Blackjack GAME lines, do not dump that terminal UI
            // into the permanent chat panel.
            if (app.blackjack.active)
            {
                if (
                    Contains(msg.data, "BLACKJACK") ||
                    Contains(msg.data, "Dealer:") ||
                    Contains(msg.data, "Turn:") ||
                    Contains(msg.data, "Commands: /hit") ||
                    Contains(msg.data, "Place your bet") ||
                    Contains(msg.data, " chips") ||
                    Contains(msg.data, "Total:") ||
                    Contains(msg.data, "====")
                )
                {
                    continue;
                }
            }
            if (Contains(msg.data, "challenged you to"))
            {
                app.pendingChallenge.active = true;
                app.pendingChallenge.error.clear();
                app.pendingChallenge.message = msg.data;
                app.pendingChallenge.game = DetectChallengeGame(msg.data);
                app.pendingChallenge.title = ChallengeTitle(
                    app.pendingChallenge.game
                );

                app.showHelpMenu = false;
                app.commandPopup.open = false;

                if (
                    app.pendingChallenge.game != GameView::CHESS &&
                    app.pendingChallenge.game != GameView::POKER
                )
                {
                    AddChatLine(
                        app.history,
                        msg.data,
                        SUCCESS
                    );
                }

                continue;
            }

            // Poker is graphical now. The legacy challenge sequence also
            // sends a second GAME line such as:
            //
            //   Starting chips: 1000 | Blinds: 10/20
            //
            // Keep those details in the invite modal instead of dumping
            // terminal-style game text into permanent chat.
            if (
                app.pendingChallenge.active &&
                app.pendingChallenge.game == GameView::POKER &&
                (
                    Contains(msg.data, "Starting chips:") ||
                    Contains(msg.data, "Blinds:")
                )
            )
            {
                app.poker.status = msg.data;
                continue;
            }

            if (Contains(msg.data, "Type /accept or /decline"))
                continue;

            if (
                Contains(msg.data, "Challenge cancelled") ||
                Contains(msg.data, "Challenge declined")
            )
            {
                app.pendingChallenge.active = false;
                app.pendingChallenge.error.clear();
            }

            if (
                Contains(msg.data, "accepted your challenge") &&
                app.outboundChallengeGame != GameView::HOME
            )
            {
                app.gameView = app.outboundChallengeGame;
                app.outboundChallengeGame = GameView::HOME;
            }

            if (Contains(msg.data, "Chess") || Contains(msg.data, "CHESS"))
            {
                app.chess.status = msg.data;
                app.gameView = GameView::CHESS;
                continue;
            }

            if (Contains(msg.data, "Blackjack") || Contains(msg.data, "BLACKJACK"))
            {
                app.blackjack.status = msg.data;
                app.gameView = GameView::BLACKJACK;
                continue;
            }

            // Poker's game lifecycle belongs entirely in the right-side
            // table UI. Challenge-sent / accepted messages from older
            // servers should not appear as green terminal lines in chat.
            if (Contains(msg.data, "Poker") || Contains(msg.data, "POKER"))
            {
                app.poker.status = msg.data;

                if (
                    Contains(msg.data, "accepted") ||
                    Contains(msg.data, "challenge sent")
                )
                {
                    app.gameView = GameView::POKER;
                }

                continue;
            }

            AddChatLine(app.history, msg.data, CHAT_GAME);
            continue;
        }

        if (msg.type == "ERR")
        {
            if (app.gameView == GameView::CHESS)
            {
                app.chess.status = msg.data;
                continue;
            }

            if (app.gameView == GameView::BLACKJACK)
            {
                app.blackjack.status = msg.data;
                continue;
            }

            if (app.gameView == GameView::ROULETTE)
            {
                app.roulette.status = msg.data;
                continue;
            }

            if (app.gameView == GameView::POKER)
            {
                app.poker.status = msg.data;
                continue;
            }

            if (app.gameView == GameView::ARENA)
            {
                app.arena.status = msg.data;
                continue;
            }

            AddChatLine(
                app.history,
                "[!] " + msg.data,
                ERROR_COLOR
            );

            continue;
        }

        AddChatLine(app.history, msg.data, TEXT_MUTED);
    }

    int messagesAdded = (int)app.history.size() - oldHistorySize;

    if (app.chatScrollOffset > 0 && messagesAdded > 0)
        app.chatScrollOffset += messagesAdded;
}
