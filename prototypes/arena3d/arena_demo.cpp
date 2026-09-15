#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace
{
    constexpr int SCREEN_WIDTH = 1280;
    constexpr int SCREEN_HEIGHT = 720;

    constexpr float ARENA_HALF = 19.0f;
    constexpr float TANK_RADIUS = 0.70f;
    constexpr float PLAYER_SPEED = 7.0f;
    constexpr float BOT_SPEED = 4.0f;
    constexpr float BULLET_SPEED = 24.0f;
    constexpr float BULLET_RADIUS = 0.16f;
    constexpr float PLAYER_FIRE_COOLDOWN = 0.22f;
    constexpr float BOT_FIRE_COOLDOWN = 0.75f;
    constexpr float RESPAWN_TIME = 2.0f;

    constexpr int MAX_HEALTH = 100;
    constexpr int SHOT_DAMAGE = 25;
    constexpr int KILL_LIMIT = 5;

    constexpr Color JENG_RED = {235, 64, 64, 255};
    constexpr Color JENG_YELLOW = {245, 205, 66, 255};
    constexpr Color FLOOR_COLOR = {31, 34, 41, 255};
    constexpr Color WALL_COLOR = {64, 68, 78, 255};
    constexpr Color OBSTACLE_COLOR = {82, 87, 101, 255};
    constexpr Color HUD_BG = {10, 11, 15, 215};

    struct Obstacle
    {
        BoundingBox box{};
    };

    struct Combatant
    {
        std::string name;
        Vector3 position{};
        Vector3 spawn{};
        Vector3 aimDirection{0.0f, 0.0f, -1.0f};

        float bodyYaw = 180.0f;
        float fireTimer = 0.0f;
        float respawnTimer = 0.0f;

        int health = MAX_HEALTH;
        int kills = 0;
        int deaths = 0;

        bool alive = true;
        bool human = false;
        Color color = WHITE;
    };

    struct Projectile
    {
        Vector3 position{};
        Vector3 velocity{};
        int owner = -1;
        bool active = true;
    };

    float LengthXZ(Vector3 v)
    {
        return std::sqrt(v.x * v.x + v.z * v.z);
    }

    Vector3 NormalizeXZ(Vector3 v)
    {
        float length = LengthXZ(v);
        if (length <= 0.0001f)
            return {0.0f, 0.0f, 0.0f};

        return {v.x / length, 0.0f, v.z / length};
    }

    Vector3 Add(Vector3 a, Vector3 b)
    {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }

    Vector3 Subtract(Vector3 a, Vector3 b)
    {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    Vector3 Scale(Vector3 v, float s)
    {
        return {v.x * s, v.y * s, v.z * s};
    }

    float DistanceXZ(Vector3 a, Vector3 b)
    {
        return LengthXZ(Subtract(a, b));
    }

    float DirectionYaw(Vector3 direction)
    {
        return std::atan2(direction.x, direction.z) * RAD2DEG;
    }

    Vector3 BoxCenter(const BoundingBox& box)
    {
        return {
            (box.min.x + box.max.x) * 0.5f,
            (box.min.y + box.max.y) * 0.5f,
            (box.min.z + box.max.z) * 0.5f
        };
    }

    Vector3 BoxSize(const BoundingBox& box)
    {
        return {
            box.max.x - box.min.x,
            box.max.y - box.min.y,
            box.max.z - box.min.z
        };
    }

    bool PositionBlocked(Vector3 position, const std::vector<Obstacle>& obstacles)
    {
        if (
            position.x < -ARENA_HALF + TANK_RADIUS ||
            position.x > ARENA_HALF - TANK_RADIUS ||
            position.z < -ARENA_HALF + TANK_RADIUS ||
            position.z > ARENA_HALF - TANK_RADIUS)
        {
            return true;
        }

        Vector3 center = {position.x, 0.65f, position.z};

        for (const Obstacle& obstacle : obstacles)
        {
            if (CheckCollisionBoxSphere(obstacle.box, center, TANK_RADIUS))
                return true;
        }

        return false;
    }

    void MoveCombatant(
        Combatant& combatant,
        Vector3 movement,
        float dt,
        float speed,
        const std::vector<Obstacle>& obstacles)
    {
        if (!combatant.alive)
            return;

        movement = NormalizeXZ(movement);
        if (LengthXZ(movement) <= 0.001f)
            return;

        combatant.bodyYaw = DirectionYaw(movement);

        Vector3 next = combatant.position;
        next.x += movement.x * speed * dt;
        if (!PositionBlocked(next, obstacles))
            combatant.position.x = next.x;

        next = combatant.position;
        next.z += movement.z * speed * dt;
        if (!PositionBlocked(next, obstacles))
            combatant.position.z = next.z;
    }

    Vector3 MouseWorldPoint(const Camera3D& camera, float planeY)
    {
        Ray ray = GetMouseRay(GetMousePosition(), camera);

        if (std::fabs(ray.direction.y) <= 0.0001f)
            return camera.target;

        float t = (planeY - ray.position.y) / ray.direction.y;
        if (t < 0.0f)
            t = 0.0f;

        return Add(ray.position, Scale(ray.direction, t));
    }

    bool ProjectileHitsObstacle(
        const Projectile& projectile,
        const std::vector<Obstacle>& obstacles)
    {
        for (const Obstacle& obstacle : obstacles)
        {
            if (CheckCollisionBoxSphere(obstacle.box, projectile.position, BULLET_RADIUS))
                return true;
        }

        return false;
    }

    void FireProjectile(
        int owner,
        const Combatant& shooter,
        std::vector<Projectile>& projectiles)
    {
        Vector3 direction = NormalizeXZ(shooter.aimDirection);
        if (LengthXZ(direction) <= 0.001f)
            return;

        Vector3 start = {shooter.position.x, 1.10f, shooter.position.z};
        start = Add(start, Scale(direction, 1.55f));

        Projectile projectile;
        projectile.owner = owner;
        projectile.position = start;
        projectile.velocity = Scale(direction, BULLET_SPEED);
        projectiles.push_back(projectile);
    }

    void RespawnCombatant(Combatant& combatant)
    {
        combatant.position = combatant.spawn;
        combatant.health = MAX_HEALTH;
        combatant.alive = true;
        combatant.respawnTimer = 0.0f;
        combatant.fireTimer = 0.35f;
    }

    void DamageCombatant(
        int attacker,
        int victim,
        std::vector<Combatant>& combatants)
    {
        if (
            attacker < 0 || attacker >= (int)combatants.size() ||
            victim < 0 || victim >= (int)combatants.size())
        {
            return;
        }

        Combatant& target = combatants[victim];
        if (!target.alive)
            return;

        target.health -= SHOT_DAMAGE;
        if (target.health > 0)
            return;

        target.health = 0;
        target.alive = false;
        target.respawnTimer = RESPAWN_TIME;
        target.deaths++;

        if (attacker != victim)
            combatants[attacker].kills++;
    }

    int FindNearestTarget(int seeker, const std::vector<Combatant>& combatants)
    {
        int best = -1;
        float bestDistance = 1000000.0f;

        for (int i = 0; i < (int)combatants.size(); i++)
        {
            if (i == seeker || !combatants[i].alive)
                continue;

            float distance = DistanceXZ(
                combatants[seeker].position,
                combatants[i].position);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = i;
            }
        }

        return best;
    }

    void UpdateBots(
        std::vector<Combatant>& combatants,
        std::vector<Projectile>& projectiles,
        const std::vector<Obstacle>& obstacles,
        float dt)
    {
        for (int i = 0; i < (int)combatants.size(); i++)
        {
            Combatant& bot = combatants[i];
            if (bot.human || !bot.alive)
                continue;

            int targetIndex = FindNearestTarget(i, combatants);
            if (targetIndex < 0)
                continue;

            Combatant& target = combatants[targetIndex];
            Vector3 toTarget = NormalizeXZ(Subtract(target.position, bot.position));
            bot.aimDirection = toTarget;

            float distance = DistanceXZ(bot.position, target.position);
            Vector3 movement = {0.0f, 0.0f, 0.0f};

            if (distance > 10.0f)
            {
                movement = toTarget;
            }
            else if (distance < 4.5f)
            {
                movement = Scale(toTarget, -1.0f);
            }
            else
            {
                float strafe = (i % 2 == 0) ? 1.0f : -1.0f;
                movement = {
                    -toTarget.z * strafe + toTarget.x * 0.25f,
                    0.0f,
                    toTarget.x * strafe + toTarget.z * 0.25f
                };
            }

            MoveCombatant(bot, movement, dt, BOT_SPEED, obstacles);

            bot.fireTimer -= dt;
            if (bot.fireTimer <= 0.0f && distance < 16.0f)
            {
                FireProjectile(i, bot, projectiles);
                bot.fireTimer = BOT_FIRE_COOLDOWN + (float)(i % 3) * 0.08f;
            }
        }
    }

    void UpdateRespawns(std::vector<Combatant>& combatants, float dt)
    {
        for (Combatant& combatant : combatants)
        {
            if (combatant.alive)
                continue;

            combatant.respawnTimer -= dt;
            if (combatant.respawnTimer <= 0.0f)
                RespawnCombatant(combatant);
        }
    }

    int MatchWinner(const std::vector<Combatant>& combatants)
    {
        for (int i = 0; i < (int)combatants.size(); i++)
        {
            if (combatants[i].kills >= KILL_LIMIT)
                return i;
        }

        return -1;
    }

    void ResetMatch(std::vector<Combatant>& combatants)
    {
        for (Combatant& combatant : combatants)
        {
            combatant.kills = 0;
            combatant.deaths = 0;
            RespawnCombatant(combatant);
        }
    }

    void DrawArena(const std::vector<Obstacle>& obstacles)
    {
        DrawPlane({0.0f, 0.0f, 0.0f}, {40.0f, 40.0f}, FLOOR_COLOR);

        DrawCube(
            {0.0f, 2.25f, -20.0f},
            40.0f,
            4.5f,
            1.0f,
            WALL_COLOR
            );

        DrawCube(   
            {0.0f, 2.25f, 20.0f},
            40.0f,
            4.5f,
            1.0f,
            WALL_COLOR
        );

        DrawCube(
            {-20.0f, 2.25f, 0.0f},
            1.0f,
            4.5f,
            40.0f,
            WALL_COLOR
        );

        DrawCube(
            {20.0f, 2.25f, 0.0f},
            1.0f,
            4.5f,
            40.0f,
            WALL_COLOR
        );

        for (const Obstacle& obstacle : obstacles)
        {
            DrawCubeV(BoxCenter(obstacle.box), BoxSize(obstacle.box), OBSTACLE_COLOR);
            DrawCubeWiresV(
                BoxCenter(obstacle.box),
                BoxSize(obstacle.box),
                Color{125, 130, 145, 255});
        }

        DrawGrid(40, 1.0f);
    }

    void DrawTank(const Combatant& c, Model& bodyModel, Model& turretModel)
    {
        if (!c.alive)
            return;

        DrawModelEx(
            bodyModel,
            {c.position.x, 0.48f, c.position.z},
            {0.0f, 1.0f, 0.0f},
            c.bodyYaw,
            {1.0f, 1.0f, 1.0f},
            c.color);

        float turretYaw = DirectionYaw(c.aimDirection);

        DrawModelEx(
            turretModel,
            {c.position.x, 1.02f, c.position.z},
            {0.0f, 1.0f, 0.0f},
            turretYaw,
            {1.0f, 1.0f, 1.0f},
            c.color);

        Vector3 barrelDirection = NormalizeXZ(c.aimDirection);
        Vector3 barrelStart = {c.position.x, 1.12f, c.position.z};
        Vector3 barrelEnd = Add(barrelStart, Scale(barrelDirection, 1.75f));

        DrawCylinderEx(
            barrelStart,
            barrelEnd,
            0.10f,
            0.10f,
            10,
            Color{35, 36, 43, 255});

        DrawSphere(
            {c.position.x, 1.65f, c.position.z},
            0.10f,
            c.human ? JENG_YELLOW : c.color);
    }

    void DrawProjectiles(const std::vector<Projectile>& projectiles)
    {
        for (const Projectile& projectile : projectiles)
        {
            if (!projectile.active)
                continue;

            DrawSphere(projectile.position, BULLET_RADIUS, JENG_YELLOW);
            DrawSphereWires(
                projectile.position,
                BULLET_RADIUS + 0.05f,
                6,
                6,
                JENG_RED);
        }
    }

    void DrawHud(const std::vector<Combatant>& combatants, int winner)
    {
        const Combatant& player = combatants[0];

        DrawRectangle(18, 18, 370, 116, HUD_BG);
        DrawRectangleLines(18, 18, 370, 116, JENG_RED);

        DrawText("JENG ARENA // 3D COMBAT PROTOTYPE", 32, 30, 20, JENG_YELLOW);
        DrawText(
            TextFormat("HEALTH  %d / %d", player.health, MAX_HEALTH),
            32,
            62,
            19,
            player.health > 25 ? RAYWHITE : JENG_RED);
        DrawText(
            TextFormat("KILLS   %d     DEATHS   %d", player.kills, player.deaths),
            32,
            88,
            18,
            RAYWHITE);
        DrawText(
            TextFormat("FIRST TO %d KILLS", KILL_LIMIT),
            32,
            112,
            14,
            Color{165, 168, 180, 255});

        int boardX = SCREEN_WIDTH - 250;
        int boardY = 18;
        DrawRectangle(boardX, boardY, 232, 38 + (int)combatants.size() * 28, HUD_BG);
        DrawText("FFA SCOREBOARD", boardX + 14, boardY + 10, 18, JENG_YELLOW);

        std::vector<int> order;
        for (int i = 0; i < (int)combatants.size(); i++)
            order.push_back(i);

        std::sort(order.begin(), order.end(), [&](int a, int b)
        {
            return combatants[a].kills > combatants[b].kills;
        });

        for (int row = 0; row < (int)order.size(); row++)
        {
            int i = order[row];
            const Combatant& c = combatants[i];
            DrawText(
                TextFormat("%s   %d - %d", c.name.c_str(), c.kills, c.deaths),
                boardX + 14,
                boardY + 40 + row * 28,
                17,
                c.human ? JENG_YELLOW : c.color);
        }

        DrawRectangle(18, SCREEN_HEIGHT - 62, 620, 44, HUD_BG);
        DrawText(
            "WASD Move   Mouse Aim   RMB Camera   Left Click Fire   R Reset   ESC Quit",
            30,
            SCREEN_HEIGHT - 48,
            17,
            RAYWHITE);

        if (!player.alive)
        {
            DrawText(
                TextFormat("RESPAWNING IN %.1f", std::max(0.0f, player.respawnTimer)),
                SCREEN_WIDTH / 2 - 115,
                SCREEN_HEIGHT - 105,
                22,
                JENG_RED);
        }

        if (winner >= 0)
        {
            DrawRectangle(
                SCREEN_WIDTH / 2 - 250,
                SCREEN_HEIGHT / 2 - 80,
                500,
                160,
                Color{8, 9, 12, 235});

            DrawRectangleLinesEx(
                Rectangle{
                    (float)SCREEN_WIDTH / 2.0f - 250.0f,
                    (float)SCREEN_HEIGHT / 2.0f - 80.0f,
                    500.0f,
                    160.0f},
                3.0f,
                JENG_RED);

            std::string title = combatants[winner].name + " WINS THE MATCH";
            DrawText(
                title.c_str(),
                SCREEN_WIDTH / 2 - MeasureText(title.c_str(), 30) / 2,
                SCREEN_HEIGHT / 2 - 38,
                30,
                JENG_YELLOW);

            const char* restart = "Press R to start another match";
            DrawText(
                restart,
                SCREEN_WIDTH / 2 - MeasureText(restart, 20) / 2,
                SCREEN_HEIGHT / 2 + 18,
                20,
                RAYWHITE);
        }

        Vector2 mouse = GetMousePosition();
        DrawCircleLines((int)mouse.x, (int)mouse.y, 8.0f, JENG_YELLOW);
        DrawLine((int)mouse.x - 12, (int)mouse.y, (int)mouse.x + 12, (int)mouse.y, JENG_YELLOW);
        DrawLine((int)mouse.x, (int)mouse.y - 12, (int)mouse.x, (int)mouse.y + 12, JENG_YELLOW);
    }
}

