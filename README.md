# JENG CHAT - Poker + PNG Cards v1

This version moves JENG CHAT another step away from the terminal-style client.
It keeps chat on the left and the game workspace on the right, adds a shared
PNG playing-card system, and implements **heads-up Texas Hold'em Poker**.

## What is included

- Existing login, chat, Help, challenge popups, Chess/Blackjack/Roulette panels
- Shared card renderer used by Poker and Blackjack
- 52 starter card PNGs + `back.png`
- Poker challenge flow
- Private hole-card packets
- Community cards
- Small blind / big blind
- Fold, Check, Call, Raise
- Flop, Turn, River, Showdown
- Hand evaluation (pair through straight flush)
- Chip stacks and pot
- Dealer button rotation
- Next-hand flow
- Resign / disconnect handling

Poker v1 is **heads-up (2 players)**. Multi-player tables and side pots are a
future expansion.

## Project structure

```text
JengChat_Poker_v1/
|-- main.cpp
|-- app_state.h
|-- protocol.cpp / protocol.h
|-- networking.cpp / networking.h
|-- ui/
|-- games/
|   |-- poker.cpp / poker.h
|   |-- blackjack.cpp / blackjack.h
|   |-- cards/
|       |-- card_renderer.cpp
|       |-- card_renderer.h
|-- assets/
|   |-- cards/
|       |-- AS.png ... KC.png
|       |-- back.png
|-- server/
|   |-- server_google_cloud_poker.cpp
|   |-- README.txt
|-- build_windows.sh
|-- package_windows.sh
```

## Build the Windows client

Run from an **MSYS2 UCRT64** terminal in the project root:

```bash
./build_windows.sh
```

Equivalent command:

```bash
g++ -std=c++17 \
    main.cpp networking.cpp protocol.cpp \
    ui/*.cpp games/*.cpp games/cards/*.cpp \
    -I. -o JengChat.exe \
    -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32 -pthread
```

## Package it for a Windows friend

```bash
./package_windows.sh
```

The package script includes the `assets/` folder automatically, so your friend
gets the card PNGs too.

## Update the Google Cloud server

The current production server must be replaced/recompiled before Poker works.
Copy:

```text
server/server_google_cloud_poker.cpp
```

to the VM, then compile:

```bash
g++ -std=c++17 server_google_cloud_poker.cpp -o jengchat_server -pthread
```

Stop the old server process, start the new binary in your normal `screen`
session, and keep TCP port `54000` open as before.

## Poker UI

Open **Poker** from the Game Hub or Help menu. If no table is active, click
**CHALLENGE PLAYER**. The prompt asks for:

1. Opponent username
2. Starting chips
3. Small blind

Example:

```text
Opponent: stack
Starting chips: 1000
Small blind: 10
```

The big blind becomes `20` automatically.

During a hand, the GUI exposes mouse buttons for:

- Fold
- Check / Call
- Raise-to amount (`-` / `+` controls)
- Next Hand
- Resign

## Card PNG naming

The card renderer expects:

```text
AS.png   = Ace of Spades
10H.png  = Ten of Hearts
QD.png   = Queen of Diamonds
KC.png   = King of Clubs
back.png = hidden card
```

The included starter deck can be replaced later with your own art. Keep the
same filenames and the C++ code will continue working.

## Privacy model

The server sends each player only their own hole cards:

```text
POKER_HOLE|AS|KS
```

The other client does **not** receive those cards. It renders `back.png` until
showdown, when the server sends a `POKER_REVEAL` packet. This prevents a
modified client from simply unhiding an opponent's private cards.

## Tested in this build

The client source was syntax-checked across the reorganized modules, and the
Linux poker server was compiled successfully. A two-client socket integration
test verified challenge/accept, private hole cards, blind posting, call/check,
street advancement, raise/call, community cards, showdown, hand evaluation,
and next-hand dealer rotation.
