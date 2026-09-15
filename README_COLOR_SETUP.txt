JENG CHAT COLOR SETTINGS v1
============================

This patch gives JENG CHAT runtime color customization first.
Fonts are intentionally NOT included yet.

FILES
=====

Replace:
    theme.h

Add:
    ui/appearance.h
    ui/appearance.cpp

Your normal build already uses ui/*.cpp, so appearance.cpp is automatically
compiled on Windows and macOS.

WHAT CAN BE CHANGED
===================

- Background
- Panel color
- Main text
- Muted text
- Chat background
- Normal chat text
- System chat text
- Game chat text
- Red accent
- Yellow accent

The color editor uses RGB sliders and a live chat preview.

Settings are saved per-user:

Windows:
    %APPDATA%\JengChat\theme.cfg

macOS:
    ~/Library/Application Support/JengChat/theme.cfg

Linux:
    ~/.config/JengChat/theme.cfg


============================================================
1. MAIN.CPP
============================================================

Near your other UI includes add:

    #include "ui/appearance.h"

At startup, BEFORE InitWindow(), call:

    LoadAppearanceSettings();

If your main() currently starts with:
    PrepareMacBundleWorkingDirectory();

then this is a good order:

    PrepareMacBundleWorkingDirectory();
    LoadAppearanceSettings();

    SetConfigFlags(...);
    InitWindow(...);


Near the END of your virtual-canvas drawing code, after your normal UI/modal
drawing, call:

    DrawAppearanceSettings();

It needs to be before:
    EndTextureMode();

so it appears inside the same 1000x650 virtual UI.


If main.cpp has an interactionsBlocked boolean, add:

    || IsAppearanceSettingsOpen()

to it so clicks do not pass through the appearance window.


============================================================
2. UI/HELP.CPP
============================================================

Add this include:

    #include "appearance.h"

Your modern Help menu currently has a lower-right informational card called
CONTROLS. Re-use that card as the Appearance entry.

Where the hover booleans are created, add:

    bool appearanceHover = IsMouseInside(controlsInfo);

Replace the old CONTROLS informational drawing block with:

    DrawMenuCard(
        controlsInfo,
        "APPEARANCE",
        "Customize client colors",
        JENG_RED,
        appearanceHover
    );

Then in the ACTIONS section add:

    if (CardClicked(controlsInfo))
    {
        OpenAppearanceSettings();
        CloseHelp(app);
        return;
    }

Now Help -> APPEARANCE opens the color editor.


============================================================
3. PROTOCOL.CPP — CHAT COLORS
============================================================

Inside ProcessIncomingMessages(AppState& app):

NORMAL CHAT
-----------

Change:

    AddChatLine(
        app.history,
        "[" + CurrentTime() + "] " + sender + ": " + message,
        TEXT_MAIN
    );

to:

    AddChatLine(
        app.history,
        "[" + CurrentTime() + "] " + sender + ": " + message,
        CHAT_TEXT
    );


SYSTEM CHAT
-----------

Change:

    AddChatLine(app.history, msg.data, JENG_YELLOW);

to:

    AddChatLine(app.history, msg.data, CHAT_SYSTEM);


GENERIC GAME CHAT
-----------------

Where a normal GAME packet ultimately gets added with SUCCESS:

    AddChatLine(app.history, msg.data, SUCCESS);

change that normal/final fallback to:

    AddChatLine(app.history, msg.data, CHAT_GAME);

You do NOT need to change game-specific green/red payout UI.


============================================================
4. UI/CHAT.CPP — CHAT BACKGROUND
============================================================

In DrawChatPanel(), find the rectangle that draws the main chat/history area.

It probably currently uses PANEL or PANEL_ALT, for example:

    DrawRectangleRounded(historyArea, ..., PANEL);

For the actual message-history background, use:

    CHAT_BG

instead.

Do not blindly replace every PANEL in chat.cpp — buttons/input boxes can
remain PANEL/PANEL_LIGHT. Only use CHAT_BG for the message/history area.


============================================================
5. BUILD
============================================================

No server changes are required.

No build command changes are required because both of your current builds use:

    ui/*.cpp

Windows:
    run your normal build_windows.sh

macOS:
    push to GitHub and run the existing macOS GitHub Actions workflow.


============================================================
6. IMPORTANT NOTE ABOUT OLD CHAT MESSAGES
============================================================

Your ChatLine currently stores a Color value when the message is received.
Therefore:

- messages received AFTER changing a chat color use the new color
- messages already in history keep the color they were originally assigned

When we do the font pass, I recommend also upgrading ChatLine to store a
semantic message type (CHAT / SYSTEM / GAME / ERROR) instead of a fixed Color.
Then old chat history will recolor instantly when the theme changes.