int main()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "JENG ARENA - Phase 1");
    SetTargetFPS(144);
    SetExitKey(KEY_ESCAPE);
    ShowCursor();

    Camera3D camera{};
    camera.position = {0.0f, 3.8f, 7.0f};
    camera.target = {0.0f, 0.9f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float cameraYaw = 0.0f;

    const float cameraDistance = 7.0f;
    const float cameraHeight = 3.8f;
    const float cameraTargetHeight = 0.9f;

    const float cameraSensitivity = 0.0065f;

    Vector2 savedMousePosition = {
        SCREEN_WIDTH / 2.0f,
        SCREEN_HEIGHT / 2.0f
    };

    Model bodyModel = LoadModelFromMesh(GenMeshCube(1.45f, 0.62f, 2.05f));
    Model turretModel = LoadModelFromMesh(GenMeshCube(1.05f, 0.36f, 0.95f));

    std::vector<Obstacle> obstacles = {

    // Large center structure
    {
        {
            {-3.0f, 0.0f, -3.0f},
            {3.0f, 4.0f, 3.0f}
        }
    },

    // Left wall
    {
        {
            {-14.0f, 0.0f, -2.0f},
            {-10.0f, 3.6f, 2.0f}
        }
    },

    // Right wall
    {
        {
            {10.0f, 0.0f, -2.0f},
            {14.0f, 3.6f, 2.0f}
        }
    },

    // North wall
    {
        {
            {-2.0f, 0.0f, -14.0f},
            {2.0f, 3.6f, -10.0f}
        }
    },

    // South wall
    {
        {
            {-2.0f, 0.0f, 10.0f},
            {2.0f, 3.6f, 14.0f}
        }
    },

    // Shorter cover
    {
        {
            {-13.0f, 0.0f, -13.0f},
            {-9.0f, 2.5f, -9.0f}
        }
    },

    {
        {
            {9.0f, 0.0f, 9.0f},
            {13.0f, 2.5f, 13.0f}
        }
    }
};

    std::vector<Combatant> combatants;

    Combatant player;
    player.name = "YOU";
    player.position = {-14.0f, 0.0f, 14.0f};
    player.spawn = player.position;
    player.human = true;
    player.color = JENG_RED;
    combatants.push_back(player);

    Combatant bot1;
    bot1.name = "BOT ALPHA";
    bot1.position = {14.0f, 0.0f, -14.0f};
    bot1.spawn = bot1.position;
    bot1.color = {75, 160, 255, 255};
    combatants.push_back(bot1);

    Combatant bot2;
    bot2.name = "BOT BRAVO";
    bot2.position = {14.0f, 0.0f, 14.0f};
    bot2.spawn = bot2.position;
    bot2.color = {90, 215, 130, 255};
    combatants.push_back(bot2);

    Combatant bot3;
    bot3.name = "BOT CHARLIE";
    bot3.position = {-14.0f, 0.0f, -14.0f};
    bot3.spawn = bot3.position;
    bot3.color = {190, 105, 245, 255};
    combatants.push_back(bot3);

    std::vector<Projectile> projectiles;

    while (!WindowShouldClose())
    {

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            savedMousePosition = GetMousePosition();
            HideCursor();
            SetMousePosition(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
        {
            ShowCursor();
            SetMousePosition((int)savedMousePosition.x, (int)savedMousePosition.y);
        }


        float dt = std::min(GetFrameTime(), 1.0f / 30.0f);
        Combatant& playerRef = combatants[0];
        int winner = MatchWinner(combatants);

        if (winner < 0)
        {
            if (playerRef.alive)
            {
                Vector3 movement = {0.0f, 0.0f, 0.0f};
                if (IsKeyDown(KEY_W)) movement.z -= 1.0f;
                if (IsKeyDown(KEY_S)) movement.z += 1.0f;
                if (IsKeyDown(KEY_A)) movement.x -= 1.0f;
                if (IsKeyDown(KEY_D)) movement.x += 1.0f;

                MoveCombatant(playerRef, movement, dt, PLAYER_SPEED, obstacles);
            }

            // ====================================================
// THIRD-PERSON CAMERA
// ====================================================

// Right mouse button rotates the camera only horizontally.
// Measure horizontal movement from screen center, apply it to yaw,
// then recenter the mouse for unlimited rotation.
if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
{
    const Vector2 screenCenter = {
        SCREEN_WIDTH / 2.0f,
        SCREEN_HEIGHT / 2.0f
    };

    Vector2 mousePosition = GetMousePosition();

    float mouseDeltaX =
        mousePosition.x -
        screenCenter.x;

    cameraYaw -=
        mouseDeltaX *
        cameraSensitivity;

    SetMousePosition(
        (int)screenCenter.x,
        (int)screenCenter.y
    );
}


// Always keep the tank locked in the middle of the view.
camera.target = {
    playerRef.position.x,
    playerRef.position.y + cameraTargetHeight,
    playerRef.position.z
};


// Orbit the camera around the tank on the X/Z plane.
//
// No pitch/y-axis camera control is used.
camera.position = {
    playerRef.position.x +
        sinf(cameraYaw) *
        cameraDistance,

    playerRef.position.y +
        cameraHeight,

    playerRef.position.z +
        cosf(cameraYaw) *
        cameraDistance
};

            if (playerRef.alive)
{
    // Do not change turret aim while RMB is being used
    // to rotate the camera.
    if (!IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        Vector3 mousePoint =
            MouseWorldPoint(
                camera,
                1.0f
            );

        Vector3 aim =
            NormalizeXZ(
                Subtract(
                    mousePoint,
                    playerRef.position
                )
            );

        if (LengthXZ(aim) > 0.001f)
            playerRef.aimDirection = aim;
    }

                playerRef.fireTimer -= dt;
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && playerRef.fireTimer <= 0.0f)
                {
                    FireProjectile(0, playerRef, projectiles);
                    playerRef.fireTimer = PLAYER_FIRE_COOLDOWN;
                }
            }

            UpdateBots(combatants, projectiles, obstacles, dt);

            for (Projectile& projectile : projectiles)
            {
                if (!projectile.active)
                    continue;

                projectile.position = Add(projectile.position, Scale(projectile.velocity, dt));

                if (
                    std::fabs(projectile.position.x) > 22.0f ||
                    std::fabs(projectile.position.z) > 22.0f)
                {
                    projectile.active = false;
                    continue;
                }

                if (ProjectileHitsObstacle(projectile, obstacles))
                {
                    projectile.active = false;
                    continue;
                }

                for (int i = 0; i < (int)combatants.size(); i++)
                {
                    if (i == projectile.owner || !combatants[i].alive)
                        continue;

                    Vector3 center = {
                        combatants[i].position.x,
                        0.80f,
                        combatants[i].position.z
                    };

                    if (CheckCollisionSpheres(
                            projectile.position,
                            BULLET_RADIUS,
                            center,
                            TANK_RADIUS))
                    {
                        DamageCombatant(projectile.owner, i, combatants);
                        projectile.active = false;
                        break;
                    }
                }
            }

            projectiles.erase(
                std::remove_if(
                    projectiles.begin(),
                    projectiles.end(),
                    [](const Projectile& p) { return !p.active; }),
                projectiles.end());

            UpdateRespawns(combatants, dt);
        }

        if (IsKeyPressed(KEY_R))
        {
            ResetMatch(combatants);
            projectiles.clear();
        }

        BeginDrawing();
        ClearBackground(Color{11, 12, 16, 255});

        BeginMode3D(camera);
        DrawArena(obstacles);
        for (Combatant& combatant : combatants)
            DrawTank(combatant, bodyModel, turretModel);
        DrawProjectiles(projectiles);
        EndMode3D();

        DrawHud(combatants, MatchWinner(combatants));
        EndDrawing();
    }

    ShowCursor();
    UnloadModel(bodyModel);
    UnloadModel(turretModel);
    CloseWindow();
    return 0;
}
