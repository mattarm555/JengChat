# JENG CHAT

Cross-platform graphical chat client and multiplayer game platform, written in C++ with [Raylib](https://www.raylib.com/).

Chat stays on screen while you play Chess, Blackjack, Poker, or Roulette. The client is a desktop app; the server owns multiplayer game state.

**Repository:** https://github.com/mattarm555/JengChat

---

## Features

### Chat

- Real-time TCP chat with usernames and timestamps
- System, game, and error messages
- Permanent chat panel while games are open
- Scrollable history and connected/offline status
- `/users` support
- Incoming game challenge popups

### Interface

- Raylib UI, resizable window, mouse-first controls
- Fixed virtual canvas (`1200×650`) scaled to the window
- F11 fullscreen
- Modal help, command, challenge, and appearance windows
- Windows application icon and macOS `.app` bundle support

### Appearance

Users can customize background, panel, text, muted text, chat colors, and red/yellow accents. Settings are stored locally:

| Platform | Path |
| --- | --- |
| Windows | `%APPDATA%\JengChat\theme.cfg` |
| macOS | `~/Library/Application Support/JengChat/theme.cfg` |
| Linux | `~/.config/JengChat/theme.cfg` |

Font customization is planned.

---

## Games

The server is authoritative. The client draws, animates, and sends input.

### Chess

Graphical board with PNG pieces. Selecting a piece asks the server for legal destinations (`CHESS_LEGAL|e2` → `CHESS_LEGAL|e2|e3,e4`), so highlights match the same rules used to validate moves.

- Mouse selection, legal-move highlights (green empty / red captures)
- Check prevention, castling, en passant, promotion
- Captured-piece display, resign, white/black orientation

### Blackjack

Graphical multiplayer, 2–6 players, PNG cards, deal animations.

- Table creation and invites
- Configurable starting chips and number of hands
- Blackjack 3:2, push, double down, one split, double after split
- Dealer hits below 17, stands on all 17
- Final results stay visible after the match

### Poker

Heads-up Texas Hold'em.

- Table/lobby, invites, starting chips and small-blind config
- Dealer rotation, blinds, private hole cards
- Flop / turn / river, fold / check / call / raise
- Server-side evaluation, showdown, next-hand flow
- Hole cards are only sent to their owner until showdown

### Roulette

European wheel (0–36), 1–6 players.

- Table creation, invites, configurable chips and rounds
- Straight numbers, red/black, odd/even, low/high, dozens
- Chip selection, bet lock/clear, host-controlled next round
- Animated wheel and final-round animation

---

## Architecture

**The server owns game state. The client owns presentation.**

| Client | Server |
| --- | --- |
| Drawing, mouse input, animations | Connected users, sessions, turn order |
| Local UI and chat display | Legal moves, cards, bets, chips |
| Appearance settings | Roulette results, poker hands, match completion |

---

## Project structure

```
JengChat/
├── .github/workflows/build-macos.yml
├── assets/          # cards, chess, poker, roulette, ui
├── games/           # chess, blackjack, poker, roulette, card renderer
├── server/          # Linux multiplayer server
├── ui/              # chat, login, help, appearance, popups
├── main.cpp
├── networking.cpp / protocol.cpp
├── Makefile
├── build_windows.sh / package_windows.sh
├── build_macos.sh / build_macos_ci.sh
└── README.md
```

---

## Networking

TCP on port **54000**. Messages are newline-delimited:

```
TYPE|data
```

Examples: `CHAT|username|message`, `SYS|message`, `GAME|message`, `ERR|message`, `READY|message`.

Game packets include `CHESS_*`, `BJ_*`, `POKER_*`, and `RLT_*` (state, legal moves, hole cards, bets, results, and so on). Handling lives in `networking.cpp` and `protocol.cpp`.

---

## Windows development

Developed with **MSYS2 UCRT64**.

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-raylib make
g++ --version
make --version
```

From the `JengChat` repo root:

```bash
make          # build JengChat.exe
make run
make clean
make rebuild
# or: ./build_windows.sh
```

The Windows build compiles the icon resource plus `main.cpp`, `win_icon.cpp`, `mac_bundle.cpp`, `networking.cpp`, `protocol.cpp`, `ui/*.cpp`, `games/*.cpp`, and `games/cards/*.cpp`.

### Windows release

```bash
make release
# or: ./package_windows.sh
```

Output:

```
dist/
├── JengChat_Windows/
└── JengChat_Windows.zip
```

The ZIP is what to send. Recipients do not need MSYS2, GCC, Raylib headers, Visual Studio, or source.

The executable is not commercially code-signed, so Windows SmartScreen may warn.

---

## macOS builds

GitHub Actions can produce macOS builds from a Windows machine.

- Workflow: `.github/workflows/build-macos.yml`
- Artifacts: `JengChat-macOS-Apple-Silicon`, `JengChat-macOS-Intel`
- Output: `JengChat.app` (Raylib linked statically)

**Actions → Build JENG CHAT for macOS → Run workflow**

Use Apple Silicon for M-series Macs and Intel for older Macs.

Ad-hoc signed: Control-click `JengChat.app` → Open, or **System Settings → Privacy & Security → Open Anyway**.

---

## Server

Linux. Typical build from the folder that contains the server source:

```bash
g++ -std=c++17 server_google_cloud_poker_v3.cpp -o jengchat_server -pthread
./jengchat_server
```

Listens on **54000**.

Keep it in `screen`:

```bash
screen -S jengchat
# detach: Ctrl+A, D
screen -ls
screen -r jengchat
```

If the port is in use, an old process is usually still bound:

```bash
sudo ss -ltnp | grep 54000
```

---

## Controls

| Key | Action |
| --- | --- |
| Mouse | Primary interaction |
| F11 | Toggle fullscreen |
| ESC | Close menus / cancel appearance changes / leave fullscreen |

ESC does not quit the app. Incoming challenges must be accepted or declined. Most game actions are graphical, not terminal commands.

---

## Goals

- User-selectable fonts and extra theme presets
- Semantic chat-message types so history recolors with the theme
- More animations and games, better lobbies
- Saved preferences, versioned Windows + macOS releases
- Apple notarization, Windows code signing, installer

Someone should be able to download JENG CHAT, connect, chat, and play without a compiler or terminal.

---

## License

No public license is specified yet. Add a `LICENSE` file if this becomes an open-source project.

## Author

Matt Armstrong
