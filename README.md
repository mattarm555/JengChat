JENG CHAT

JENG CHAT is a cross-platform graphical chat client and multiplayer game platform built in C++ with Raylib.

The application combines a permanent chat panel with mouse-driven multiplayer games, including Chess, Blackjack, Poker, and Roulette. The client is designed to feel like a real desktop application rather than a terminal interface, while the server remains authoritative for multiplayer game state.

Repository: https://github.com/mattarm555/JengChat

Features

Chat

Real-time TCP chat

Usernames and timestamps

System, game, and error messages

Permanent chat panel while games are open

Scrollable chat history

Connected/offline status indicator

/users support

Incoming game challenge popups

Graphical Interface

Built with Raylib

Resizable window

Fixed virtual resolution for consistent UI scaling

F11 fullscreen support

Mouse-first controls

Modal help, command, challenge, and appearance windows

Windows application icon

macOS application bundle support

Appearance Customization

Users can customize:

Application background

Panel colors

Main text color

Muted text color

Chat background

Normal chat text

System chat text

Game chat text

Red accent

Yellow accent

Appearance settings are stored locally.

Windows:

%APPDATA%\JengChat\theme.cfg

macOS:

~/Library/Application Support/JengChat/theme.cfg

Font customization is planned as a future extension.

Games

Chess

Chess uses a graphical board with PNG piece assets and server-authoritative move validation.

Features:

Mouse-driven piece selection

Legal-move highlighting

Green highlights for legal empty squares

Red highlights for legal captures

Selected-piece highlighting

Server-authoritative move validation

Check prevention

Castling support

En passant support

Pawn promotion

Captured-piece display

Larger turn indicator

Resign support

White/black board orientation

When a piece is selected, the client asks the server for legal moves.

Example:

CHESS_LEGAL|e2

Response:

CHESS_LEGAL|e2|e3,e4

This keeps the highlighted moves synchronized with the same rules the server uses to validate moves.

Blackjack

Graphical multiplayer Blackjack supports up to six players.

Features:

2–6 players

Table creation

Player invites

Configurable starting chips

Configurable number of hands

Server-authoritative dealing

PNG playing cards

Deal animations

Dealer hole card

Blackjack pays 3:2

Push handling

Double down

One split

Double after split

Split 21 treated as normal 21

Dealer hits below 17

Dealer stands on all 17

Betting increments

Hand-result display

Final results remain visible after the match

Poker

JENG CHAT currently supports heads-up Texas Hold'em.

Features:

Two-player tables

Table creation and lobby

Player invites

Starting-chip configuration

Small-blind configuration

Dealer rotation

Small and big blinds

Private hole cards

Flop, turn, and river

Fold

Check

Call

Raise

Server-side hand evaluation

Showdown

Pot display

Player stacks

Next-hand flow

Resign/disconnect handling

Private hole cards are only sent to the player who owns them until showdown.

Roulette

European Roulette uses numbers 0–36.

Features:

1–6 players

Table creation and invites

Configurable starting chips

Configurable number of rounds

Server-authoritative result selection

Animated roulette wheel

Chip selection

Bet clearing and locking

Straight-number bets

Red / Black

Odd / Even

Low / High

Dozens

Correct payout handling

Host-controlled next round

Final-round animation before match completion

Architecture

JENG CHAT follows a simple rule:

The server owns game state. The client owns presentation.

The client handles:

Drawing

Mouse input

Animations

Local UI state

Chat display

Appearance settings

The server handles:

Connected users

Game sessions

Turn order

Legal moves

Cards

Bets

Chips

Roulette results

Poker hands

Match completion

Project Structure

JengChat/
├── .github/
│   └── workflows/
│       └── build-macos.yml
├── assets/
│   ├── cards/
│   ├── chess/
│   ├── poker/
│   ├── roulette/
│   └── ui/
├── games/
│   ├── cards/
│   │   ├── card_renderer.cpp
│   │   └── card_renderer.h
│   ├── blackjack.cpp
│   ├── blackjack.h
│   ├── chess.cpp
│   ├── chess.h
│   ├── chess_pieces.cpp
│   ├── chess_pieces.h
│   ├── game_area.cpp
│   ├── game_area.h
│   ├── poker.cpp
│   ├── poker.h
│   ├── roulette.cpp
│   └── roulette.h
├── server/
│   └── server_google_cloud_poker_v3.cpp
├── ui/
│   ├── appearance.cpp
│   ├── appearance.h
│   ├── challenge_popup.cpp
│   ├── challenge_popup.h
│   ├── chat.cpp
│   ├── chat.h
│   ├── command_popup.cpp
│   ├── command_popup.h
│   ├── header.cpp
│   ├── header.h
│   ├── help.cpp
│   ├── help.h
│   ├── login.cpp
│   ├── login.h
│   ├── ui_common.cpp
│   └── ui_common.h
├── app_state.h
├── config.h
├── mac_bundle.cpp
├── mac_bundle.h
├── main.cpp
├── networking.cpp
├── networking.h
├── protocol.cpp
├── protocol.h
├── theme.h
├── win_icon.cpp
├── win_icon.h
├── build_windows.sh
├── build_macos_ci.sh
├── package_windows.sh
├── Makefile
├── jengchat.rc
└── README.md

