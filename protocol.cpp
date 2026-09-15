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

        return GameView::HOME;
    }

    string ChallengeTitle(GameView game, const string& message)
    {
        if (game == GameView::CHESS)
            return "CHESS CHALLENGE";

        if (game == GameView::BLACKJACK)
            return "BLACKJACK CHALLENGE";

        if (game == GameView::POKER)
            return "POKER CHALLENGE";

        if (game == GameView::ROULETTE)
            return "ROULETTE INVITE";

        if (Contains(message, "Tic-Tac-Toe"))
            return "TIC-TAC-TOE CHALLENGE";

        return "GAME CHALLENGE";
    }

    void HandleChessPacket(AppState& app, const NetMessage& msg)
    {
        ChessClientState& chess = app.chess;

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
            // Keep the final hand on the table instead of switching to
            // a separate ENDED screen. The payout messages from the
            // final hand are already stored in bj.payoutMessages, so
            // leaving the client in RESULT phase keeps the HAND RESULTS
            // box visible while still marking the match as finished.
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

    void HandlePokerPacket(AppState& app, const NetMessage& msg)
    {
        PokerClientState& poker = app.poker;

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
                    TEXT_MAIN
                );
            }

            continue;
        }

        if (msg.type == "SYS")
        {
            AddChatLine(app.history, msg.data, JENG_YELLOW);
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
                    app.pendingChallenge.game,
                    msg.data
                );

                app.showHelpMenu = false;
                app.commandPopup.open = false;

                if (app.pendingChallenge.game != GameView::CHESS)
                    AddChatLine(app.history, msg.data, SUCCESS);

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

            AddChatLine(app.history, msg.data, SUCCESS);
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

            AddChatLine(
                app.history,
                "[!] " + msg.data,
                ERROR_COLOR
            );

            if (app.gameView == GameView::POKER)
                app.poker.status = msg.data;

            continue;
        }

        AddChatLine(app.history, msg.data, TEXT_MUTED);
    }

    int messagesAdded = (int)app.history.size() - oldHistorySize;

    if (app.chatScrollOffset > 0 && messagesAdded > 0)
        app.chatScrollOffset += messagesAdded;
}
