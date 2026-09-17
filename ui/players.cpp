#include "players.h"
#include "ui_common.h"
#include "../networking.h"
#include "../theme.h"
#include <algorithm>
#include <cctype>

namespace
{
    std::string LowerName(std::string value)
    {
        for (char& c : value) c = (char)std::tolower((unsigned char)c);
        return value;
    }

    void RefreshPlayers(AppState& app)
    {
        app.onlineUsers.clear();
        app.onlineUsersScroll = 0;
        app.onlineUsersStatus = "Loading players...";
        if (!NetSendLine("USERS_REQUEST")) app.onlineUsersStatus = NetLastError();
    }

    const char* InviteTitle(GameView game)
    {
        switch (game)
        {
            case GameView::CHESS: return "CHESS CHALLENGE";
            case GameView::BLACKJACK: return "BLACKJACK INVITE";
            case GameView::POKER: return "POKER INVITE";
            case GameView::ROULETTE: return "ROULETTE INVITE";
            case GameView::ARENA: return "JENG ARENA INVITE";
            default: return "ONLINE PLAYERS";
        }
    }

    std::string InviteCommand(GameView game, const std::string& name)
    {
        switch (game)
        {
            case GameView::CHESS: return "/chess " + name;
            case GameView::BLACKJACK: return "/blackjack " + name;
            case GameView::POKER: return "/poker " + name;
            case GameView::ROULETTE: return "/roulette " + name;
            case GameView::ARENA: return "ARENA_INVITE|" + name;
            default: return "";
        }
    }

    bool AlreadyAtTable(const AppState& app, const std::string& name)
    {
        switch (app.playerInviteGame)
        {
            case GameView::BLACKJACK:
                for (const auto& player : app.blackjack.players) if (player.name == name) return true;
                break;
            case GameView::ROULETTE:
                for (const auto& player : app.roulette.players) if (player.name == name) return true;
                break;
            case GameView::ARENA:
                for (const auto& player : app.arena.players) if (player.name == name) return true;
                break;
            case GameView::POKER: return app.poker.opponent == name;
            default: break;
        }
        return false;
    }
}

void OpenPlayerInvite(AppState& app, GameView game)
{
    app.showOnlineUsers = true;
    app.playerInviteGame = game;
    app.playerSearch.clear();
    app.playerSearchFocused = true;
    RefreshPlayers(app);
}

void OpenOnlineUsers(AppState& app)
{
    OpenPlayerInvite(app, GameView::HOME);
}

void DrawOnlineUsers(AppState& app, int canvasWidth, int canvasHeight)
{
    const bool inviting = app.playerInviteGame != GameView::HOME;
    DrawRectangle(0, 0, canvasWidth, canvasHeight, Color{0, 0, 0, 205});
    Rectangle panel = {canvasWidth / 2.0f - 300, canvasHeight / 2.0f - 265, 600, 530};
    DrawRectangleRounded(panel, 0.04f, 8, PANEL);
    DrawFittedText(InviteTitle(app.playerInviteGame), {panel.x + 24, panel.y + 24, 430, 28}, 24, JENG_YELLOW);
    if (DrawActionButton({panel.x + 484, panel.y + 20, 92, 36}, "CLOSE", false, JENG_RED))
    { app.showOnlineUsers = false; return; }
    DrawFittedText("Current players online.", {panel.x + 24, panel.y + 65, 552, 20}, 15, TEXT_MUTED);

    Rectangle search = {panel.x + 24, panel.y + 98, 552, 42};
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        app.playerSearchFocused = IsMouseInside(search);
    std::string previous = app.playerSearch;
    if (app.playerSearchFocused)
    {
        int key = GetCharPressed();
        while (key > 0)
        {
            if (key >= 32 && key <= 125 && app.playerSearch.size() < 32)
                app.playerSearch += (char)key;
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !app.playerSearch.empty()) app.playerSearch.pop_back();
    }
    if (app.playerSearch != previous) app.onlineUsersScroll = 0;
    DrawRectangleRounded(search, 0.1f, 8, PANEL_LIGHT);
    DrawRectangleRoundedLinesEx(search, 0.1f, 8, 1.5f, app.playerSearchFocused ? JENG_YELLOW : TEXT_MUTED);
    DrawFittedText(app.playerSearch.empty() ? "Search players..." : app.playerSearch,
        {search.x + 12, search.y + 12, search.width - 24, 20}, 17,
        app.playerSearch.empty() ? TEXT_MUTED : TEXT_MAIN);

    std::vector<std::string> matches;
    const std::string query = LowerName(app.playerSearch);
    for (const auto& name : app.onlineUsers)
    {
        if (inviting && (name == app.username || AlreadyAtTable(app, name))) continue;
        if (LowerName(name).find(query) != std::string::npos) matches.push_back(name);
    }
    Rectangle list = {panel.x + 24, panel.y + 154, 552, 288};
    if (IsMouseInside(list)) app.onlineUsersScroll -= (int)GetMouseWheelMove();
    app.onlineUsersScroll = std::clamp(app.onlineUsersScroll, 0, std::max(0, (int)matches.size() - 6));
    if (matches.empty() && app.onlineUsersStatus.empty())
        DrawFittedText("No matching players online.", {list.x, list.y + 12, list.width, 24}, 17, TEXT_MUTED);
    for (int row = 0; row < 6 && row + app.onlineUsersScroll < (int)matches.size(); ++row)
    {
        const std::string& name = matches[row + app.onlineUsersScroll];
        float y = list.y + row * 48;
        DrawRectangleRounded({list.x, y, list.width, 42}, 0.1f, 6, PANEL_ALT);
        DrawFittedText(name + (name == app.username ? " (you)" : ""),
            {list.x + 12, y + 12, inviting ? 400.0f : 528.0f, 20}, 17, TEXT_MAIN);
        if (inviting && DrawActionButton({list.x + 430, y + 4, 110, 34},
            "INVITE", !NetIsConnected(), JENG_YELLOW))
        {
            const std::string command = InviteCommand(app.playerInviteGame, name);
            if (!command.empty() && NetSendLine(command))
            {
                app.outboundChallengeGame = app.playerInviteGame;
                app.showOnlineUsers = false;
                if (app.playerInviteGame == GameView::ARENA)
                    app.arena.status = "Invitation requested for " + name + ".";
                return;
            }
            app.onlineUsersStatus = NetLastError();
        }
    }
    DrawFittedText(app.onlineUsersStatus, {panel.x + 24, panel.y + 450, 552, 20}, 15, TEXT_MUTED);
    if (DrawActionButton({panel.x + 24, panel.y + 480, 110, 32}, "REFRESH", false, JENG_YELLOW))
        RefreshPlayers(app);
    DrawFittedText(std::to_string(matches.size()) + " players", {panel.x + 160, panel.y + 488, 360, 20}, 15, TEXT_MUTED);
}