Networking

JENG CHAT uses TCP on port:

54000

Messages are newline-delimited and generally follow:

TYPE|data

Examples:

CHAT|username|message
SYS|message
GAME|message
ERR|message
READY|message

Structured game packets include:

CHESS_STATE|...
CHESS_LEGAL|...
CHESS_MOVE|...

BJ_STATE|...
BJ_PAYOUT|...
BJ_END|...

POKER_STATE|...
POKER_HOLE|...
POKER_BOARD|...
POKER_RESULT|...

RLT_STATE|...
RLT_BETS|...
RLT_FINAL|...

Networking and packet handling live mainly in:

networking.cpp
protocol.cpp

Windows Development

JENG CHAT is currently developed on Windows using MSYS2 UCRT64.

Install the compiler toolchain:

pacman -S --needed mingw-w64-ucrt-x86_64-toolchain

Install Raylib:

pacman -S --needed mingw-w64-ucrt-x86_64-raylib

Install Make:

pacman -S --needed make

Verify:

g++ --version
make --version

Navigate to the repository:

cd /c/Users/jengb/Desktop/JengChatV2/JengChat

Check:

pwd
git status

Building on Windows

Build:

make

Build and run:

make run

Clean:

make clean

Fresh rebuild:

make rebuild

You can also use:

./build_windows.sh

The Windows build compiles the icon resource and all client source files, including:

main.cpp
win_icon.cpp
mac_bundle.cpp
networking.cpp
protocol.cpp
ui/*.cpp
games/*.cpp
games/cards/*.cpp

The final executable is:

JengChat.exe

Creating a Windows Release

Build a ready-to-send package with:

make release

or:

./package_windows.sh

The packaging process should:

Build the latest executable

Create a clean distribution folder

Copy JengChat.exe

Copy assets/

Copy required DLLs

Add a basic user README

Create a ZIP

Output:

dist/
├── JengChat_Windows/
└── JengChat_Windows.zip

The ZIP is the file intended for distribution.

Recipients should not need:

MSYS2

GCC / G++

Raylib development files

Visual Studio

Source code

macOS Builds

JENG CHAT can be built for macOS through GitHub Actions, even from a Windows development machine.

Workflow:

.github/workflows/build-macos.yml

Artifacts:

JengChat-macOS-Apple-Silicon
JengChat-macOS-Intel

The build produces:

JengChat.app

Raylib is built statically during CI so the recipient does not need Homebrew or Raylib installed.

Run the workflow from:

Repository
→ Actions
→ Build JENG CHAT for macOS
→ Run workflow

Use the Apple Silicon build for M-series Macs and the Intel build for older Intel Macs.

Because JENG CHAT is currently ad-hoc signed, macOS may require:

Control-click JengChat.app
→ Open
→ Open

or:

System Settings
→ Privacy & Security
→ Open Anyway

Server

The multiplayer server is designed to run on Linux.

Typical build:

g++ -std=c++17 server_google_cloud_poker_v3.cpp -o jengchat_server -pthread

Run:

./jengchat_server

The server listens on port:

54000

Running in screen

Create:

screen -S jengchat

Detach:

Ctrl+A
D

List:

screen -ls

Reconnect:

screen -r jengchat

If the port is already in use:

sudo ss -ltnp | grep 54000

An older server process is usually still holding the port.

Controls

Global:

Mouse   Primary interaction
F11     Toggle fullscreen
ESC     Close menus / cancel appearance changes / leave fullscreen

ESC does not immediately quit JENG CHAT.

Incoming game challenges require an explicit accept or decline.

Most game actions are exposed graphically instead of through terminal commands.

Git Workflow

Check changes:

git status

Stage:

git add .

Commit:

git commit -m "Describe your change"

Push:

git push

Pull:

git pull

Distribution Notes

Windows

The executable is not currently signed with a commercial Windows code-signing certificate, so Windows SmartScreen may show a warning.

macOS

The application is currently ad-hoc signed and may trigger Gatekeeper.

These are signing/distribution warnings, not development-environment requirements.

Development Goals

Planned or useful future improvements:

User-selectable fonts

Additional theme presets

Semantic chat-message types so existing history recolors instantly

More game animations

More multiplayer games

Improved matchmaking/lobbies

Saved user preferences

Automated versioned Windows + macOS releases

Apple notarization

Windows code signing

Installer support

Design Philosophy

JENG CHAT aims to combine:

A lightweight desktop chat client

Multiplayer games that feel native to the interface

A simple server-authoritative architecture

The long-term goal is for someone to download JENG CHAT, open it, connect, chat, and play without needing a terminal, compiler, or development environment.

License

No public license is currently specified.

If JENG CHAT becomes a public/open-source project, add a LICENSE file and update this section.

Author

Created and developed by Matt Armstrong.