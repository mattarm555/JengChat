#pragma once

#include "raylib.h"

#include <string>
#include <vector>

enum class AppScreen
{
    LOGIN,
    MAIN
};

enum class GameView
{
    HOME,
    CHESS,
    BLACKJACK,
    POKER,
    ROULETTE,
    ARENA
};

struct ChatLine
{
    std::string text;
    Color color;
};

struct CommandPromptState
{
    bool open = false;

    std::string title;
    std::string description;
    std::string command;

    int fieldCount = 0;
    std::string fieldLabels[3];
    std::string fieldValues[3];

    int activeField = 0;
    std::string error;
};

struct PendingChallengeState
{
    bool active = false;
    std::string title = "GAME CHALLENGE";
    std::string message;
    std::string error;
    GameView game = GameView::HOME;
};




struct BlackjackHandClientState
{
    std::vector<std::string> cards;
    int bet = 0;
    int value = 0;
    bool done = false;
    bool busted = false;
    bool doubled = false;
    bool fromSplit = false;
};

struct BlackjackPlayerClientState
{
    std::string name;
    int chips = 0;
    bool betPlaced = false;
    int pendingBet = 0;
    std::vector<BlackjackHandClientState> hands;
};

struct BlackjackDealEvent
{
    std::string target;   // DEALER, P1, P2
    int handIndex = 0;
    int cardIndex = 0;
    std::string card;     // AS, 10H, -- for hidden
    float progress = 0.0f;
};

struct BlackjackClientState
{
    bool active = false;
    bool dealerRevealed = false;
    bool awaitingNextHand = false;

    std::string phase = "WAITING"; // WAITING, LOBBY, BETTING, PLAYING, RESULT
    std::string hostName;
    std::string turn;
    int turnHandIndex = 0;
    int currentHand = 0;
    int totalHands = 0;
    int startingChips = 0;
    int maxPlayers = 6;

    std::vector<BlackjackPlayerClientState> players;
    std::vector<std::string> dealerCards;

    std::string status = "Invite players to a Blackjack match.";
    std::vector<std::string> payoutMessages;
    std::vector<BlackjackDealEvent> dealQueue;

    int betAmount = 10;
    int betStep = 10;
};

struct ChessClientState
{
    bool active = false;

    // 64 characters, row 0 = rank 8, row 7 = rank 1.
    // Empty squares are represented by '.'.
    std::string board =
        "rnbqkbnr"
        "pppppppp"
        "........"
        "........"
        "........"
        "........"
        "PPPPPPPP"
        "RNBQKBNR";

    std::string whitePlayer;
    std::string blackPlayer;
    std::string turn;
    std::string yourColor;
    std::string status = "Challenge a player to start Chess.";

    int selectedSquare = -1;

    // Legal destinations for the currently selected piece.
    // These are sent by the authoritative server so highlights always
    // match the moves the server will actually accept.
    std::vector<int> legalMoves;
    int legalMoveSource = -1;
    bool legalMovesLoaded = false;
};

struct PokerClientState
{
    bool tableActive = false;
    bool handActive = false;

    // Table lifecycle is separate from the poker street.
    // WAITING -> LOBBY -> PLAYING / RESULT -> ENDED
    std::string tablePhase = "WAITING";

    std::string hostName;
    std::string opponent;
    std::string stage = "WAITING";
    std::string turn;
    std::string dealer;
    std::string status = "Create a Poker table to begin.";

    std::vector<std::string> holeCards;
    std::vector<std::string> communityCards;
    std::vector<std::string> opponentCards;
    bool opponentRevealed = false;

    int yourStack = 0;
    int opponentStack = 0;
    int pot = 0;
    int yourBet = 0;
    int opponentBet = 0;
    int currentBet = 0;
    int startingChips = 0;
    int smallBlind = 0;
    int bigBlind = 0;
    int handNumber = 0;
    int lastRaiseSize = 0;

    int raiseTarget = 0;

    // Mouse-first raise increment selector.
    // The + / - controls use one of these chip steps:
    // 10, 50, or 100.
    int raiseStep = 10;
};


