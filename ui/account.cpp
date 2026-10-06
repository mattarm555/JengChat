#include "account.h"

#include "../config.h"
#include "../networking.h"
#include "../theme.h"
#include "ui_common.h"

#include <openssl/crypto.h>
#include <algorithm>

namespace {
enum class AccountPage { USERNAME, PASSWORD, DELETE_ACCOUNT };

bool panelOpen = false;
bool busy = false;
bool showPasswords = false;
bool statusError = false;
int focused = 0;
AccountPage page = AccountPage::USERNAME;
std::string fields[3];
std::string status;

void ClearValue(std::string& value) {
    OPENSSL_cleanse(value.data(), value.size());
    value.clear();
}

void ClearFields() {
    for (auto& field : fields) ClearValue(field);
    focused = 0;
    showPasswords = false;
}

void SelectPage(AccountPage selected) {
    if (busy || page == selected) return;
    page = selected;
    status.clear();
    statusError = false;
    ClearFields();
}

bool IsSecretField(int index) {
    if (page == AccountPage::USERNAME) return index == 1;
    if (page == AccountPage::PASSWORD) return true;
    return index == 0;
}

bool IsUsernameField(int index) {
    return page == AccountPage::USERNAME && index == 0;
}

int FieldCount() {
    return page == AccountPage::PASSWORD ? 3 : 2;
}

const char* FieldLabel(int index) {
    if (page == AccountPage::USERNAME)
        return index == 0 ? "NEW USERNAME" : "CURRENT PASSWORD";
    if (page == AccountPage::PASSWORD) {
        const char* labels[] = {"CURRENT PASSWORD", "NEW PASSWORD", "CONFIRM NEW PASSWORD"};
        return labels[index];
    }
    return index == 0 ? "CURRENT PASSWORD" : "TYPE DELETE TO CONFIRM";
}

void ReplaceName(std::string& value, const std::string& oldName, const std::string& newName) {
    if (value == oldName) value = newName;
}

void ApplyUsernameChange(AppState& app, const std::string& newName) {
    const std::string oldName = app.username;
    app.username = newName;
    for (auto& name : app.onlineUsers) ReplaceName(name, oldName, newName);
    ReplaceName(app.chess.whitePlayer, oldName, newName);
    ReplaceName(app.chess.blackPlayer, oldName, newName);
    ReplaceName(app.chess.turn, oldName, newName);
    ReplaceName(app.blackjack.hostName, oldName, newName);
    ReplaceName(app.blackjack.turn, oldName, newName);
    for (auto& player : app.blackjack.players) ReplaceName(player.name, oldName, newName);
    ReplaceName(app.poker.hostName, oldName, newName);
    for (auto& player : app.poker.players) ReplaceName(player.name, oldName, newName);
    ReplaceName(app.poker.turn, oldName, newName);
    ReplaceName(app.poker.dealer, oldName, newName);
    ReplaceName(app.roulette.hostName, oldName, newName);
    for (auto& player : app.roulette.players) ReplaceName(player.name, oldName, newName);
    ReplaceName(app.arena.hostName, oldName, newName);
    for (auto& player : app.arena.players) ReplaceName(player.name, oldName, newName);
    for (auto& player : app.arena.worldPlayers) ReplaceName(player.name, oldName, newName);
}
}

bool IsAccountPanelOpen() { return panelOpen; }

void OpenAccountPanel() {
    panelOpen = true;
    busy = false;
    page = AccountPage::USERNAME;
    status.clear();
    statusError = false;
    ClearFields();
}

void CancelAccountPanel() {
    if (busy) return;
    panelOpen = false;
    status.clear();
    ClearFields();
}

