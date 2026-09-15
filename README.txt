JENG CHAT — CHESS LEGAL MOVES / ROOK FIX / TURN TEXT v3
======================================================

WHAT THIS PATCH CHANGES
-----------------------

1. Selecting a chess piece asks the SERVER for every legal destination.
2. Every legal destination is highlighted:
   - empty legal squares: green tint + green center dot
   - legal captures: red tint + red outline
3. The client only sends a move if the clicked destination is in the
   authoritative legal-move list.
4. The rook movement branch on the server was rewritten explicitly and is
   shared by both white R and black r.
5. TURN text is larger:
   - TURN label: 18
   - player name: 24
6. Poker v3 lobby support, Roulette final-spin support, Blackjack final result
   behavior, and the new color-theme chat colors are preserved in protocol.cpp.


FILES TO REPLACE
----------------

Root:
    app_state.h
    protocol.cpp

Games:
    games/chess.cpp

Server source:
    server/server_google_cloud_poker_v3.cpp


NO MAIN.CPP CHANGE IS NEEDED.


CLIENT BUILD
------------

From MSYS2 UCRT64 in your repo:

    windres jengchat.rc -O coff -o jengchat_res.o

    g++ -std=c++17 main.cpp win_icon.cpp mac_bundle.cpp networking.cpp protocol.cpp ui/*.cpp games/*.cpp games/cards/*.cpp jengchat_res.o -I. -o JengChat.exe -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32 -pthread

Then:

    ./JengChat.exe


SERVER
------

Upload the replacement:

    server/server_google_cloud_poker_v3.cpp

to the Google Cloud VM and compile it there.

Typical command if you are inside the folder containing the file:

    g++ -std=c++17 server_google_cloud_poker_v3.cpp -o jengchat_server -pthread

Then stop the old server process/screen before starting the new one.

If port 54000 says "address already in use":

    sudo ss -ltnp | grep 54000
    screen -ls

Reconnect to/stop the old server before launching the new one.


HOW LEGAL MOVE HIGHLIGHTING WORKS
---------------------------------

Client selects e2:
    CHESS_LEGAL|e2

Server runs the SAME legalChessMove() validator used for actual moves and
returns something like:
    CHESS_LEGAL|e2|e3,e4

That means the green/red highlighted squares are not client guesses — they
are exactly the squares the authoritative server will accept.


ROOK TEST
---------

A rook cannot move from its starting square while its pawn is still blocking
it. For a clean white-rook test:

1. Move a2 -> a4
2. Wait for Black's move
3. Select the rook on a1

You should now see a2 and a3 highlighted if both are open (and more squares
depending on the board position).

If the rook is pinned to the king, only moves that keep the king safe will
highlight, because these are true legal moves, not merely geometric rook moves.
