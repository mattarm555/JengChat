# JENG ARENA — Phase 1

This is the first 3D gameplay prototype for JENG CHAT.

It is deliberately isolated under `prototypes/arena3d/`, so it does not replace or modify the production client.

## Current prototype

- Raylib 3D environment
- Third-person arena camera
- WASD tank movement
- Mouse aiming
- Projectile combat
- Arena collision
- Health and damage
- Death and respawn
- Four-way FFA rules
- Three AI opponents
- First to 5 kills wins
- No external 3D assets required yet

## Controls

```text
WASD        Move
Mouse       Aim turret
Left Click  Fire
R           Reset match
ESC         Quit
```

## Build

From the root of the JENG CHAT repo in MSYS2 UCRT64:

```bash
chmod +x prototypes/arena3d/build.sh
./prototypes/arena3d/build.sh
```

Run:

```bash
./prototypes/arena3d/JengArenaDemo.exe
```

## Why Phase 1 is standalone

We want to prove the realtime 3D gameplay systems without risking the stable JENG CHAT client.

Once the feel is right, Phase 2 will turn this into a real networked 1v1:

1. Move arena state/input/rendering into reusable `.h/.cpp` files.
2. Add an Arena lobby/challenge flow to JENG CHAT.
3. Add realtime server simulation.
4. Replace AI opponents with remote players.
5. Add server snapshots and client interpolation.
6. Make movement, shots, health, deaths, respawns, and score server-authoritative.
7. Expand 1v1 into 2–6 player FFA.

## Planned realtime packets

```text
ARENA_CREATE|mode|maxPlayers|killLimit
ARENA_JOIN|gameId
ARENA_START|gameId
ARENA_INPUT|sequence|moveX|moveZ|aimX|aimZ|fire
ARENA_STATE|tick|player states...
ARENA_SHOT|projectile state...
ARENA_HIT|attacker|victim|health
ARENA_KILL|attacker|victim
ARENA_RESPAWN|player|x|z
ARENA_END|winner
```

The client should send inputs, not trusted final positions. The server should own the true match state.