void DrawAccountPanel(AppState& app) {
    if (!panelOpen) return;

    DrawRectangle(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, Color{0, 0, 0, 175});
    Rectangle panel = {270, 62, 660, 526};
    DrawRectangleRounded(panel, 0.04f, 10, PANEL);
    DrawRectangleRoundedLinesEx(panel, 0.04f, 10, 2.0f, JENG_YELLOW);
    DrawFittedText("ACCOUNT", {panel.x + 28, panel.y + 22, 260, 34}, 28, JENG_RED);
    DrawFittedText("Signed in as @" + app.username,
        {panel.x + 28, panel.y + 58, 470, 22}, 16, TEXT_MUTED);

    if (DrawActionButton({panel.x + 574, panel.y + 20, 58, 32}, "X", busy, JENG_RED))
        CancelAccountPanel();

    Rectangle usernameTab = {panel.x + 28, panel.y + 96, 190, 38};
    Rectangle passwordTab = {panel.x + 235, panel.y + 96, 190, 38};
    Rectangle deleteTab = {panel.x + 442, panel.y + 96, 190, 38};
    if (DrawActionButton(usernameTab, "USERNAME", busy, JENG_YELLOW)) SelectPage(AccountPage::USERNAME);
    if (DrawActionButton(passwordTab, "PASSWORD", busy, JENG_YELLOW)) SelectPage(AccountPage::PASSWORD);
    if (DrawActionButton(deleteTab, "DELETE", busy, JENG_RED)) SelectPage(AccountPage::DELETE_ACCOUNT);

    Rectangle selected = page == AccountPage::USERNAME ? usernameTab
        : page == AccountPage::PASSWORD ? passwordTab : deleteTab;
    DrawRectangle((int)selected.x, (int)(selected.y + selected.height - 3), (int)selected.width, 3,
        page == AccountPage::DELETE_ACCOUNT ? JENG_RED : JENG_YELLOW);

    int count = FieldCount();
    float firstY = panel.y + 158;
    for (int i = 0; i < count; ++i) {
        float y = firstY + i * 76;
        DrawText(FieldLabel(i), (int)panel.x + 28, (int)y, 14,
            page == AccountPage::DELETE_ACCOUNT ? JENG_RED : JENG_YELLOW);
        Rectangle box = {panel.x + 28, y + 20, 604, 40};
        if (!busy && IsMouseInside(box) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) focused = i;
        DrawRectangleRounded(box, 0.1f, 8, PANEL_LIGHT);
        DrawRectangleRoundedLinesEx(box, 0.1f, 8, focused == i ? 2.0f : 1.0f,
            focused == i ? JENG_YELLOW : TEXT_MUTED);
        bool hidden = IsSecretField(i) && !showPasswords;
        std::string display = hidden ? std::string(fields[i].size(), '*') : fields[i];
        while (!display.empty() && MeasureText(display.c_str(), 17) > box.width - 28)
            display.erase(display.begin());
        DrawText(display.c_str(), (int)box.x + 12, (int)box.y + 11, 17, TEXT_MAIN);
        if (focused == i && !busy && ((int)(GetTime() * 2) % 2) == 0)
            DrawRectangle((int)box.x + 13 + MeasureText(display.c_str(), 17), (int)box.y + 9, 2, 22, TEXT_MAIN);
    }

    float controlsY = panel.y + 406;
    if (DrawActionButton({panel.x + 28, controlsY, 190, 34},
        showPasswords ? "HIDE PASSWORD" : "SHOW PASSWORD", busy, JENG_YELLOW))
        showPasswords = !showPasswords;

    const char* actionText = page == AccountPage::USERNAME ? "CHANGE USERNAME"
        : page == AccountPage::PASSWORD ? "CHANGE PASSWORD" : "DELETE ACCOUNT";
    bool submit = DrawActionButton({panel.x + 402, controlsY, 230, 38},
        busy ? "PROCESSING..." : actionText, busy,
        page == AccountPage::DELETE_ACCOUNT ? JENG_RED : JENG_YELLOW);

    if (page == AccountPage::USERNAME)
        DrawFittedText("Username: 3-16 letters, numbers, _ or -",
            {panel.x + 28, controlsY + 48, 604, 18}, 14, TEXT_MUTED);
    else if (page == AccountPage::PASSWORD)
        DrawFittedText("New password: 8-128 printable characters",
            {panel.x + 28, controlsY + 48, 604, 18}, 14, TEXT_MUTED);
    else
        DrawFittedText("This permanently removes your account and signs you out.",
            {panel.x + 28, controlsY + 48, 604, 18}, 14, JENG_RED);

    auto lines = WrapUIMessage(status, 14, 604, 2);
    DrawUILines(lines, panel.x + 28, controlsY + 72, 14, 18,
        statusError ? ERROR_COLOR : SUCCESS);

    if (busy) return;
    if (IsKeyPressed(KEY_TAB)) focused = (focused + 1) % count;
    int ch = GetCharPressed();
    while (ch > 0) {
        size_t limit = IsUsernameField(focused) ? 16 :
            (page == AccountPage::DELETE_ACCOUNT && focused == 1 ? 6 : 128);
        if (ch >= 32 && ch <= 126 && fields[focused].size() < limit &&
            (!IsUsernameField(focused) || IsAllowedUsernameChar((char)ch)))
            fields[focused] += (char)ch;
        ch = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !fields[focused].empty()) {
        fields[focused].back() = '\0';
        fields[focused].pop_back();
    }
    if (!(submit || IsKeyPressed(KEY_ENTER))) return;

    status.clear();
    statusError = true;
    bool sent = false;
    if (page == AccountPage::USERNAME) {
        if (fields[0].size() < 3) status = "Enter a username with at least 3 characters.";
        else if (fields[1].size() < 8) status = "Enter your current password.";
        else sent = NetChangeUsername(fields[0], fields[1]);
    }
    else if (page == AccountPage::PASSWORD) {
        if (fields[0].size() < 8) status = "Enter your current password.";
        else if (fields[1].size() < 8) status = "New password must have at least 8 characters.";
        else if (fields[1] != fields[2]) status = "New passwords do not match.";
        else sent = NetChangePassword(fields[0], fields[1]);
    }
    else {
        if (fields[0].size() < 8) status = "Enter your current password.";
        else if (fields[1] != "DELETE") status = "Type DELETE exactly to confirm account deletion.";
        else sent = NetDeleteAccount(fields[0]);
    }
    if (sent) {
        busy = true;
        statusError = false;
        status = "Processing account change...";
        ClearValue(fields[page == AccountPage::USERNAME ? 1 : 0]);
        if (page == AccountPage::PASSWORD) {
            ClearValue(fields[1]);
            ClearValue(fields[2]);
        }
    }
    else if (status.empty()) status = "Unable to send account request.";
}

bool HandleAccountPacket(AppState& app, const std::string& type, const std::string& data) {
    busy = false;
    if (type == "ACCOUNT_ERROR") {
        status = data;
        statusError = true;
        return false;
    }
    if (type == "ACCOUNT_USERNAME") {
        ApplyUsernameChange(app, data);
        status = "Username changed to @" + data + ".";
        statusError = false;
        ClearFields();
        return false;
    }
    if (type == "ACCOUNT_PASSWORD") {
        status = data.empty() ? "Password changed." : data;
        statusError = false;
        ClearFields();
        return false;
    }
    if (type == "ACCOUNT_DELETED") {
        panelOpen = false;
        ClearFields();
        NetDisconnect();
        app = AppState{};
        app.statusMessage = data.empty() ? "Account deleted." : data;
        return true;
    }
    return false;
}