struct RouletteBetClientState
{
    std::string type;
    int value = 0;
    int amount = 0;
};

struct RoulettePlayerClientState
{
    std::string name;
    int chips = 0;
    bool ready = false;
    int totalBet = 0;
};

struct RouletteClientState
{
    bool active = false;
    bool animating = false;

    // Final-round completion is deliberately delayed on the client
    // until the wheel has finished spinning and the winning number
    // has been visible briefly.
    bool matchEndPending = false;
    bool resultHoldStarted = false;

    std::string phase = "WAITING"; // WAITING, LOBBY, BETTING, RESULT, ENDED
    std::string hostName;
    std::string status = "Create a Roulette table to begin.";
    std::string pendingEndStatus;

    int startingChips = 1000;
    int currentRound = 0;
    int totalRounds = 0;
    int lastResult = -1;
    int maxPlayers = 6;

    int chipAmount = 10;

    std::vector<RoulettePlayerClientState> players;
    std::vector<RouletteBetClientState> bets;
    std::vector<std::string> payoutMessages;

    float spinStartTime = 0.0f;
    float spinDuration = 4.8f;
    float wheelStartRotation = 0.0f;
    float wheelRotation = 0.0f;
    float resultHoldStartTime = 0.0f;
    float resultHoldDuration = 1.8f;
};


struct ArenaLobbyPlayerClientState
{
    std::string name;
    bool ready = false;
    int colorIndex = -1;
    int team = -1;
};

struct ArenaWorldPlayerClientState
{
    std::string name;
    float x = 0.0f;
    float z = 0.0f;
    float bodyYaw = 180.0f;
    float aimYaw = 180.0f;
    int colorIndex = -1;
    int team = -1;
    int health = 100;
    bool alive = true;
    int kills = 0;
    int deaths = 0;
    int damageDealt = 0;
    int damageTaken = 0;
    float respawnTimer = 0.0f;
};

struct ArenaProjectileClientState
{
    float x = 0.0f;
    float y = 1.10f;
    float z = 0.0f;
};

struct ArenaMineClientState
{
    float x = 0.0f;
    float z = 0.0f;
    int colorIndex = -1;
    int team = -1;
};

struct ArenaClientState
{
    bool active = false;
    bool startSignalReceived = false;
    bool matchActive = false;

    std::string phase = "WAITING"; // WAITING, LOBBY, STARTING, PLAYING, POSTGAME, ENDED
    std::string hostName;
    std::string mode = "SCORE_FFA";
    std::string status = "Create an Arena lobby to begin.";

    int maxPlayers = 0;
    int scoreLimit = 5;
    int timeLimitSeconds = 180;

    // Incremented every time the server publishes an Arena world snapshot.
    int worldSequence = 0;
    float timeRemainingSeconds = 0.0f;

    std::vector<ArenaLobbyPlayerClientState> players;
    std::vector<ArenaWorldPlayerClientState> worldPlayers;
    std::vector<ArenaProjectileClientState> worldProjectiles;
    std::vector<ArenaMineClientState> worldMines;
};

struct AppState
{
    bool showOnlineUsers = false;
    GameView playerInviteGame = GameView::HOME;
    std::string playerSearch;
    bool playerSearchFocused = true;
    std::vector<std::string> onlineUsers;
    std::string onlineUsersStatus;
    int onlineUsersScroll = 0;

    AppScreen screen = AppScreen::LOGIN;
    GameView gameView = GameView::HOME;

    std::string username;
    std::string statusMessage;
    std::string chatInput;

    std::vector<ChatLine> history;
    int chatScrollOffset = 0;

    bool showHelpMenu = false;
    CommandPromptState commandPopup;
    PendingChallengeState pendingChallenge;
    PokerClientState poker;
    ChessClientState chess;
    BlackjackClientState blackjack;
    RouletteClientState roulette;
    ArenaClientState arena;

    // Useful later when the server sends a generic "accepted your challenge"
    // message after this client initiated a challenge.
    GameView outboundChallengeGame = GameView::HOME;
};
