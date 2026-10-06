#include "login.h"
#include "../config.h"
#include "../networking.h"
#include "../theme.h"
#include "ui_common.h"
#include <openssl/crypto.h>
#include <algorithm>

namespace {
void ClearSecret(std::string& value) { OPENSSL_cleanse(value.data(), value.size()); value.clear(); }
}

void DrawLoginScreen(AppState& app)
{
    static bool registering = false, showPassword = false, submitted = false;
    static int focused = 0;
    static std::string password, confirmation;
    if (submitted && !NetIsConnecting())
    {
        submitted = false;
        ClearSecret(password); ClearSecret(confirmation);
        if (NetIsConnected())
        {
            app.history.clear();
            for (const auto& message : NetPollMessages())
            {
                if (message.type == "AUTH_OK") app.username = message.data;
                else if (message.type == "SYS") AddChatLine(app.history, message.data, CHAT_SYSTEM);
            }
            app.statusMessage.clear();
            app.screen = AppScreen::MAIN;
            app.gameView = GameView::HOME;
            showPassword = false;
            return;
        }
        app.statusMessage = NetLastError();
    }
    bool busy = NetIsConnecting();
    Rectangle panel = {WINDOW_WIDTH / 2.0f - 280, 24, 560, WINDOW_HEIGHT - 48.0f};
    DrawRectangleRounded(panel, 0.04f, 10, PANEL);
    DrawRectangleRoundedLinesEx(panel, 0.04f, 10, 2.0f, JENG_YELLOW);
    DrawFittedText("JENG CHAT", {panel.x + 32, panel.y + 26, 496, 44}, 38, JENG_RED);
    DrawFittedText("Connect. Chat. Play.", {panel.x + 32, panel.y + 80, 496, 22}, 18, TEXT_MUTED);
    bool signIn = DrawActionButton({panel.x + 32, panel.y + 117, 238, 38}, "SIGN IN", busy, JENG_YELLOW);
    bool create = DrawActionButton({panel.x + 290, panel.y + 117, 238, 38}, "CREATE ACCOUNT", busy, JENG_YELLOW);
    if (signIn || create) {
        registering = create; ClearSecret(password); ClearSecret(confirmation);
        app.statusMessage.clear(); focused = 0; showPassword = false;
    }
    DrawRectangle((int)panel.x + (registering ? 290 : 32), (int)panel.y + 157, 238, 3, JENG_YELLOW);
    const char* labels[] = {"USERNAME", "PASSWORD", "CONFIRM PASSWORD"};
    std::string* values[] = {&app.username, &password, &confirmation};
    int count = registering ? 3 : 2;
    for (int i = 0; i < count; ++i) {
        float y = panel.y + 180 + i * 82;
        DrawText(labels[i], (int)panel.x + 32, (int)y, 15, JENG_YELLOW);
        Rectangle box = {panel.x + 32, y + 22, 496, 42};
        if (!busy && IsMouseInside(box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) focused = i;
        DrawRectangleRounded(box, 0.1f, 8, PANEL_LIGHT);
        DrawRectangleRoundedLinesEx(box, 0.1f, 8, focused == i ? 2.0f : 1.0f, focused == i ? JENG_YELLOW : TEXT_MUTED);
        std::string display = i == 0 || showPassword ? *values[i] : std::string(values[i]->size(), '*');
        while (!display.empty() && MeasureText(display.c_str(), 18) > box.width - 32) display.erase(display.begin());
        DrawText(display.c_str(), (int)box.x + 12, (int)box.y + 12, 18, TEXT_MAIN);
        if (focused == i && !busy && ((int)(GetTime() * 2) % 2) == 0)
            DrawRectangle((int)box.x + 13 + MeasureText(display.c_str(), 18), (int)box.y + 10, 2, 22, TEXT_MAIN);
    }
    float controlsY = panel.y + (registering ? 430 : 348);
    if (DrawActionButton({panel.x + 32, controlsY, 190, 30}, showPassword ? "HIDE PASSWORD" : "SHOW PASSWORD", busy, JENG_YELLOW))
        showPassword = !showPassword;
    DrawFittedText("Username: 3-16 letters, numbers, _ or -", {panel.x + 32, controlsY + 42, 496, 18}, 14, TEXT_MUTED);
    DrawFittedText("Password: 8-128 characters", {panel.x + 32, controlsY + 62, 496, 18}, 14, TEXT_MUTED);
    Rectangle submit = {panel.x + 288, controlsY, 240, 38};
    bool clicked = DrawActionButton(submit, busy ? "CONNECTING..." : registering ? "CREATE & SIGN IN" : "SIGN IN", busy, JENG_RED);
    if (busy && DrawActionButton({panel.x + 288, controlsY + 45, 240, 32}, "CANCEL", false, JENG_YELLOW)) {
        NetDisconnect(); submitted = false; ClearSecret(password); ClearSecret(confirmation);
        app.statusMessage = "Sign-in cancelled.";
    }
    auto errors = WrapUIMessage(app.statusMessage, 14, 496, 3);
    DrawUILines(errors, panel.x + 32, panel.y + 528, 14, 18, ERROR_COLOR);
    if (busy) return;
    if (IsKeyPressed(KEY_TAB)) focused = (focused + 1) % count;
    int ch = GetCharPressed();
    while (ch > 0) {
        size_t limit = focused == 0 ? 16 : 128;
        if (ch >= 32 && ch <= 126 && values[focused]->size() < limit &&
            (focused != 0 || IsAllowedUsernameChar((char)ch))) *values[focused] += (char)ch;
        ch = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !values[focused]->empty()) {
        values[focused]->back() = '\0'; values[focused]->pop_back();
    }
    if (!(clicked || IsKeyPressed(KEY_ENTER))) return;
    if (app.username.size() < 3) { app.statusMessage = "Enter a username of at least 3 characters."; return; }
    if (password.size() < 8) { app.statusMessage = "Use a password with at least 8 characters."; return; }
    if (registering && password != confirmation) { app.statusMessage = "Passwords do not match."; return; }
    app.statusMessage.clear();
    submitted = NetConnect(SERVER_IP, SERVER_PORT, app.username, password, registering);
    ClearSecret(password); ClearSecret(confirmation);
    if (!submitted) app.statusMessage = NetLastError();
}
