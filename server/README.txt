JENG CHAT - POKER SERVER UPDATE
===============================

This server is the Google Cloud / Linux-compatible JENG CHAT server with:
- Chat
- Tic-Tac-Toe
- Chess
- Blackjack
- Heads-up Texas Hold'em Poker

Poker is server-authoritative. Hole cards are sent privately to each player's
socket. The opponent receives card backs in the GUI and never receives the
other player's hole-card packet until showdown.

BUILD ON THE GOOGLE CLOUD VM
----------------------------
From the folder containing server_google_cloud_poker.cpp:

    g++ -std=c++17 server_google_cloud_poker.cpp -o jengchat_server -pthread

Then run it however you currently run JENG CHAT (for example inside screen).
The server still listens on TCP port 54000.

POKER COMMANDS USED BY THE GUI
------------------------------
/poker <username> <starting_chips> <small_blind>
/pokercheck
/pokercall
/pokerraise <total_bet>
/pokerfold
/pokernext
/resign

The graphical client hides these commands behind buttons.

POKER PACKETS
-------------
POKER_STATE
POKER_HOLE      (private to the player who owns those cards)
POKER_BOARD
POKER_REVEAL    (sent at showdown)
POKER_NOTICE
POKER_RESULT
POKER_END

Poker v1 is heads-up (2-player) No-Limit Texas Hold'em. The next expansion can
add multi-player tables, table lobbies, and side pots.
