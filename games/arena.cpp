#include "raylib.h"
#include "arena.h"
#include "../networking.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    constexpr int SCREEN_WIDTH = 1280;
    constexpr int SCREEN_HEIGHT = 720;

    struct WindowedState
    {
        int width = SCREEN_WIDTH;
        int height = SCREEN_HEIGHT;
        int x = 100;
        int y = 100;
    };

    Rectangle GetArenaViewport()
    {
        const float windowWidth =
            (float)GetScreenWidth();

        const float windowHeight =
            (float)GetScreenHeight();

        float scale = std::min(
            windowWidth / (float)SCREEN_WIDTH,
            windowHeight / (float)SCREEN_HEIGHT
        );

        if (scale <= 0.0f)
            scale = 1.0f;

        const float width =
            SCREEN_WIDTH * scale;

        const float height =
            SCREEN_HEIGHT * scale;

        return Rectangle{
            (windowWidth - width) * 0.5f,
            (windowHeight - height) * 0.5f,
            width,
            height
        };
    }

    Vector2 GetArenaMousePosition()
    {
        Vector2 mouse = GetMousePosition();
        Rectangle viewport = GetArenaViewport();

        if (viewport.width <= 0.0f || viewport.height <= 0.0f)
            return mouse;

        return Vector2{
            (mouse.x - viewport.x) *
                ((float)SCREEN_WIDTH / viewport.width),

            (mouse.y - viewport.y) *
                ((float)SCREEN_HEIGHT / viewport.height)
        };
    }

    void CenterArenaMouse()
    {
        Rectangle viewport = GetArenaViewport();

        SetMousePosition(
            (int)(viewport.x + viewport.width * 0.5f),
            (int)(viewport.y + viewport.height * 0.5f)
        );
    }

    void DrawArenaRenderTarget(
        const RenderTexture2D& target)
    {
        Rectangle viewport = GetArenaViewport();

        Rectangle source = {
            0.0f,
            0.0f,
            (float)target.texture.width,
            -(float)target.texture.height
        };

        DrawTexturePro(
            target.texture,
            source,
            viewport,
            Vector2{0.0f, 0.0f},
            0.0f,
            WHITE
        );
    }

    void ToggleArenaFullscreen(
        WindowedState& windowed)
    {
        if (!IsWindowFullscreen())
        {
            windowed.width = GetScreenWidth();
            windowed.height = GetScreenHeight();

            Vector2 position =
                GetWindowPosition();

            windowed.x = (int)position.x;
            windowed.y = (int)position.y;

            const int monitor =
                GetCurrentMonitor();

            SetWindowSize(
                GetMonitorWidth(monitor),
                GetMonitorHeight(monitor)
            );

            ToggleFullscreen();
        }
        else
        {
            ToggleFullscreen();

            SetWindowSize(
                windowed.width,
                windowed.height
            );

            SetWindowPosition(
                windowed.x,
                windowed.y
            );
        }
    }

    constexpr float ARENA_HALF = 19.0f;
    constexpr float TANK_RADIUS = 0.82f;
    constexpr float PLAYER_SPEED = 7.0f;
    constexpr float BOT_SPEED = 4.0f;
    constexpr float BULLET_SPEED = 36.0f;
    constexpr float BULLET_RADIUS = 0.16f;
    constexpr float MINE_RADIUS = 0.42f;
    constexpr float MINE_TRIGGER_RADIUS = 0.42f;
    constexpr float PLAYER_FIRE_COOLDOWN = 0.16f;
    constexpr float BOT_FIRE_COOLDOWN = 0.75f;
    constexpr float RESPAWN_TIME = 2.0f;
    constexpr int MAX_LOCAL_MINES_PER_PLAYER = 3;

    constexpr int MAX_HEALTH = 100;
    constexpr int SHOT_DAMAGE = 25;
    constexpr int MINE_DAMAGE = 100;

    constexpr int DEFAULT_SCORE_LIMIT = 5;
    constexpr float DEFAULT_TIME_LIMIT = 180.0f;

    constexpr Color JENG_RED = {235, 64, 64, 255};
    constexpr Color JENG_YELLOW = {245, 205, 66, 255};

    // Team modes use fixed team colors so affiliation is readable instantly.
    constexpr Color TEAM_RED = {235, 64, 64, 255};
    constexpr Color TEAM_BLUE = {75, 160, 255, 255};
    constexpr Color FLOOR_COLOR = {31, 34, 41, 255};
    constexpr Color WALL_COLOR = {64, 68, 78, 255};
    constexpr Color OBSTACLE_COLOR = {82, 87, 101, 255};
    constexpr Color HUD_BG = {10, 11, 15, 215};

    // 16 distinct player colors. Multiplayer Arena will make these
    // server-authoritative so two players can never reserve the same color.
    constexpr int ARENA_COLOR_COUNT = 16;

    constexpr Color ARENA_COLORS[ARENA_COLOR_COUNT] = {
        {235,  64,  64, 255},  // Jeng Red
        { 75, 160, 255, 255},  // Blue
        { 90, 215, 130, 255},  // Green
        {190, 105, 245, 255},  // Purple
        {255, 145,  70, 255},  // Orange
        { 70, 220, 220, 255},  // Cyan
        {245, 205,  66, 255},  // Gold
        {255, 105, 180, 255},  // Pink
        {135, 225,  70, 255},  // Lime
        {255,  90, 130, 255},  // Rose
        { 95, 120, 255, 255},  // Indigo
        { 65, 205, 165, 255},  // Mint
        {230, 125, 215, 255},  // Magenta
        {235, 235, 240, 255},  // White
        {170, 110,  65, 255},  // Bronze
        {145, 150, 165, 255}   // Slate
    };

    constexpr const char* ARENA_COLOR_NAMES[ARENA_COLOR_COUNT] = {
        "JENG RED", "BLUE", "GREEN", "PURPLE",
        "ORANGE", "CYAN", "GOLD", "PINK",
        "LIME", "ROSE", "INDIGO", "MINT",
        "MAGENTA", "WHITE", "BRONZE", "SLATE"
    };

    struct Obstacle
    {
        BoundingBox box{};
    };

    enum class ArenaMode
    {
        SCORE_FFA,
        TIME_FFA,
        DUEL,
        TEAM_2V2,
        TEAM_3V3
    };

    struct MatchSettings
    {
        ArenaMode mode = ArenaMode::SCORE_FFA;
        int playerCount = 4;
        int scoreLimit = DEFAULT_SCORE_LIMIT;
        float timeLimitSeconds = DEFAULT_TIME_LIMIT;
        int selectedColorIndex = 0;
    };

    struct MatchResult
    {
        bool finished = false;
        bool draw = false;
        int winnerPlayer = -1;
        int winnerTeam = -1;
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
        int damageDealt = 0;
        int damageTaken = 0;

        bool alive = true;
        bool human = false;
        int team = -1;
        int colorIndex = 0;
        Color color = WHITE;
    };

    struct Projectile
    {
        Vector3 position{};
        Vector3 velocity{};
        int owner = -1;
        bool active = true;
    };

    struct Mine
    {
        Vector3 position{};
        int owner = -1;
        int colorIndex = -1;
        int team = -1;
        Color color = WHITE;
        bool active = true;
    };

    struct DeathExplosion
    {
        Vector3 position{};
        Color tankColor = WHITE;
        float age = 0.0f;
        float duration = 0.85f;
    };

    struct ExplosionParticle
    {
        Vector3 position{};
        Vector3 velocity{};
        Color color = WHITE;
        float life = 0.0f;
        float maxLife = 0.0f;
        float radius = 0.10f;
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


    float Length3D(Vector3 v)
    {
        return std::sqrt(
            v.x * v.x +
            v.y * v.y +
            v.z * v.z
        );
    }

    const char* ArenaModeName(ArenaMode mode)
    {
        switch (mode)
        {
            case ArenaMode::SCORE_FFA: return "SCORE FFA";
            case ArenaMode::TIME_FFA:  return "TIME FFA";
            case ArenaMode::DUEL:      return "DUEL";
            case ArenaMode::TEAM_2V2:  return "TEAMS 2v2";
            case ArenaMode::TEAM_3V3:  return "TEAMS 3v3";
        }

        return "UNKNOWN";
    }


    const char* ArenaModePacketName(ArenaMode mode)
    {
        switch (mode)
        {
            case ArenaMode::SCORE_FFA: return "SCORE_FFA";
            case ArenaMode::TIME_FFA:  return "TIME_FFA";
            case ArenaMode::DUEL:      return "DUEL";
            case ArenaMode::TEAM_2V2:  return "TEAM_2V2";
            case ArenaMode::TEAM_3V3:  return "TEAM_3V3";
        }

        return "SCORE_FFA";
    }

    ArenaMode ArenaModeFromPacketName(
        const std::string& mode)
    {
        if (mode == "TIME_FFA")
            return ArenaMode::TIME_FFA;
        if (mode == "DUEL")
            return ArenaMode::DUEL;
        if (mode == "TEAM_2V2")
            return ArenaMode::TEAM_2V2;
        if (mode == "TEAM_3V3")
            return ArenaMode::TEAM_3V3;

        return ArenaMode::SCORE_FFA;
    }

    Vector3 ArenaDirectionFromYaw(float yawDegrees)
    {
        float radians = yawDegrees * DEG2RAD;

        return {
            std::sin(radians),
            0.0f,
            std::cos(radians)
        };
    }

    float ArenaLerpAngleDegrees(
        float current,
        float target,
        float amount)
    {
        float delta =
            std::fmod(
                target - current + 540.0f,
                360.0f
            ) -
            180.0f;

        return current + delta * amount;
    }

    bool IsTeamMode(const MatchSettings& settings)
    {
        return
            settings.mode == ArenaMode::TEAM_2V2 ||
            settings.mode == ArenaMode::TEAM_3V3;
    }

    bool IsTimedMode(const MatchSettings& settings)
    {
        return settings.mode == ArenaMode::TIME_FFA;
    }

    void NormalizeSettings(MatchSettings& settings)
    {
        settings.scoreLimit =
            std::max(1, std::min(settings.scoreLimit, 50));

        settings.timeLimitSeconds =
            std::max(60.0f, std::min(settings.timeLimitSeconds, 600.0f));

        settings.selectedColorIndex =
            std::max(0, std::min(settings.selectedColorIndex, ARENA_COLOR_COUNT - 1));

        switch (settings.mode)
        {
            case ArenaMode::DUEL:
                settings.playerCount = 2;
                break;

            case ArenaMode::TEAM_2V2:
                settings.playerCount = 4;
                break;

            case ArenaMode::TEAM_3V3:
                settings.playerCount = 6;
                break;

            default:
                settings.playerCount =
                    std::max(2, std::min(settings.playerCount, 6));
                break;
        }
    }

    int TeamScore(
        const std::vector<Combatant>& combatants,
        int team)
    {
        int score = 0;

        for (const Combatant& c : combatants)
        {
            if (c.team == team)
                score += c.kills;
        }

        return score;
    }

    bool AreEnemies(
        const Combatant& a,
        const Combatant& b,
        const MatchSettings& settings)
    {
        if (!IsTeamMode(settings))
            return true;

        return a.team != b.team;
    }

    std::string FormatMatchTime(float seconds)
    {
        int totalSeconds =
            std::max(0, (int)std::ceil(seconds));

        int minutes = totalSeconds / 60;
        int remainder = totalSeconds % 60;

        char buffer[32];

        std::snprintf(
            buffer,
            sizeof(buffer),
            "%d:%02d",
            minutes,
            remainder
        );

        return buffer;
    }

    // Keep the third-person camera in front of solid geometry.
    // The tank itself is NOT moved by this function. Only the camera
    // is shortened along the line from the tank to its desired position.
    Vector3 ResolveCameraCollision(
        Vector3 target,
        Vector3 desiredPosition,
        const std::vector<Obstacle>& obstacles)
    {
        Vector3 targetToCamera =
            Subtract(desiredPosition, target);

        float desiredDistance =
            Length3D(targetToCamera);

        if (desiredDistance <= 0.0001f)
            return desiredPosition;

        Vector3 rayDirection =
            Scale(
                targetToCamera,
                1.0f / desiredDistance
            );

        Ray cameraRay{};
        cameraRay.position = target;
        cameraRay.direction = rayDirection;

        float nearestHitDistance =
            desiredDistance;

        bool hitSomething = false;

        auto TestCameraBox =
            [&](const BoundingBox& box)
            {
                RayCollision hit =
                    GetRayCollisionBox(
                        cameraRay,
                        box
                    );

                if (
                    hit.hit &&
                    hit.distance >= 0.0f &&
                    hit.distance < nearestHitDistance
                )
                {
                    nearestHitDistance =
                        hit.distance;

                    hitSomething = true;
                }
            };

        // Interior cover / structures.
        for (const Obstacle& obstacle : obstacles)
        {
            TestCameraBox(
                obstacle.box
            );
        }

        // Outer arena walls. These match the four cubes drawn
        // in DrawArena(). They are camera blockers even though
        // they are not part of the tank movement obstacle list.
        const BoundingBox northWall = {
            {-20.5f, 0.0f, -20.5f},
            { 20.5f, 4.5f, -19.5f}
        };

        const BoundingBox southWall = {
            {-20.5f, 0.0f, 19.5f},
            { 20.5f, 4.5f, 20.5f}
        };

        const BoundingBox westWall = {
            {-20.5f, 0.0f, -19.5f},
            {-19.5f, 4.5f,  19.5f}
        };

        const BoundingBox eastWall = {
            {19.5f, 0.0f, -19.5f},
            {20.5f, 4.5f,  19.5f}
        };

        TestCameraBox(northWall);
        TestCameraBox(southWall);
        TestCameraBox(westWall);
        TestCameraBox(eastWall);

        if (!hitSomething)
            return desiredPosition;

        // Keep a small gap between the camera and the wall so
        // the near clipping plane does not enter the geometry.
        constexpr float CAMERA_WALL_PADDING = 0.30f;

        float safeDistance =
            nearestHitDistance -
            CAMERA_WALL_PADDING;

        // If the player is extremely close to a wall, never push
        // the camera through that wall just to satisfy a minimum
        // third-person distance. Pull it very close instead.
        if (safeDistance < 0.10f)
        {
            safeDistance =
                std::max(
                    0.02f,
                    nearestHitDistance * 0.5f
                );
        }

        return Add(
            target,
            Scale(
                rayDirection,
                safeDistance
            )
        );
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
        Ray ray = GetMouseRay(GetArenaMousePosition(), camera);

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

    void ConfigureCombatants(
        std::vector<Combatant>& combatants,
        const MatchSettings& settings,
        const std::string& username)
    {
        combatants.clear();

        const Vector3 spawns[6] = {
            {-14.0f, 0.0f,  14.0f},
            {-14.0f, 0.0f, -14.0f},
            {  0.0f, 0.0f,  15.0f},
            { 14.0f, 0.0f, -14.0f},
            { 14.0f, 0.0f,  14.0f},
            {  0.0f, 0.0f, -15.0f}
        };

        const char* botNames[5] = {
            "BOT ALPHA",
            "BOT BRAVO",
            "BOT CHARLIE",
            "BOT DELTA",
            "BOT ECHO"
        };

        // Every player receives one unique color. The local player gets the
        // color chosen on the setup screen; bots fill from the remaining
        // palette slots. In the networked version the server will own these
        // reservations and reject duplicate color selections.
        bool colorUsed[ARENA_COLOR_COUNT] = {};

        for (int i = 0; i < settings.playerCount; i++)
        {
            Combatant c;

            c.name =
                (i == 0)
                ? username
                : botNames[i - 1];

            c.position = spawns[i];
            c.spawn = spawns[i];
            c.human = i == 0;

            if (IsTeamMode(settings))
            {
                int teamSize =
                    settings.playerCount / 2;

                c.team =
                    (i < teamSize)
                    ? 0
                    : 1;
            }
            else
            {
                c.team = -1;
            }

            if (IsTeamMode(settings))
            {
                // Team games intentionally override individual color choices.
                // Every Red Team tank is red and every Blue Team tank is blue.
                c.colorIndex = -1;
                c.color =
                    (c.team == 0)
                    ? TEAM_RED
                    : TEAM_BLUE;
            }
            else
            {
                // Duel and FFA keep the unique-color system.
                if (i == 0)
                {
                    c.colorIndex = settings.selectedColorIndex;
                }
                else
                {
                    c.colorIndex = -1;

                    // Walk the palette starting just after the local player's
                    // choice so bot colors stay deterministic but unique.
                    for (
                        int offset = 1;
                        offset <= ARENA_COLOR_COUNT;
                        offset++
                    )
                    {
                        int candidate =
                            (
                                settings.selectedColorIndex +
                                offset
                            ) %
                            ARENA_COLOR_COUNT;

                        if (!colorUsed[candidate])
                        {
                            c.colorIndex = candidate;
                            break;
                        }
                    }

                    if (c.colorIndex < 0)
                        c.colorIndex = 0;
                }

                colorUsed[c.colorIndex] = true;
                c.color = ARENA_COLORS[c.colorIndex];
            }

            combatants.push_back(c);
        }
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
        std::vector<Combatant>& combatants,
        int damage = SHOT_DAMAGE)
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

        int appliedDamage =
            std::min(
                damage,
                target.health
            );

        target.health -= appliedDamage;
        target.damageTaken += appliedDamage;

        if (
            attacker >= 0 &&
            attacker < (int)combatants.size()
        )
        {
            combatants[attacker].damageDealt +=
                appliedDamage;
        }

        if (target.health > 0)
            return;

        target.health = 0;
        target.alive = false;
        target.respawnTimer = RESPAWN_TIME;
        target.deaths++;

        if (attacker != victim)
            combatants[attacker].kills++;
    }

    void PlaceLocalMine(
        int owner,
        const std::vector<Combatant>& combatants,
        std::vector<Mine>& mines)
    {
        if (
            owner < 0 ||
            owner >= (int)combatants.size() ||
            !combatants[owner].alive
        )
        {
            return;
        }

        int activeOwned = 0;
        int oldestOwned = -1;

        for (int i = 0; i < (int)mines.size(); i++)
        {
            if (mines[i].active && mines[i].owner == owner)
            {
                activeOwned++;

                if (oldestOwned < 0)
                    oldestOwned = i;
            }
        }

        if (
            activeOwned >= MAX_LOCAL_MINES_PER_PLAYER &&
            oldestOwned >= 0
        )
        {
            mines.erase(mines.begin() + oldestOwned);
        }

        const Combatant& placer = combatants[owner];

        Mine mine;
        mine.position = {
            placer.position.x,
            0.0f,
            placer.position.z
        };
        mine.owner = owner;
        mine.colorIndex = placer.colorIndex;
        mine.team = placer.team;
        mine.color = placer.color;
        mine.active = true;

        mines.push_back(mine);
    }

    void UpdateLocalMines(
        std::vector<Mine>& mines,
        std::vector<Combatant>& combatants,
        const MatchSettings& settings)
    {
        for (Mine& mine : mines)
        {
            if (!mine.active)
                continue;

            for (int i = 0; i < (int)combatants.size(); i++)
            {
                if (
                    i == mine.owner ||
                    !combatants[i].alive
                )
                {
                    continue;
                }

                if (
                    mine.owner >= 0 &&
                    mine.owner < (int)combatants.size() &&
                    !AreEnemies(
                        combatants[mine.owner],
                        combatants[i],
                        settings
                    )
                )
                {
                    continue;
                }

                float triggerDistance =
                    TANK_RADIUS +
                    MINE_TRIGGER_RADIUS;

                if (
                    DistanceXZ(
                        mine.position,
                        combatants[i].position
                    ) <= triggerDistance
                )
                {
                    DamageCombatant(
                        mine.owner,
                        i,
                        combatants,
                        MINE_DAMAGE
                    );

                    mine.active = false;
                    break;
                }
            }
        }

        mines.erase(
            std::remove_if(
                mines.begin(),
                mines.end(),
                [](const Mine& mine)
                {
                    return !mine.active;
                }
            ),
            mines.end()
        );
    }

    int FindNearestTarget(
        int seeker,
        const std::vector<Combatant>& combatants,
        const MatchSettings& settings)
    {
        int best = -1;
        float bestDistance = 1000000.0f;

        for (int i = 0; i < (int)combatants.size(); i++)
        {
            if (i == seeker || !combatants[i].alive)
                continue;

            if (
                !AreEnemies(
                    combatants[seeker],
                    combatants[i],
                    settings
                )
            )
            {
                continue;
            }

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
        const MatchSettings& settings,
        float dt)
    {
        for (int i = 0; i < (int)combatants.size(); i++)
        {
            Combatant& bot = combatants[i];
            if (bot.human || !bot.alive)
                continue;

            int targetIndex = FindNearestTarget(i, combatants, settings);
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

    MatchResult EvaluateMatch(
        const std::vector<Combatant>& combatants,
        const MatchSettings& settings,
        float timeRemaining)
    {
        MatchResult result;

        if (IsTimedMode(settings))
        {
            if (timeRemaining > 0.0f)
                return result;

            int bestKills = -1;
            int bestPlayer = -1;
            bool tie = false;

            for (int i = 0; i < (int)combatants.size(); i++)
            {
                if (combatants[i].kills > bestKills)
                {
                    bestKills = combatants[i].kills;
                    bestPlayer = i;
                    tie = false;
                }
                else if (combatants[i].kills == bestKills)
                {
                    tie = true;
                }
            }

            result.finished = true;

            if (tie)
            {
                result.draw = true;
            }
            else
            {
                result.winnerPlayer = bestPlayer;
            }

            return result;
        }

        if (IsTeamMode(settings))
        {
            for (int team = 0; team < 2; team++)
            {
                if (TeamScore(combatants, team) >= settings.scoreLimit)
                {
                    result.finished = true;
                    result.winnerTeam = team;
                    return result;
                }
            }

            return result;
        }

        for (int i = 0; i < (int)combatants.size(); i++)
        {
            if (combatants[i].kills >= settings.scoreLimit)
            {
                result.finished = true;
                result.winnerPlayer = i;
                return result;
            }
        }

        return result;
    }


    void ResetMatch(
        std::vector<Combatant>& combatants,
        const MatchSettings& settings)
    {
        for (Combatant& combatant : combatants)
        {
            combatant.kills = 0;
            combatant.deaths = 0;
            combatant.damageDealt = 0;
            combatant.damageTaken = 0;
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

    bool HasNameplateLineOfSight(
        const Camera3D& camera,
        Vector3 labelWorldPosition,
        const std::vector<Obstacle>& obstacles)
    {
        Vector3 toLabel =
            Subtract(
                labelWorldPosition,
                camera.position
            );

        float distance =
            Length3D(toLabel);

        if (distance <= 0.001f)
            return true;

        Ray ray{};
        ray.position = camera.position;
        ray.direction =
            Scale(
                toLabel,
                1.0f / distance
            );

        for (const Obstacle& obstacle : obstacles)
        {
            RayCollision hit =
                GetRayCollisionBox(
                    ray,
                    obstacle.box
                );

            if (
                hit.hit &&
                hit.distance > 0.0f &&
                hit.distance < distance - 0.25f
            )
            {
                return false;
            }
        }

        return true;
    }


    void DrawNameplates(
        const std::vector<Combatant>& combatants,
        const Camera3D& camera,
        const std::vector<Obstacle>& obstacles,
        const MatchSettings& settings)
    {
        Vector3 cameraForward =
            Subtract(
                camera.target,
                camera.position
            );

        for (const Combatant& c : combatants)
        {
            if (!c.alive)
                continue;

            Vector3 labelWorldPosition = {
                c.position.x,
                2.05f,
                c.position.z
            };

            Vector3 cameraToLabel =
                Subtract(
                    labelWorldPosition,
                    camera.position
                );

            float facing =
                cameraForward.x * cameraToLabel.x +
                cameraForward.y * cameraToLabel.y +
                cameraForward.z * cameraToLabel.z;

            if (facing <= 0.0f)
                continue;

            if (
                !HasNameplateLineOfSight(
                    camera,
                    labelWorldPosition,
                    obstacles
                )
            )
            {
                continue;
            }

            Vector2 screen =
                GetWorldToScreenEx(
                    labelWorldPosition,
                    camera,
                    SCREEN_WIDTH,
                    SCREEN_HEIGHT
                );

            if (
                screen.x < -120.0f ||
                screen.x > SCREEN_WIDTH + 120.0f ||
                screen.y < -40.0f ||
                screen.y > SCREEN_HEIGHT + 40.0f
            )
            {
                continue;
            }

            std::string displayName = c.name;

            if (IsTeamMode(settings))
            {
                displayName =
                    std::string(c.team == 0 ? "[R] " : "[B] ") +
                    displayName;
            }

            int fontSize = c.human ? 18 : 16;
            int textWidth =
                MeasureText(
                    displayName.c_str(),
                    fontSize
                );

            Rectangle background = {
                screen.x - textWidth / 2.0f - 7.0f,
                screen.y - 4.0f,
                (float)textWidth + 14.0f,
                (float)fontSize + 8.0f
            };

            DrawRectangleRounded(
                background,
                0.35f,
                6,
                Color{8, 9, 12, 205}
            );

            DrawRectangleLinesEx(
                background,
                c.human ? 2.0f : 1.0f,
                c.color
            );

            DrawText(
                displayName.c_str(),
                (int)(screen.x - textWidth / 2.0f),
                (int)screen.y,
                fontSize,
                c.human
                    ? JENG_YELLOW
                    : RAYWHITE
            );
        }
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

    void DrawMines(const std::vector<Mine>& mines)
    {
        for (const Mine& mine : mines)
        {
            if (!mine.active)
                continue;

            DrawCylinder(
                {mine.position.x, 0.10f, mine.position.z},
                MINE_RADIUS,
                MINE_RADIUS * 0.86f,
                0.20f,
                18,
                Color{30, 32, 38, 255}
            );

            DrawCylinder(
                {mine.position.x, 0.22f, mine.position.z},
                MINE_RADIUS * 0.58f,
                MINE_RADIUS * 0.58f,
                0.08f,
                18,
                mine.color
            );

            DrawSphereWires(
                {mine.position.x, 0.18f, mine.position.z},
                MINE_RADIUS + 0.06f,
                8,
                12,
                mine.color
            );
        }
    }

    void SpawnDeathExplosion(
        const Combatant& combatant,
        std::vector<DeathExplosion>& explosions,
        std::vector<ExplosionParticle>& particles)
    {
        DeathExplosion explosion;
        explosion.position = {
            combatant.position.x,
            0.80f,
            combatant.position.z
        };
        explosion.tankColor = combatant.color;
        explosions.push_back(explosion);

        constexpr int PARTICLE_COUNT = 30;

        for (int i = 0; i < PARTICLE_COUNT; i++)
        {
            float angle =
                (2.0f * 3.14159265358979323846f * (float)i) /
                (float)PARTICLE_COUNT;

            float speed =
                2.5f +
                0.55f * (float)(i % 5);

            float vertical =
                2.2f +
                0.45f * (float)(i % 4);

            ExplosionParticle particle;
            particle.position = explosion.position;
            particle.velocity = {
                std::cos(angle) * speed,
                vertical,
                std::sin(angle) * speed
            };

            if (i % 5 == 0)
                particle.color = combatant.color;
            else if (i % 2 == 0)
                particle.color = JENG_YELLOW;
            else
                particle.color = Color{255, 125, 45, 255};

            particle.maxLife =
                0.45f +
                0.06f * (float)(i % 6);
            particle.life = particle.maxLife;
            particle.radius =
                0.07f +
                0.02f * (float)(i % 4);

            particles.push_back(particle);
        }
    }

    void SyncDeathExplosionTracker(
        const std::vector<Combatant>& combatants,
        std::vector<std::string>& trackedNames,
        std::vector<int>& trackedDeaths)
    {
        trackedNames.clear();
        trackedDeaths.clear();

        for (const Combatant& combatant : combatants)
        {
            trackedNames.push_back(combatant.name);
            trackedDeaths.push_back(combatant.deaths);
        }
    }

    void DetectDeathExplosions(
        const std::vector<Combatant>& combatants,
        std::vector<std::string>& trackedNames,
        std::vector<int>& trackedDeaths,
        std::vector<DeathExplosion>& explosions,
        std::vector<ExplosionParticle>& particles)
    {
        bool layoutChanged =
            trackedNames.size() != combatants.size() ||
            trackedDeaths.size() != combatants.size();

        if (!layoutChanged)
        {
            for (int i = 0; i < (int)combatants.size(); i++)
            {
                if (trackedNames[i] != combatants[i].name)
                {
                    layoutChanged = true;
                    break;
                }
            }
        }

        if (layoutChanged)
        {
            SyncDeathExplosionTracker(
                combatants,
                trackedNames,
                trackedDeaths
            );
            return;
        }

        for (int i = 0; i < (int)combatants.size(); i++)
        {
            if (combatants[i].deaths > trackedDeaths[i])
            {
                SpawnDeathExplosion(
                    combatants[i],
                    explosions,
                    particles
                );
            }

            trackedDeaths[i] = combatants[i].deaths;
        }
    }

    void UpdateDeathExplosions(
        std::vector<DeathExplosion>& explosions,
        std::vector<ExplosionParticle>& particles,
        float dt)
    {
        for (DeathExplosion& explosion : explosions)
            explosion.age += dt;

        explosions.erase(
            std::remove_if(
                explosions.begin(),
                explosions.end(),
                [](const DeathExplosion& explosion)
                {
                    return explosion.age >= explosion.duration;
                }
            ),
            explosions.end()
        );

        for (ExplosionParticle& particle : particles)
        {
            particle.life -= dt;

            particle.position =
                Add(
                    particle.position,
                    Scale(
                        particle.velocity,
                        dt
                    )
                );

            particle.velocity.y -=
                5.5f * dt;
        }

        particles.erase(
            std::remove_if(
                particles.begin(),
                particles.end(),
                [](const ExplosionParticle& particle)
                {
                    return particle.life <= 0.0f;
                }
            ),
            particles.end()
        );
    }

    void DrawDeathExplosions(
        const std::vector<DeathExplosion>& explosions,
        const std::vector<ExplosionParticle>& particles)
    {
        for (const DeathExplosion& explosion : explosions)
        {
            float t =
                std::min(
                    1.0f,
                    explosion.age /
                    explosion.duration
                );

            if (t < 0.42f)
            {
                float burstT = t / 0.42f;
                float radius =
                    0.35f +
                    burstT * 1.45f;

                DrawSphere(
                    explosion.position,
                    radius,
                    Color{255, 105, 35, 255}
                );

                DrawSphere(
                    explosion.position,
                    radius * 0.58f,
                    JENG_YELLOW
                );

                DrawSphereWires(
                    explosion.position,
                    radius * 1.12f,
                    10,
                    16,
                    explosion.tankColor
                );
            }
            else
            {
                float smokeT =
                    (t - 0.42f) /
                    0.58f;

                Vector3 smokePosition = {
                    explosion.position.x,
                    explosion.position.y +
                        0.65f +
                        smokeT * 1.35f,
                    explosion.position.z
                };

                DrawSphere(
                    smokePosition,
                    0.70f + smokeT * 0.45f,
                    Color{65, 65, 72, 255}
                );

                DrawSphereWires(
                    smokePosition,
                    0.78f + smokeT * 0.48f,
                    8,
                    12,
                    Color{105, 105, 115, 255}
                );
            }
        }

        for (const ExplosionParticle& particle : particles)
        {
            if (particle.life <= 0.0f)
                continue;

            float lifeRatio =
                particle.maxLife > 0.0f
                ? particle.life / particle.maxLife
                : 0.0f;

            DrawSphere(
                particle.position,
                particle.radius *
                    std::max(0.25f, lifeRatio),
                particle.color
            );
        }
    }

    bool MenuButton(
        Rectangle rect,
        const char* label,
        bool selected = false)
    {
        Vector2 mouse = GetArenaMousePosition();
        bool hover = CheckCollisionPointRec(mouse, rect);

        Color fill =
            selected
            ? Color{70, 30, 32, 255}
            : (
                hover
                ? Color{40, 42, 50, 255}
                : Color{24, 25, 31, 255}
            );

        Color border =
            selected
            ? JENG_RED
            : (
                hover
                ? JENG_YELLOW
                : Color{80, 83, 94, 255}
            );

        DrawRectangleRounded(
            rect,
            0.15f,
            8,
            fill
        );

        DrawRectangleLinesEx(
            rect,
            selected ? 2.0f : 1.0f,
            border
        );

        int fontSize = 18;
        int width =
            MeasureText(
                label,
                fontSize
            );

        DrawText(
            label,
            (int)(rect.x + rect.width / 2.0f - width / 2.0f),
            (int)(rect.y + rect.height / 2.0f - fontSize / 2.0f),
            fontSize,
            selected
                ? JENG_YELLOW
                : RAYWHITE
        );

        return
            hover &&
            IsMouseButtonPressed(
                MOUSE_BUTTON_LEFT
            );
    }


    bool ColorSwatchButton(
        Rectangle rect,
        Color color,
        bool selected)
    {
        Vector2 mouse = GetArenaMousePosition();
        bool hover = CheckCollisionPointRec(mouse, rect);

        DrawRectangleRounded(
            rect,
            0.22f,
            6,
            color
        );

        DrawRectangleLinesEx(
            Rectangle{
                rect.x - (selected ? 3.0f : 1.0f),
                rect.y - (selected ? 3.0f : 1.0f),
                rect.width + (selected ? 6.0f : 2.0f),
                rect.height + (selected ? 6.0f : 2.0f)
            },
            selected ? 3.0f : 1.0f,
            selected
                ? JENG_YELLOW
                : (
                    hover
                    ? RAYWHITE
                    : Color{80, 83, 94, 255}
                )
        );

        return
            hover &&
            IsMouseButtonPressed(
                MOUSE_BUTTON_LEFT
            );
    }


    bool DrawSetupScreen(
        MatchSettings& settings,
        const std::string& username,
        AppState& app)
    {
        NormalizeSettings(settings);

        DrawText(
            "JENG ARENA",
            62,
            48,
            42,
            JENG_YELLOW
        );

        DrawText(
            "MATCH SETUP",
            65,
            96,
            22,
            RAYWHITE
        );

        DrawText(
            TextFormat(
                "SIGNED IN AS  %s",
                username.c_str()
            ),
            65,
            131,
            17,
            Color{170, 174, 188, 255}
        );

        DrawText(
            "GAME MODE",
            65,
            186,
            18,
            JENG_RED
        );

        const ArenaMode modes[5] = {
            ArenaMode::SCORE_FFA,
            ArenaMode::TIME_FFA,
            ArenaMode::DUEL,
            ArenaMode::TEAM_2V2,
            ArenaMode::TEAM_3V3
        };

        for (int i = 0; i < 5; i++)
        {
            Rectangle card = {
                65.0f,
                220.0f + i * 62.0f,
                310.0f,
                48.0f
            };

            if (
                MenuButton(
                    card,
                    ArenaModeName(modes[i]),
                    settings.mode == modes[i]
                )
            )
            {
                settings.mode = modes[i];
                NormalizeSettings(settings);
            }
        }

        Rectangle settingsPanel = {
            435.0f,
            185.0f,
            775.0f,
            430.0f
        };

        DrawRectangleRounded(
            settingsPanel,
            0.03f,
            8,
            Color{18, 19, 24, 245}
        );

        DrawRectangleLinesEx(
            settingsPanel,
            1.5f,
            Color{70, 72, 82, 255}
        );

        DrawText(
            ArenaModeName(settings.mode),
            475,
            220,
            30,
            JENG_YELLOW
        );

        const char* description = "";

        switch (settings.mode)
        {
            case ArenaMode::SCORE_FFA:
                description =
                    "Every player for themselves. First player to the score target wins.";
                break;

            case ArenaMode::TIME_FFA:
                description =
                    "Every player for themselves. Highest kill count when time expires wins.";
                break;

            case ArenaMode::DUEL:
                description =
                    "One versus one. First player to the selected score wins.";
                break;

            case ArenaMode::TEAM_2V2:
                description =
                    "Two teams of two. Friendly fire is disabled. First team to target score wins.";
                break;

            case ArenaMode::TEAM_3V3:
                description =
                    "Two teams of three. Friendly fire is disabled. First team to target score wins.";
                break;
        }

        DrawText(
            description,
            475,
            269,
            16,
            Color{180, 183, 195, 255}
        );

        int settingsY = 330;

        if (
            settings.mode == ArenaMode::SCORE_FFA ||
            settings.mode == ArenaMode::TIME_FFA
        )
        {
            DrawText(
                "PLAYERS",
                475,
                settingsY,
                16,
                Color{160, 164, 178, 255}
            );

            Rectangle minusPlayers = {
                475.0f,
                (float)settingsY + 32.0f,
                44.0f,
                40.0f
            };

            Rectangle plusPlayers = {
                615.0f,
                (float)settingsY + 32.0f,
                44.0f,
                40.0f
            };

            if (MenuButton(minusPlayers, "-"))
                settings.playerCount--;

            if (MenuButton(plusPlayers, "+"))
                settings.playerCount++;

            NormalizeSettings(settings);

            DrawText(
                TextFormat("%d", settings.playerCount),
                555,
                settingsY + 41,
                24,
                RAYWHITE
            );
        }
        else
        {
            DrawText(
                TextFormat(
                    "PLAYERS   %d",
                    settings.playerCount
                ),
                475,
                settingsY + 12,
                18,
                RAYWHITE
            );
        }

        if (IsTimedMode(settings))
        {
            DrawText(
                "TIME LIMIT",
                760,
                settingsY,
                16,
                Color{160, 164, 178, 255}
            );

            Rectangle minusTime = {
                760.0f,
                (float)settingsY + 32.0f,
                44.0f,
                40.0f
            };

            Rectangle plusTime = {
                930.0f,
                (float)settingsY + 32.0f,
                44.0f,
                40.0f
            };

            if (MenuButton(minusTime, "-"))
                settings.timeLimitSeconds -= 60.0f;

            if (MenuButton(plusTime, "+"))
                settings.timeLimitSeconds += 60.0f;

            NormalizeSettings(settings);

            std::string timeText =
                FormatMatchTime(
                    settings.timeLimitSeconds
                );

            DrawText(
                timeText.c_str(),
                835,
                settingsY + 41,
                24,
                RAYWHITE
            );
        }
        else
        {
            DrawText(
                "SCORE TO WIN",
                760,
                settingsY,
                16,
                Color{160, 164, 178, 255}
            );

            Rectangle minusScore = {
                760.0f,
                (float)settingsY + 32.0f,
                44.0f,
                40.0f
            };

            Rectangle plusScore = {
                930.0f,
                (float)settingsY + 32.0f,
                44.0f,
                40.0f
            };

            if (MenuButton(minusScore, "-"))
                settings.scoreLimit--;

            if (MenuButton(plusScore, "+"))
                settings.scoreLimit++;

            NormalizeSettings(settings);

            DrawText(
                TextFormat("%d", settings.scoreLimit),
                850,
                settingsY + 41,
                24,
                RAYWHITE
            );
        }

        if (!IsTeamMode(settings))
        {
            DrawText(
                "TANK COLOR",
                475,
                421,
                16,
                Color{160, 164, 178, 255}
            );

            // Duel and FFA: 16 unique player-color choices.
            for (int i = 0; i < ARENA_COLOR_COUNT; i++)
            {
                int row = i / 8;
                int col = i % 8;

                Rectangle swatch = {
                    475.0f + col * 38.0f,
                    451.0f + row * 38.0f,
                    28.0f,
                    28.0f
                };

                if (
                    ColorSwatchButton(
                        swatch,
                        ARENA_COLORS[i],
                        settings.selectedColorIndex == i
                    )
                )
                {
                    settings.selectedColorIndex = i;
                }
            }

            DrawText(
                "SELECTED",
                805,
                421,
                14,
                Color{160, 164, 178, 255}
            );

            DrawText(
                ARENA_COLOR_NAMES[
                    settings.selectedColorIndex
                ],
                805,
                447,
                18,
                ARENA_COLORS[
                    settings.selectedColorIndex
                ]
            );

            DrawText(
                "Each player must use a unique color.",
                805,
                475,
                14,
                Color{150, 154, 168, 255}
            );

            DrawText(
                settings.mode == ArenaMode::DUEL
                    ? "1v1 // UNIQUE COLORS"
                    : "FFA // UNIQUE COLORS",
                805,
                499,
                14,
                RAYWHITE
            );
        }
        else
        {
            DrawText(
                "TEAM COLORS",
                475,
                421,
                16,
                Color{160, 164, 178, 255}
            );

            Rectangle redPreview = {
                475.0f,
                451.0f,
                145.0f,
                52.0f
            };

            Rectangle bluePreview = {
                640.0f,
                451.0f,
                145.0f,
                52.0f
            };

            DrawRectangleRounded(
                redPreview,
                0.18f,
                6,
                TEAM_RED
            );

            DrawRectangleRounded(
                bluePreview,
                0.18f,
                6,
                TEAM_BLUE
            );

            DrawRectangleLinesEx(
                redPreview,
                2.0f,
                RAYWHITE
            );

            DrawRectangleLinesEx(
                bluePreview,
                2.0f,
                RAYWHITE
            );

            DrawText(
                "RED TEAM",
                (int)redPreview.x + 22,
                (int)redPreview.y + 16,
                20,
                RAYWHITE
            );

            DrawText(
                "BLUE TEAM",
                (int)bluePreview.x + 15,
                (int)bluePreview.y + 16,
                20,
                RAYWHITE
            );

            DrawText(
                "Team games use fixed colors.",
                805,
                447,
                16,
                RAYWHITE
            );

            DrawText(
                "Red Team = red tanks",
                805,
                475,
                14,
                TEAM_RED
            );

            DrawText(
                "Blue Team = blue tanks",
                805,
                499,
                14,
                TEAM_BLUE
            );
        }

        Rectangle onlineButton = {
            475.0f,
            548.0f,
            500.0f,
            48.0f
        };

        Rectangle localButton = {
            990.0f,
            548.0f,
            180.0f,
            48.0f
        };

        if (
            MenuButton(
                onlineButton,
                "CREATE ONLINE LOBBY",
                true
            )
        )
        {
            std::string packet =
                std::string("ARENA_CREATE|") +
                ArenaModePacketName(settings.mode) +
                "|" +
                std::to_string(settings.playerCount) +
                "|" +
                std::to_string(settings.scoreLimit) +
                "|" +
                std::to_string((int)settings.timeLimitSeconds) +
                "|" +
                std::to_string(settings.selectedColorIndex);

            if (NetSendLine(packet))
            {
                app.arena.status =
                    "Creating Arena lobby...";
            }
            else
            {
                app.arena.status =
                    NetLastError();
            }
        }

        bool startLocal =
            MenuButton(
                localButton,
                "LOCAL TEST"
            );

        if (!app.arena.status.empty())
        {
            DrawText(
                app.arena.status.c_str(),
                475,
                612,
                15,
                Color{170, 174, 188, 255}
            );
        }

        DrawText(
            "Online creates a JENG CHAT lobby. Local Test keeps the bot prototype available.",
            65,
            664,
            15,
            Color{135, 139, 154, 255}
        );

        return startLocal;
    }



    bool ArenaLobbyIsTeamMode(const std::string& mode)
    {
        return
            mode == "TEAM_2V2" ||
            mode == "TEAM_3V3";
    }


    const char* ArenaLobbyModeLabel(const std::string& mode)
    {
        if (mode == "SCORE_FFA") return "SCORE FFA";
        if (mode == "TIME_FFA") return "TIME FFA";
        if (mode == "DUEL") return "DUEL";
        if (mode == "TEAM_2V2") return "TEAMS 2v2";
        if (mode == "TEAM_3V3") return "TEAMS 3v3";
        return "JENG ARENA";
    }


    int FindArenaLobbyPlayer(
        const ArenaClientState& arena,
        const std::string& username)
    {
        for (int i = 0; i < (int)arena.players.size(); i++)
        {
            if (arena.players[i].name == username)
                return i;
        }

        return -1;
    }


    bool ArenaLobbyColorUsedByOther(
        const ArenaClientState& arena,
        int colorIndex,
        const std::string& username)
    {
        for (const ArenaLobbyPlayerClientState& player : arena.players)
        {
            if (
                player.name != username &&
                player.colorIndex == colorIndex
            )
            {
                return true;
            }
        }

        return false;
    }


    void DrawArenaLobbyScreen(AppState& app)
    {
        ArenaClientState& lobby = app.arena;

        DrawText(
            "JENG ARENA",
            58,
            42,
            40,
            JENG_YELLOW
        );

        DrawText(
            "ONLINE LOBBY",
            61,
            89,
            22,
            RAYWHITE
        );

        DrawText(
            ArenaLobbyModeLabel(lobby.mode),
            61,
            130,
            25,
            JENG_RED
        );

        std::string ruleText;

        if (lobby.mode == "TIME_FFA")
        {
            ruleText =
                "TIME  " +
                FormatMatchTime(
                    (float)lobby.timeLimitSeconds
                );
        }
        else
        {
            ruleText =
                "SCORE TO WIN  " +
                std::to_string(
                    lobby.scoreLimit
                );
        }

        DrawText(
            ruleText.c_str(),
            61,
            165,
            16,
            Color{170, 174, 188, 255}
        );

        DrawText(
            TextFormat(
                "HOST  %s",
                lobby.hostName.c_str()
            ),
            61,
            190,
            16,
            Color{170, 174, 188, 255}
        );

        Rectangle playerPanel = {
            55.0f,
            230.0f,
            650.0f,
            315.0f
        };

        DrawRectangleRounded(
            playerPanel,
            0.025f,
            8,
            Color{18, 19, 24, 245}
        );

        DrawRectangleLinesEx(
            playerPanel,
            1.5f,
            Color{70, 72, 82, 255}
        );

        DrawText(
            TextFormat(
                "PLAYERS  %d / %d",
                (int)lobby.players.size(),
                lobby.maxPlayers
            ),
            80,
            251,
            18,
            JENG_YELLOW
        );

        bool teamMode =
            ArenaLobbyIsTeamMode(
                lobby.mode
            );

        for (int i = 0; i < (int)lobby.players.size(); i++)
        {
            const ArenaLobbyPlayerClientState& player =
                lobby.players[i];

            float y =
                292.0f +
                i * 38.0f;

            Color rowColor =
                player.name == app.username
                ? JENG_YELLOW
                : RAYWHITE;

            DrawText(
                player.name.c_str(),
                82,
                (int)y,
                18,
                rowColor
            );

            if (teamMode)
            {
                const char* teamText =
                    player.team == 0
                    ? "RED TEAM"
                    : "BLUE TEAM";

                Color teamColor =
                    player.team == 0
                    ? TEAM_RED
                    : TEAM_BLUE;

                DrawText(
                    teamText,
                    360,
                    (int)y,
                    17,
                    teamColor
                );
            }
            else if (
                player.colorIndex >= 0 &&
                player.colorIndex < ARENA_COLOR_COUNT
            )
            {
                DrawRectangle(
                    365,
                    (int)y + 1,
                    18,
                    18,
                    ARENA_COLORS[
                        player.colorIndex
                    ]
                );

                DrawText(
                    ARENA_COLOR_NAMES[
                        player.colorIndex
                    ],
                    394,
                    (int)y,
                    15,
                    Color{170, 174, 188, 255}
                );
            }

            DrawText(
                player.ready
                    ? "READY"
                    : "NOT READY",
                555,
                (int)y,
                16,
                player.ready
                    ? Color{90, 220, 130, 255}
                    : Color{220, 105, 105, 255}
            );
        }

        Rectangle controlPanel = {
            735.0f,
            230.0f,
            490.0f,
            315.0f
        };

        DrawRectangleRounded(
            controlPanel,
            0.025f,
            8,
            Color{18, 19, 24, 245}
        );

        DrawRectangleLinesEx(
            controlPanel,
            1.5f,
            Color{70, 72, 82, 255}
        );

        bool isHost =
            lobby.hostName == app.username;

        int localIndex =
            FindArenaLobbyPlayer(
                lobby,
                app.username
            );

        bool localReady =
            localIndex >= 0 &&
            lobby.players[localIndex].ready;

        static std::string inviteInput;
        static bool inviteInputActive = false;

        if (isHost && lobby.phase == "LOBBY")
        {
            DrawText(
                "INVITE PLAYER",
                765,
                253,
                16,
                JENG_RED
            );

            Rectangle inviteBox = {
                765.0f,
                285.0f,
                275.0f,
                43.0f
            };

            Rectangle inviteButton = {
                1055.0f,
                285.0f,
                140.0f,
                43.0f
            };

            Vector2 mouse =
                GetArenaMousePosition();

            if (
                IsMouseButtonPressed(
                    MOUSE_BUTTON_LEFT
                )
            )
            {
                inviteInputActive =
                    CheckCollisionPointRec(
                        mouse,
                        inviteBox
                    );
            }

            DrawRectangleRounded(
                inviteBox,
                0.12f,
                8,
                Color{31, 33, 40, 255}
            );

            DrawRectangleRoundedLinesEx(
                inviteBox,
                0.12f,
                8,
                inviteInputActive
                    ? 2.0f
                    : 1.0f,
                inviteInputActive
                    ? JENG_YELLOW
                    : Color{80, 83, 94, 255}
            );

            if (inviteInput.empty())
            {
                DrawText(
                    "username...",
                    780,
                    297,
                    17,
                    Color{125, 129, 143, 255}
                );
            }
            else
            {
                DrawText(
                    inviteInput.c_str(),
                    780,
                    297,
                    17,
                    RAYWHITE
                );
            }

            if (inviteInputActive)
            {
                int key =
                    GetCharPressed();

                while (key > 0)
                {
                    if (
                        key >= 32 &&
                        key <= 125 &&
                        inviteInput.size() < 16
                    )
                    {
                        char c =
                            (char)key;

                        if (
                            std::isalnum(
                                (unsigned char)c
                            ) ||
                            c == '_' ||
                            c == '-'
                        )
                        {
                            inviteInput += c;
                        }
                    }

                    key =
                        GetCharPressed();
                }

                if (
                    IsKeyPressed(
                        KEY_BACKSPACE
                    ) &&
                    !inviteInput.empty()
                )
                {
                    inviteInput.pop_back();
                }
            }

            if (
                MenuButton(
                    inviteButton,
                    "INVITE"
                ) &&
                !inviteInput.empty()
            )
            {
                if (
                    NetSendLine(
                        "ARENA_INVITE|" +
                        inviteInput
                    )
                )
                {
                    lobby.status =
                        "Invitation sent to " +
                        inviteInput +
                        ".";
                    inviteInput.clear();
                }
                else
                {
                    lobby.status =
                        NetLastError();
                }
            }
        }
        else
        {
            DrawText(
                isHost
                    ? "LOBBY LOCKED"
                    : "WAITING FOR HOST",
                765,
                253,
                16,
                JENG_RED
            );
        }

        if (!teamMode && lobby.phase == "LOBBY")
        {
            DrawText(
                "YOUR TANK COLOR",
                765,
                357,
                15,
                Color{160, 164, 178, 255}
            );

            int selectedColor =
                localIndex >= 0
                ? lobby.players[localIndex].colorIndex
                : -1;

            for (int i = 0; i < ARENA_COLOR_COUNT; i++)
            {
                int row = i / 8;
                int col = i % 8;

                Rectangle swatch = {
                    765.0f + col * 49.0f,
                    386.0f + row * 42.0f,
                    30.0f,
                    30.0f
                };

                bool taken =
                    ArenaLobbyColorUsedByOther(
                        lobby,
                        i,
                        app.username
                    );

                if (
                    !taken &&
                    ColorSwatchButton(
                        swatch,
                        ARENA_COLORS[i],
                        selectedColor == i
                    )
                )
                {
                    NetSendLine(
                        "ARENA_COLOR|" +
                        std::to_string(i)
                    );
                }

                if (taken)
                {
                    DrawLine(
                        (int)swatch.x,
                        (int)swatch.y,
                        (int)(
                            swatch.x +
                            swatch.width
                        ),
                        (int)(
                            swatch.y +
                            swatch.height
                        ),
                        Color{20, 20, 24, 230}
                    );

                    DrawLine(
                        (int)(
                            swatch.x +
                            swatch.width
                        ),
                        (int)swatch.y,
                        (int)swatch.x,
                        (int)(
                            swatch.y +
                            swatch.height
                        ),
                        Color{20, 20, 24, 230}
                    );
                }
            }
        }
        else if (teamMode)
        {
            DrawText(
                "TEAM COLORS",
                765,
                357,
                15,
                Color{160, 164, 178, 255}
            );

            if (localIndex >= 0)
            {
                int team =
                    lobby.players[
                        localIndex
                    ].team;

                DrawText(
                    team == 0
                        ? "YOU ARE RED TEAM"
                        : "YOU ARE BLUE TEAM",
                    765,
                    390,
                    21,
                    team == 0
                        ? TEAM_RED
                        : TEAM_BLUE
                );
            }
        }

        Rectangle readyButton = {
            55.0f,
            570.0f,
            270.0f,
            48.0f
        };

        if (
            lobby.phase == "LOBBY" &&
            MenuButton(
                readyButton,
                localReady
                    ? "UNREADY"
                    : "READY",
                localReady
            )
        )
        {
            NetSendLine(
                "ARENA_READY"
            );
        }

        if (isHost)
        {
            Rectangle startButton = {
                345.0f,
                570.0f,
                360.0f,
                48.0f
            };

            if (
                lobby.phase == "LOBBY" &&
                MenuButton(
                    startButton,
                    "START ONLINE MATCH",
                    true
                )
            )
            {
                NetSendLine(
                    "ARENA_START"
                );
            }
        }

        Rectangle leaveButton = {
            735.0f,
            570.0f,
            220.0f,
            48.0f
        };

        if (
            MenuButton(
                leaveButton,
                "LEAVE LOBBY"
            )
        )
        {
            NetSendLine(
                "ARENA_LEAVE"
            );

            lobby.active = false;
            lobby.players.clear();
            lobby.phase = "WAITING";
            lobby.status =
                "Left Arena lobby.";
        }

        DrawText(
            lobby.status.c_str(),
            55,
            642,
            16,
            lobby.phase == "STARTING"
                ? JENG_YELLOW
                : Color{160, 164, 178, 255}
        );

        if (lobby.phase == "STARTING")
        {
            DrawText(
                "LOBBY NETWORKING COMPLETE // REALTIME COMBAT SYNC IS THE NEXT PHASE",
                55,
                674,
                14,
                JENG_YELLOW
            );
        }
    }


    void DrawHud(
        const std::vector<Combatant>& combatants,
        const MatchSettings& settings,
        float timeRemaining,
        const MatchResult& result,
        int localPlayerIndex = 0)
    {
        if (combatants.empty())
            return;

        localPlayerIndex =
            std::max(
                0,
                std::min(
                    localPlayerIndex,
                    (int)combatants.size() - 1
                )
            );

        const Combatant& player =
            combatants[localPlayerIndex];

        DrawRectangle(
            18,
            18,
            430,
            136,
            HUD_BG
        );

        DrawRectangleLines(
            18,
            18,
            430,
            136,
            JENG_RED
        );

        DrawText(
            ArenaModeName(settings.mode),
            32,
            30,
            20,
            JENG_YELLOW
        );

        DrawText(
            TextFormat(
                "HEALTH  %d / %d",
                player.health,
                MAX_HEALTH
            ),
            32,
            62,
            19,
            player.health > 25
                ? RAYWHITE
                : JENG_RED
        );

        if (IsTeamMode(settings))
        {
            DrawText(
                TextFormat(
                    "RED %d    BLUE %d    TARGET %d",
                    TeamScore(combatants, 0),
                    TeamScore(combatants, 1),
                    settings.scoreLimit
                ),
                32,
                89,
                18,
                RAYWHITE
            );
        }
        else
        {
            DrawText(
                TextFormat(
                    "KILLS %d    DEATHS %d    DMG %d",
                    player.kills,
                    player.deaths,
                    player.damageDealt
                ),
                32,
                89,
                18,
                RAYWHITE
            );
        }

        if (IsTimedMode(settings))
        {
            std::string timeText =
                "TIME  " +
                FormatMatchTime(
                    timeRemaining
                );

            DrawText(
                timeText.c_str(),
                32,
                118,
                18,
                Color{180, 183, 195, 255}
            );
        }
        else
        {
            DrawText(
                TextFormat(
                    "SCORE TO WIN  %d",
                    settings.scoreLimit
                ),
                32,
                118,
                16,
                Color{165, 168, 180, 255}
            );
        }

        DrawRectangle(
            18,
            SCREEN_HEIGHT - 62,
            1244,
            44,
            HUD_BG
        );

        DrawText(
            "WASD Move   Mouse Camera   LMB Fire   RMB Mine   HOLD TAB Scoreboard   R Rematch   M Setup   F11 Fullscreen   ESC Back",
            30,
            SCREEN_HEIGHT - 48,
            17,
            RAYWHITE
        );

        if (!player.alive && !result.finished)
        {
            DrawText(
                TextFormat(
                    "RESPAWNING IN %.1f",
                    std::max(
                        0.0f,
                        player.respawnTimer
                    )
                ),
                SCREEN_WIDTH / 2 - 115,
                SCREEN_HEIGHT - 105,
                22,
                JENG_RED
            );
        }

        if (result.finished)
        {
            DrawRectangle(
                SCREEN_WIDTH / 2 - 285,
                SCREEN_HEIGHT / 2 - 95,
                570,
                190,
                Color{8, 9, 12, 240}
            );

            DrawRectangleLinesEx(
                Rectangle{
                    (float)SCREEN_WIDTH / 2.0f - 285.0f,
                    (float)SCREEN_HEIGHT / 2.0f - 95.0f,
                    570.0f,
                    190.0f
                },
                3.0f,
                JENG_RED
            );

            std::string title;

            if (result.draw)
            {
                title = "MATCH DRAW";
            }
            else if (result.winnerTeam >= 0)
            {
                title =
                    result.winnerTeam == 0
                    ? "RED TEAM WINS"
                    : "BLUE TEAM WINS";
            }
            else if (
                result.winnerPlayer >= 0 &&
                result.winnerPlayer < (int)combatants.size()
            )
            {
                title =
                    combatants[result.winnerPlayer].name +
                    " WINS";
            }
            else
            {
                title = "MATCH OVER";
            }

            int titleWidth =
                MeasureText(
                    title.c_str(),
                    32
                );

            DrawText(
                title.c_str(),
                SCREEN_WIDTH / 2 - titleWidth / 2,
                SCREEN_HEIGHT / 2 - 48,
                32,
                JENG_YELLOW
            );

            const char* options =
                "R  REMATCH        M  MATCH SETUP";

            DrawText(
                options,
                SCREEN_WIDTH / 2 -
                    MeasureText(options, 19) / 2,
                SCREEN_HEIGHT / 2 + 22,
                19,
                RAYWHITE
            );
        }

    }


    void DrawScoreboardOverlay(
        const std::vector<Combatant>& combatants,
        const MatchSettings& settings,
        float timeRemaining)
    {
        if (combatants.empty())
            return;

        DrawRectangle(
            0,
            0,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            Color{0, 0, 0, 155}
        );

        const float panelWidth = 900.0f;
        const float rowHeight = 48.0f;
        const float panelHeight =
            150.0f + rowHeight * combatants.size();

        Rectangle panel = {
            SCREEN_WIDTH / 2.0f - panelWidth / 2.0f,
            SCREEN_HEIGHT / 2.0f - panelHeight / 2.0f,
            panelWidth,
            panelHeight
        };

        DrawRectangleRounded(
            panel,
            0.025f,
            8,
            Color{12, 13, 17, 245}
        );

        DrawRectangleLinesEx(
            panel,
            2.0f,
            JENG_RED
        );

        DrawText(
            "JENG ARENA // SCOREBOARD",
            (int)panel.x + 30,
            (int)panel.y + 24,
            28,
            JENG_YELLOW
        );

        std::string matchInfo = ArenaModeName(settings.mode);

        if (IsTimedMode(settings))
        {
            matchInfo +=
                "    TIME " +
                FormatMatchTime(timeRemaining);
        }
        else
        {
            matchInfo +=
                "    TARGET " +
                std::to_string(settings.scoreLimit);
        }

        if (IsTeamMode(settings))
        {
            matchInfo +=
                "    RED " +
                std::to_string(TeamScore(combatants, 0)) +
                "  BLUE " +
                std::to_string(TeamScore(combatants, 1));
        }

        DrawText(
            matchInfo.c_str(),
            (int)panel.x + 30,
            (int)panel.y + 63,
            17,
            Color{175, 179, 193, 255}
        );

        const int headerY =
            (int)panel.y + 102;

        const int xColor =
            (int)panel.x + 32;
        const int xPlayer =
            (int)panel.x + 75;
        const int xTeam =
            (int)panel.x + 365;
        const int xKills =
            (int)panel.x + 495;
        const int xDeaths =
            (int)panel.x + 590;
        const int xDamage =
            (int)panel.x + 690;
        const int xTaken =
            (int)panel.x + 800;

        DrawText("PLAYER", xPlayer, headerY, 16, Color{150, 154, 168, 255});
        DrawText("TEAM", xTeam, headerY, 16, Color{150, 154, 168, 255});
        DrawText("K", xKills, headerY, 16, Color{150, 154, 168, 255});
        DrawText("D", xDeaths, headerY, 16, Color{150, 154, 168, 255});
        DrawText("DMG", xDamage, headerY, 16, Color{150, 154, 168, 255});
        DrawText("TAKEN", xTaken, headerY, 16, Color{150, 154, 168, 255});

        std::vector<int> order;
        for (int i = 0; i < (int)combatants.size(); i++)
            order.push_back(i);

        std::sort(
            order.begin(),
            order.end(),
            [&](int a, int b)
            {
                if (combatants[a].kills != combatants[b].kills)
                    return combatants[a].kills > combatants[b].kills;

                if (combatants[a].damageDealt != combatants[b].damageDealt)
                    return combatants[a].damageDealt > combatants[b].damageDealt;

                return combatants[a].deaths < combatants[b].deaths;
            }
        );

        for (int row = 0; row < (int)order.size(); row++)
        {
            int i = order[row];
            const Combatant& c = combatants[i];

            int y =
                headerY + 29 +
                (int)(row * rowHeight);

            if (row % 2 == 0)
            {
                DrawRectangle(
                    (int)panel.x + 18,
                    y - 8,
                    (int)panel.width - 36,
                    38,
                    Color{25, 26, 32, 210}
                );
            }

            DrawCircle(
                xColor + 11,
                y + 7,
                9.0f,
                c.color
            );

            DrawCircleLines(
                xColor + 11,
                y + 7,
                10.0f,
                RAYWHITE
            );

            DrawText(
                c.name.c_str(),
                xPlayer,
                y - 3,
                19,
                c.human ? JENG_YELLOW : RAYWHITE
            );

            const char* teamText = "--";
            Color teamColor = Color{150, 154, 168, 255};

            if (IsTeamMode(settings))
            {
                teamText = c.team == 0 ? "RED" : "BLUE";
                teamColor =
                    c.team == 0
                    ? JENG_RED
                    : Color{75, 160, 255, 255};
            }

            DrawText(teamText, xTeam, y - 3, 18, teamColor);
            DrawText(TextFormat("%d", c.kills), xKills, y - 3, 19, RAYWHITE);
            DrawText(TextFormat("%d", c.deaths), xDeaths, y - 3, 19, RAYWHITE);
            DrawText(TextFormat("%d", c.damageDealt), xDamage, y - 3, 19, RAYWHITE);
            DrawText(TextFormat("%d", c.damageTaken), xTaken, y - 3, 19, RAYWHITE);
        }

        DrawText(
            "Release TAB to close",
            (int)panel.x + 30,
            (int)(panel.y + panel.height - 31),
            15,
            Color{135, 139, 154, 255}
        );
    }

}


namespace
{
    bool arenaInitialized = false;

    std::string arenaUsername = "PLAYER";

    RenderTexture2D arenaTarget{};

    Camera3D camera{};
    float cameraYaw = 0.0f;

    constexpr float cameraDistance = 7.0f;
    constexpr float cameraHeight = 3.8f;
    constexpr float cameraTargetHeight = 0.9f;
    constexpr float cameraReturnSpeed = 6.0f;
    constexpr float cameraSensitivity = 0.0080f;

    float currentCameraDistance = cameraDistance;


    Model bodyModel{};
    Model turretModel{};

    std::vector<Obstacle> obstacles;

    MatchSettings settings;
    MatchResult result;

    std::vector<Combatant> combatants;
    std::vector<Projectile> projectiles;
    std::vector<Mine> mines;
    std::vector<DeathExplosion> deathExplosions;
    std::vector<ExplosionParticle> explosionParticles;
    std::vector<std::string> trackedDeathNames;
    std::vector<int> trackedDeathCounts;

    bool inSetup = true;
    float timeRemaining = DEFAULT_TIME_LIMIT;

    // Online match render/prediction state. The server owns the real
    // positions; the local client predicts its own tank between snapshots
    // and interpolates remote tanks toward server targets.
    bool onlineMatchInitialized = false;
    int onlineLocalPlayerIndex = -1;
    int onlineLastWorldSequence = -1;
    int onlineInputSequence = 0;
    float onlineInputSendAccumulator = 0.0f;

    std::vector<Vector3> onlineTargetPositions;
    std::vector<float> onlineTargetBodyYaws;
    std::vector<float> onlineTargetAimYaws;


    void BeginArenaMouseLook()
    {
        HideCursor();
        CenterArenaMouse();
    }


    void UpdateArenaMouseLook()
    {
        if (!IsWindowFocused())
            return;

        HideCursor();

        Vector2 mouse = GetArenaMousePosition();
        float mouseDeltaX =
            mouse.x - SCREEN_WIDTH * 0.5f;

        cameraYaw -=
            mouseDeltaX *
            cameraSensitivity;

        CenterArenaMouse();
    }


    void BuildArenaObstacles()
    {
        obstacles = {
            {
                {
                    {-3.0f, 0.0f, -3.0f},
                    { 3.0f, 4.0f,  3.0f}
                }
            },

            {
                {
                    {-14.0f, 0.0f, -2.0f},
                    {-10.0f, 3.6f,  2.0f}
                }
            },

            {
                {
                    {10.0f, 0.0f, -2.0f},
                    {14.0f, 3.6f,  2.0f}
                }
            },

            {
                {
                    {-2.0f, 0.0f, -14.0f},
                    { 2.0f, 3.6f, -10.0f}
                }
            },

            {
                {
                    {-2.0f, 0.0f, 10.0f},
                    { 2.0f, 3.6f, 14.0f}
                }
            },

            {
                {
                    {-13.0f, 0.0f, -13.0f},
                    { -9.0f, 2.5f,  -9.0f}
                }
            },

            {
                {
                    { 9.0f, 0.0f,  9.0f},
                    {13.0f, 2.5f, 13.0f}
                }
            }
        };
    }


    void ResetArenaCamera()
    {
        cameraYaw = 0.0f;
        currentCameraDistance = cameraDistance;

        camera.position = {
            0.0f,
            3.8f,
            7.0f
        };

        camera.target = {
            0.0f,
            0.9f,
            0.0f
        };

        camera.up = {
            0.0f,
            1.0f,
            0.0f
        };

        camera.fovy = 60.0f;
        camera.projection = CAMERA_PERSPECTIVE;
    }


    void ResetOnlineMatchState()
    {
        onlineMatchInitialized = false;
        onlineLocalPlayerIndex = -1;
        onlineLastWorldSequence = -1;
        onlineInputSequence = 0;
        onlineInputSendAccumulator = 0.0f;

        onlineTargetPositions.clear();
        onlineTargetBodyYaws.clear();
        onlineTargetAimYaws.clear();

        combatants.clear();
        projectiles.clear();
        mines.clear();
        deathExplosions.clear();
        explosionParticles.clear();
        trackedDeathNames.clear();
        trackedDeathCounts.clear();
    }


    Color OnlineCombatantColor(
        const ArenaWorldPlayerClientState& player,
        bool teamMode)
    {
        if (teamMode)
        {
            return
                player.team == 0
                ? TEAM_RED
                : TEAM_BLUE;
        }

        if (
            player.colorIndex >= 0 &&
            player.colorIndex < ARENA_COLOR_COUNT
        )
        {
            return ARENA_COLORS[player.colorIndex];
        }

        return WHITE;
    }


    Color OnlineMineColor(
        const ArenaMineClientState& mine,
        bool teamMode)
    {
        if (teamMode)
        {
            return
                mine.team == 0
                ? TEAM_RED
                : TEAM_BLUE;
        }

        if (
            mine.colorIndex >= 0 &&
            mine.colorIndex < ARENA_COLOR_COUNT
        )
        {
            return ARENA_COLORS[mine.colorIndex];
        }

        return WHITE;
    }


    bool BuildOnlineCombatants(
        const AppState& app)
    {
        if (app.arena.worldPlayers.empty())
            return false;

        settings = MatchSettings{};
        settings.mode =
            ArenaModeFromPacketName(
                app.arena.mode
            );
        settings.playerCount =
            (int)app.arena.worldPlayers.size();
        settings.scoreLimit =
            app.arena.scoreLimit;
        settings.timeLimitSeconds =
            (float)app.arena.timeLimitSeconds;

        NormalizeSettings(settings);

        combatants.clear();
        onlineTargetPositions.clear();
        onlineTargetBodyYaws.clear();
        onlineTargetAimYaws.clear();

        onlineLocalPlayerIndex = -1;

        bool teamMode =
            IsTeamMode(settings);

        for (
            int i = 0;
            i < (int)app.arena.worldPlayers.size();
            i++
        )
        {
            const ArenaWorldPlayerClientState& world =
                app.arena.worldPlayers[i];

            Combatant combatant;
            combatant.name = world.name;
            combatant.position = {
                world.x,
                0.0f,
                world.z
            };
            combatant.spawn = combatant.position;
            combatant.bodyYaw = world.bodyYaw;
            combatant.aimDirection =
                ArenaDirectionFromYaw(
                    world.aimYaw
                );
            combatant.health = world.health;
            combatant.alive = world.alive;
            combatant.kills = world.kills;
            combatant.deaths = world.deaths;
            combatant.damageDealt = world.damageDealt;
            combatant.damageTaken = world.damageTaken;
            combatant.respawnTimer = world.respawnTimer;
            combatant.human =
                world.name == app.username;
            combatant.team = world.team;
            combatant.colorIndex = world.colorIndex;
            combatant.color =
                OnlineCombatantColor(
                    world,
                    teamMode
                );

            if (combatant.human)
                onlineLocalPlayerIndex = i;

            combatants.push_back(combatant);
            onlineTargetPositions.push_back(
                combatant.position
            );
            onlineTargetBodyYaws.push_back(
                world.bodyYaw
            );
            onlineTargetAimYaws.push_back(
                world.aimYaw
            );
        }

        if (
            onlineLocalPlayerIndex < 0 ||
            onlineLocalPlayerIndex >=
                (int)combatants.size()
        )
        {
            return false;
        }

        result = MatchResult{};
        projectiles.clear();
        mines.clear();

        for (const ArenaMineClientState& worldMine : app.arena.worldMines)
        {
            Mine mine;
            mine.position = {worldMine.x, 0.0f, worldMine.z};
            mine.colorIndex = worldMine.colorIndex;
            mine.team = worldMine.team;
            mine.color = OnlineMineColor(worldMine, teamMode);
            mine.active = true;
            mines.push_back(mine);
        }

        timeRemaining =
            app.arena.timeRemainingSeconds > 0.0f
            ? app.arena.timeRemainingSeconds
            : settings.timeLimitSeconds;

        onlineLastWorldSequence =
            app.arena.worldSequence;
        onlineInputSequence = 0;
        onlineInputSendAccumulator = 0.0f;
        onlineMatchInitialized = true;

        deathExplosions.clear();
        explosionParticles.clear();
        SyncDeathExplosionTracker(
            combatants,
            trackedDeathNames,
            trackedDeathCounts
        );

        ResetArenaCamera();
        BeginArenaMouseLook();

        return true;
    }


    int FindOnlineCombatantByName(
        const std::string& name)
    {
        for (
            int i = 0;
            i < (int)combatants.size();
            i++
        )
        {
            if (combatants[i].name == name)
                return i;
        }

        return -1;
    }


    bool OnlineWorldLayoutChanged(
        const AppState& app)
    {
        if (
            app.arena.worldPlayers.size() !=
            combatants.size()
        )
        {
            return true;
        }

        for (
            const ArenaWorldPlayerClientState& world :
            app.arena.worldPlayers
        )
        {
            if (
                FindOnlineCombatantByName(
                    world.name
                ) < 0
            )
            {
                return true;
            }
        }

        return false;
    }


    void ApplyOnlineWorldSnapshot(
        const AppState& app)
    {
        if (!onlineMatchInitialized)
        {
            BuildOnlineCombatants(app);
            return;
        }

        if (
            app.arena.worldSequence ==
            onlineLastWorldSequence
        )
        {
            return;
        }

        if (OnlineWorldLayoutChanged(app))
        {
            BuildOnlineCombatants(app);
            return;
        }

        bool teamMode =
            IsTeamMode(settings);

        for (
            const ArenaWorldPlayerClientState& world :
            app.arena.worldPlayers
        )
        {
            int index =
                FindOnlineCombatantByName(
                    world.name
                );

            if (index < 0)
                continue;

            Vector3 serverPosition = {
                world.x,
                0.0f,
                world.z
            };

            onlineTargetPositions[index] =
                serverPosition;
            onlineTargetBodyYaws[index] =
                world.bodyYaw;
            onlineTargetAimYaws[index] =
                world.aimYaw;

            Combatant& combatant =
                combatants[index];

            combatant.team = world.team;
            combatant.colorIndex =
                world.colorIndex;
            combatant.color =
                OnlineCombatantColor(
                    world,
                    teamMode
                );
            combatant.health = world.health;
            combatant.alive = world.alive;
            combatant.kills = world.kills;
            combatant.deaths = world.deaths;
            combatant.damageDealt = world.damageDealt;
            combatant.damageTaken = world.damageTaken;
            combatant.respawnTimer = world.respawnTimer;

            // Large disagreement means prediction diverged badly or the
            // server corrected a collision. Snap instead of visibly sliding
            // through geometry for several frames.
            float error =
                DistanceXZ(
                    combatant.position,
                    serverPosition
                );

            if (
                index == onlineLocalPlayerIndex &&
                error > 2.25f
            )
            {
                combatant.position =
                    serverPosition;
            }
        }

        timeRemaining = app.arena.timeRemainingSeconds;

        projectiles.clear();
        for (
            const ArenaProjectileClientState& worldProjectile :
            app.arena.worldProjectiles
        )
        {
            Projectile projectile;
            projectile.position = {
                worldProjectile.x,
                worldProjectile.y,
                worldProjectile.z
            };
            projectile.active = true;
            projectiles.push_back(projectile);
        }

        mines.clear();
        for (
            const ArenaMineClientState& worldMine :
            app.arena.worldMines
        )
        {
            Mine mine;
            mine.position = {
                worldMine.x,
                0.0f,
                worldMine.z
            };
            mine.colorIndex = worldMine.colorIndex;
            mine.team = worldMine.team;
            mine.color = OnlineMineColor(worldMine, teamMode);
            mine.active = true;
            mines.push_back(mine);
        }

        onlineLastWorldSequence =
            app.arena.worldSequence;
    }


    void SmoothOnlineCombatants(float dt)
    {
        if (!onlineMatchInitialized)
            return;

        for (
            int i = 0;
            i < (int)combatants.size();
            i++
        )
        {
            Combatant& combatant =
                combatants[i];

            bool local =
                i == onlineLocalPlayerIndex;

            // Remote players interpolate aggressively toward the latest
            // snapshot. The local tank only receives a gentle correction
            // because its immediate movement is client-predicted.
            float positionSpeed =
                local ? 4.0f : 14.0f;

            float alpha =
                1.0f -
                std::exp(
                    -positionSpeed * dt
                );

            combatant.position.x +=
                (
                    onlineTargetPositions[i].x -
                    combatant.position.x
                ) *
                alpha;

            combatant.position.z +=
                (
                    onlineTargetPositions[i].z -
                    combatant.position.z
                ) *
                alpha;

            float rotationAlpha =
                1.0f -
                std::exp(
                    -(local ? 6.0f : 16.0f) * dt
                );

            if (!local)
            {
                combatant.bodyYaw =
                    ArenaLerpAngleDegrees(
                        combatant.bodyYaw,
                        onlineTargetBodyYaws[i],
                        rotationAlpha
                    );

                float currentAimYaw =
                    DirectionYaw(
                        combatant.aimDirection
                    );

                float aimYaw =
                    ArenaLerpAngleDegrees(
                        currentAimYaw,
                        onlineTargetAimYaws[i],
                        rotationAlpha
                    );

                combatant.aimDirection =
                    ArenaDirectionFromYaw(
                        aimYaw
                    );
            }
        }
    }


    void DrawOnlineCombatBanner(
        const AppState& app)
    {
        DrawText(
            "ONLINE SERVER-AUTHORIZED COMBAT",
            32,
            147,
            15,
            Color{90, 220, 130, 255}
        );

        DrawText(
            TextFormat(
                "WORLD SNAPSHOT  %d",
                app.arena.worldSequence
            ),
            32,
            166,
            14,
            Color{165, 168, 180, 255}
        );
    }


    void DrawOnlineLoadingScreen(
        const AppState& app)
    {
        BeginTextureMode(arenaTarget);

        ClearBackground(
            Color{11, 12, 16, 255}
        );

        DrawText(
            "JENG ARENA",
            58,
            42,
            40,
            JENG_YELLOW
        );

        DrawText(
            "STARTING ONLINE MATCH",
            61,
            96,
            24,
            JENG_RED
        );

        DrawText(
            app.arena.status.c_str(),
            61,
            145,
            18,
            RAYWHITE
        );

        DrawText(
            "Waiting for the first authoritative world snapshot...",
            61,
            184,
            16,
            Color{160, 164, 178, 255}
        );

        EndTextureMode();
    }


    void UpdateOnlineArenaMatch(
        AppState& app,
        float dt)
    {
        if (app.arena.worldPlayers.empty())
        {
            ShowCursor();
            DrawOnlineLoadingScreen(app);
            return;
        }

        if (!onlineMatchInitialized)
        {
            if (!BuildOnlineCombatants(app))
            {
                DrawOnlineLoadingScreen(app);
                return;
            }
        }
        else
        {
            ApplyOnlineWorldSnapshot(app);
        }

        DetectDeathExplosions(
            combatants,
            trackedDeathNames,
            trackedDeathCounts,
            deathExplosions,
            explosionParticles
        );

        UpdateDeathExplosions(
            deathExplosions,
            explosionParticles,
            dt
        );

        if (
            onlineLocalPlayerIndex < 0 ||
            onlineLocalPlayerIndex >=
                (int)combatants.size()
        )
        {
            DrawOnlineLoadingScreen(app);
            return;
        }

        // Camera now follows horizontal mouse movement continuously.
        UpdateArenaMouseLook();

        Combatant& player =
            combatants[onlineLocalPlayerIndex];

        // RMB is reserved for placing a server-authoritative mine.
        if (
            player.alive &&
            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)
        )
        {
            NetSendLine("ARENA_MINE");
        }

        Vector3 cameraForward = {
            -std::sin(cameraYaw),
            0.0f,
            -std::cos(cameraYaw)
        };

        Vector3 cameraRight = {
            std::cos(cameraYaw),
            0.0f,
            -std::sin(cameraYaw)
        };

        Vector3 movement = {
            0.0f,
            0.0f,
            0.0f
        };

        if (player.alive)
        {
            if (IsKeyDown(KEY_W))
                movement = Add(movement, cameraForward);

            if (IsKeyDown(KEY_S))
                movement = Add(
                    movement,
                    Scale(cameraForward, -1.0f)
                );

            if (IsKeyDown(KEY_A))
                movement = Add(
                    movement,
                    Scale(cameraRight, -1.0f)
                );

            if (IsKeyDown(KEY_D))
                movement = Add(movement, cameraRight);
        }

        movement = NormalizeXZ(movement);

        // Immediate local prediction. The authoritative server sends the
        // position back and SmoothOnlineCombatants() reconciles it.
        if (player.alive)
        {
            MoveCombatant(
                player,
                movement,
                dt,
                PLAYER_SPEED,
                obstacles
            );
        }

        SmoothOnlineCombatants(dt);

        // ----------------------------------------------------
        // Third-person camera with the same wall collision used locally.
        // ----------------------------------------------------
        camera.target = {
            player.position.x,
            player.position.y +
                cameraTargetHeight,
            player.position.z
        };

        Vector3 desiredCameraPosition = {
            player.position.x +
                std::sin(cameraYaw) *
                cameraDistance,

            player.position.y +
                cameraHeight,

            player.position.z +
                std::cos(cameraYaw) *
                cameraDistance
        };

        Vector3 collisionSafeCameraPosition =
            ResolveCameraCollision(
                camera.target,
                desiredCameraPosition,
                obstacles
            );

        Vector3 desiredCameraDirection =
            Subtract(
                desiredCameraPosition,
                camera.target
            );

        float desiredCameraRayLength =
            Length3D(
                desiredCameraDirection
            );

        Vector3 cameraDirection = {
            0.0f,
            0.0f,
            1.0f
        };

        if (desiredCameraRayLength > 0.0001f)
        {
            cameraDirection =
                Scale(
                    desiredCameraDirection,
                    1.0f /
                        desiredCameraRayLength
                );
        }

        float collisionSafeDistance =
            Length3D(
                Subtract(
                    collisionSafeCameraPosition,
                    camera.target
                )
            );

        if (
            collisionSafeDistance <
            currentCameraDistance
        )
        {
            currentCameraDistance =
                collisionSafeDistance;
        }
        else
        {
            float returnAlpha =
                1.0f -
                std::exp(
                    -cameraReturnSpeed *
                    dt
                );

            currentCameraDistance +=
                (
                    collisionSafeDistance -
                    currentCameraDistance
                ) *
                returnAlpha;
        }

        camera.position =
            Add(
                camera.target,
                Scale(
                    cameraDirection,
                    currentCameraDistance
                )
            );

        Vector3 cameraAimDirection =
            NormalizeXZ(
                Subtract(
                    camera.target,
                    camera.position
                )
            );

        if (
            player.alive &&
            LengthXZ(
                cameraAimDirection
            ) > 0.001f
        )
        {
            player.aimDirection =
                cameraAimDirection;
        }

        // Send whether LMB is currently held. The server owns the actual
        // fire-rate cooldown, so a shot request cannot be lost between
        // the client's 30 Hz network input packets.
        bool fireRequested =
            player.alive &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT);

        // ----------------------------------------------------
        // Send movement + aim + fire input at 30 Hz.
        // ----------------------------------------------------
        onlineInputSendAccumulator += dt;

        constexpr float INPUT_INTERVAL =
            1.0f / 30.0f;

        if (
            onlineInputSendAccumulator >=
            INPUT_INTERVAL
        )
        {
            onlineInputSendAccumulator =
                std::fmod(
                    onlineInputSendAccumulator,
                    INPUT_INTERVAL
                );

            onlineInputSequence++;

            float aimYaw =
                DirectionYaw(
                    player.aimDirection
                );

            std::string packet =
                std::string("ARENA_INPUT|") +
                std::to_string(
                    onlineInputSequence
                ) +
                "|" +
                std::to_string(movement.x) +
                "|" +
                std::to_string(movement.z) +
                "|" +
                std::to_string(aimYaw) +
                "|" +
                std::to_string(fireRequested ? 1 : 0);

            NetSendLine(packet);
        }

        // ----------------------------------------------------
        // Render
        // ----------------------------------------------------
        BeginTextureMode(arenaTarget);

        ClearBackground(
            Color{11, 12, 16, 255}
        );

        BeginMode3D(camera);

        DrawArena(obstacles);

        for (Combatant& combatant : combatants)
        {
            DrawTank(
                combatant,
                bodyModel,
                turretModel
            );
        }

        DrawProjectiles(projectiles);
        DrawMines(mines);
        DrawDeathExplosions(
            deathExplosions,
            explosionParticles
        );

        EndMode3D();

        DrawNameplates(
            combatants,
            camera,
            obstacles,
            settings
        );

        DrawHud(
            combatants,
            settings,
            timeRemaining,
            result,
            onlineLocalPlayerIndex
        );

        DrawOnlineCombatBanner(app);

        if (IsKeyDown(KEY_TAB))
        {
            DrawScoreboardOverlay(
                combatants,
                settings,
                timeRemaining
            );
        }

        EndTextureMode();
    }


    void DrawOnlinePostGameScreen(AppState& app, float dt)
    {
        ShowCursor();

        if (!app.arena.worldPlayers.empty())
        {
            if (!onlineMatchInitialized)
                BuildOnlineCombatants(app);
            else
                ApplyOnlineWorldSnapshot(app);
        }

        DetectDeathExplosions(
            combatants,
            trackedDeathNames,
            trackedDeathCounts,
            deathExplosions,
            explosionParticles
        );

        UpdateDeathExplosions(
            deathExplosions,
            explosionParticles,
            dt
        );

        BeginTextureMode(arenaTarget);

        ClearBackground(
            Color{11, 12, 16, 255}
        );

        // Keep a frozen view of the final battlefield behind the results.
        if (!combatants.empty())
        {
            BeginMode3D(camera);

            DrawArena(obstacles);

            for (Combatant& combatant : combatants)
            {
                DrawTank(
                    combatant,
                    bodyModel,
                    turretModel
                );
            }

            DrawProjectiles(projectiles);
            DrawMines(mines);
            DrawDeathExplosions(
                deathExplosions,
                explosionParticles
            );

            EndMode3D();
        }

        DrawScoreboardOverlay(
            combatants,
            settings,
            timeRemaining
        );

        DrawText(
            "MATCH COMPLETE",
            SCREEN_WIDTH / 2 -
                MeasureText("MATCH COMPLETE", 28) / 2,
            24,
            28,
            JENG_YELLOW
        );

        int resultWidth =
            MeasureText(
                app.arena.status.c_str(),
                18
            );

        DrawText(
            app.arena.status.c_str(),
            SCREEN_WIDTH / 2 - resultWidth / 2,
            60,
            18,
            RAYWHITE
        );

        Rectangle playAgainButton = {
            SCREEN_WIDTH / 2.0f - 290.0f,
            642.0f,
            260.0f,
            50.0f
        };

        Rectangle exitButton = {
            SCREEN_WIDTH / 2.0f + 30.0f,
            642.0f,
            260.0f,
            50.0f
        };

        if (
            MenuButton(
                playAgainButton,
                "PLAY AGAIN",
                true
            )
        )
        {
            if (NetSendLine("ARENA_PLAY_AGAIN"))
            {
                app.arena.status =
                    "Returning the party to the Arena lobby...";
            }
            else
            {
                app.arena.status = NetLastError();
            }
        }

        if (
            MenuButton(
                exitButton,
                "EXIT PARTY"
            )
        )
        {
            NetSendLine("ARENA_LEAVE");

            app.arena.active = false;
            app.arena.matchActive = false;
            app.arena.startSignalReceived = false;
            app.arena.players.clear();
            app.arena.worldPlayers.clear();
            app.arena.worldProjectiles.clear();
            app.arena.worldMines.clear();
            app.arena.timeRemainingSeconds = 0.0f;
            app.arena.phase = "WAITING";
            app.arena.status =
                "Left the Arena party.";

            ResetOnlineMatchState();
            inSetup = true;
            ShowCursor();
        }

        EndTextureMode();
    }


    void StartArenaMatch()
    {
        NormalizeSettings(settings);

        ConfigureCombatants(
            combatants,
            settings,
            arenaUsername
        );

        projectiles.clear();
        mines.clear();
        deathExplosions.clear();
        explosionParticles.clear();
        SyncDeathExplosionTracker(
            combatants,
            trackedDeathNames,
            trackedDeathCounts
        );

        result = MatchResult{};
        timeRemaining = settings.timeLimitSeconds;

        ResetArenaCamera();

        BeginArenaMouseLook();
        inSetup = false;
    }


    void ReturnArenaToSetup()
    {
        inSetup = true;
        ShowCursor();
    }


    void EnsureArenaInitialized(
        const std::string& username)
    {
        if (arenaInitialized)
        {
            if (!username.empty())
                arenaUsername = username;

            // Arena feels substantially better at a high frame cap.
            SetTargetFPS(144);
            return;
        }

        arenaUsername =
            username.empty()
            ? "PLAYER"
            : username;

        arenaTarget =
            LoadRenderTexture(
                SCREEN_WIDTH,
                SCREEN_HEIGHT
            );

        SetTextureFilter(
            arenaTarget.texture,
            TEXTURE_FILTER_BILINEAR
        );

        bodyModel =
            LoadModelFromMesh(
                GenMeshCube(
                    1.45f,
                    0.62f,
                    2.05f
                )
            );

        turretModel =
            LoadModelFromMesh(
                GenMeshCube(
                    1.05f,
                    0.36f,
                    0.95f
                )
            );

        BuildArenaObstacles();

        settings = MatchSettings{};
        result = MatchResult{};

        combatants.clear();
        projectiles.clear();
        mines.clear();
        deathExplosions.clear();
        explosionParticles.clear();
        trackedDeathNames.clear();
        trackedDeathCounts.clear();

        inSetup = true;
        timeRemaining = settings.timeLimitSeconds;

        ResetArenaCamera();

        ShowCursor();

        SetTargetFPS(144);

        arenaInitialized = true;
    }
}


void ArenaUpdateAndRender(
    AppState& app)
{
    EnsureArenaInitialized(
        app.username
    );

    float dt =
        std::min(
            GetFrameTime(),
            1.0f / 30.0f
        );

    // ========================================================
    // ONLINE LOBBY
    // ========================================================

    if (
        app.arena.active &&
        app.arena.phase == "PLAYING"
    )
    {
        UpdateOnlineArenaMatch(
            app,
            dt
        );
        return;
    }

    if (
        app.arena.active &&
        app.arena.phase == "POSTGAME"
    )
    {
        DrawOnlinePostGameScreen(app, dt);
        return;
    }

    if (app.arena.active)
    {
        if (onlineMatchInitialized)
            ResetOnlineMatchState();

        ShowCursor();

        BeginTextureMode(
            arenaTarget
        );

        ClearBackground(
            Color{11, 12, 16, 255}
        );

        DrawArenaLobbyScreen(
            app
        );

        EndTextureMode();
        return;
    }


    // ========================================================
    // MATCH SETUP
    // ========================================================

    if (inSetup)
    {
        ShowCursor();

        BeginTextureMode(arenaTarget);

        ClearBackground(
            Color{11, 12, 16, 255}
        );

        bool startMatch =
            DrawSetupScreen(
                settings,
                arenaUsername,
                app
            );

        EndTextureMode();

        if (startMatch)
            StartArenaMatch();

        return;
    }


    if (combatants.empty())
    {
        ReturnArenaToSetup();
        return;
    }


    // Return to match setup at any time.
    if (IsKeyPressed(KEY_M))
    {
        ReturnArenaToSetup();
        return;
    }


    // Rematch with the same settings.
    if (IsKeyPressed(KEY_R))
    {
        ConfigureCombatants(
            combatants,
            settings,
            arenaUsername
        );

        projectiles.clear();
        mines.clear();
        deathExplosions.clear();
        explosionParticles.clear();
        SyncDeathExplosionTracker(
            combatants,
            trackedDeathNames,
            trackedDeathCounts
        );

        result = MatchResult{};
        timeRemaining =
            settings.timeLimitSeconds;

        ResetArenaCamera();
        BeginArenaMouseLook();
    }


    // Camera follows horizontal mouse movement continuously.
    UpdateArenaMouseLook();


    Combatant& playerRef =
        combatants[0];

    // RMB places a mine in local test mode too.
    if (
        !result.finished &&
        playerRef.alive &&
        IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)
    )
    {
        PlaceLocalMine(
            0,
            combatants,
            mines
        );
    }


    // ========================================================
    // ACTIVE MATCH SIMULATION
    // ========================================================

    if (!result.finished)
    {
        if (IsTimedMode(settings))
        {
            timeRemaining =
                std::max(
                    0.0f,
                    timeRemaining - dt
                );
        }

        if (playerRef.alive)
        {
            Vector3 cameraForward = {
                -sinf(cameraYaw),
                0.0f,
                -cosf(cameraYaw)
            };

            Vector3 cameraRight = {
                cosf(cameraYaw),
                0.0f,
                -sinf(cameraYaw)
            };

            Vector3 movement = {
                0.0f,
                0.0f,
                0.0f
            };

            if (IsKeyDown(KEY_W))
            {
                movement =
                    Add(
                        movement,
                        cameraForward
                    );
            }

            if (IsKeyDown(KEY_S))
            {
                movement =
                    Add(
                        movement,
                        Scale(
                            cameraForward,
                            -1.0f
                        )
                    );
            }

            if (IsKeyDown(KEY_A))
            {
                movement =
                    Add(
                        movement,
                        Scale(
                            cameraRight,
                            -1.0f
                        )
                    );
            }

            if (IsKeyDown(KEY_D))
            {
                movement =
                    Add(
                        movement,
                        cameraRight
                    );
            }

            MoveCombatant(
                playerRef,
                movement,
                dt,
                PLAYER_SPEED,
                obstacles
            );
        }
    }


    // ========================================================
    // THIRD-PERSON CAMERA
    // ========================================================

    camera.target = {
        playerRef.position.x,
        playerRef.position.y +
            cameraTargetHeight,
        playerRef.position.z
    };

    Vector3 desiredCameraPosition = {
        playerRef.position.x +
            sinf(cameraYaw) *
            cameraDistance,

        playerRef.position.y +
            cameraHeight,

        playerRef.position.z +
            cosf(cameraYaw) *
            cameraDistance
    };

    Vector3 collisionSafeCameraPosition =
        ResolveCameraCollision(
            camera.target,
            desiredCameraPosition,
            obstacles
        );

    Vector3 desiredCameraDirection =
        Subtract(
            desiredCameraPosition,
            camera.target
        );

    float desiredCameraRayLength =
        Length3D(
            desiredCameraDirection
        );

    Vector3 cameraDirection = {
        0.0f,
        0.0f,
        1.0f
    };

    if (desiredCameraRayLength > 0.0001f)
    {
        cameraDirection =
            Scale(
                desiredCameraDirection,
                1.0f /
                    desiredCameraRayLength
            );
    }

    float collisionSafeDistance =
        Length3D(
            Subtract(
                collisionSafeCameraPosition,
                camera.target
            )
        );

    if (
        collisionSafeDistance <
        currentCameraDistance
    )
    {
        currentCameraDistance =
            collisionSafeDistance;
    }
    else
    {
        float returnAlpha =
            1.0f -
            std::exp(
                -cameraReturnSpeed *
                dt
            );

        currentCameraDistance +=
            (
                collisionSafeDistance -
                currentCameraDistance
            ) *
            returnAlpha;
    }

    camera.position =
        Add(
            camera.target,
            Scale(
                cameraDirection,
                currentCameraDistance
            )
        );


    // ========================================================
    // AIM + COMBAT
    // ========================================================

    if (!result.finished)
    {
        if (playerRef.alive)
        {
            Vector3 cameraAimDirection =
                NormalizeXZ(
                    Subtract(
                        camera.target,
                        camera.position
                    )
                );

            if (
                LengthXZ(
                    cameraAimDirection
                ) > 0.001f
            )
            {
                playerRef.aimDirection =
                    cameraAimDirection;
            }

            playerRef.fireTimer -= dt;

            if (
                IsMouseButtonDown(
                    MOUSE_BUTTON_LEFT
                ) &&
                playerRef.fireTimer <= 0.0f
            )
            {
                FireProjectile(
                    0,
                    playerRef,
                    projectiles
                );

                playerRef.fireTimer =
                    PLAYER_FIRE_COOLDOWN;
            }
        }

        UpdateBots(
            combatants,
            projectiles,
            obstacles,
            settings,
            dt
        );


        // ====================================================
        // PROJECTILES
        // ====================================================

        for (Projectile& projectile : projectiles)
        {
            if (!projectile.active)
                continue;

            projectile.position =
                Add(
                    projectile.position,
                    Scale(
                        projectile.velocity,
                        dt
                    )
                );

            if (
                std::fabs(
                    projectile.position.x
                ) > 22.0f ||
                std::fabs(
                    projectile.position.z
                ) > 22.0f
            )
            {
                projectile.active = false;
                continue;
            }

            if (
                ProjectileHitsObstacle(
                    projectile,
                    obstacles
                )
            )
            {
                projectile.active = false;
                continue;
            }

            for (
                int i = 0;
                i < (int)combatants.size();
                i++
            )
            {
                if (
                    i == projectile.owner ||
                    !combatants[i].alive
                )
                {
                    continue;
                }

                if (
                    projectile.owner >= 0 &&
                    projectile.owner <
                        (int)combatants.size() &&
                    !AreEnemies(
                        combatants[projectile.owner],
                        combatants[i],
                        settings
                    )
                )
                {
                    // Friendly fire is disabled in team modes.
                    continue;
                }

                Vector3 center = {
                    combatants[i].position.x,
                    0.80f,
                    combatants[i].position.z
                };

                if (
                    CheckCollisionSpheres(
                        projectile.position,
                        BULLET_RADIUS,
                        center,
                        TANK_RADIUS
                    )
                )
                {
                    DamageCombatant(
                        projectile.owner,
                        i,
                        combatants
                    );

                    projectile.active = false;
                    break;
                }
            }
        }

        projectiles.erase(
            std::remove_if(
                projectiles.begin(),
                projectiles.end(),
                [](const Projectile& p)
                {
                    return !p.active;
                }
            ),
            projectiles.end()
        );

        UpdateLocalMines(
            mines,
            combatants,
            settings
        );

        UpdateRespawns(
            combatants,
            dt
        );
    }

    DetectDeathExplosions(
        combatants,
        trackedDeathNames,
        trackedDeathCounts,
        deathExplosions,
        explosionParticles
    );

    UpdateDeathExplosions(
        deathExplosions,
        explosionParticles,
        dt
    );


    // Evaluate after all gameplay for this frame.
    if (!result.finished)
    {
        MatchResult evaluated =
            EvaluateMatch(
                combatants,
                settings,
                timeRemaining
            );

        if (evaluated.finished)
        {
            result = evaluated;
            projectiles.clear();
        }
    }


    // ========================================================
    // DRAW INTO ARENA'S OWN 1280 x 720 TARGET
    // ========================================================

    BeginTextureMode(arenaTarget);

    ClearBackground(
        Color{11, 12, 16, 255}
    );

    BeginMode3D(camera);

    DrawArena(obstacles);

    for (Combatant& combatant : combatants)
    {
        DrawTank(
            combatant,
            bodyModel,
            turretModel
        );
    }

    DrawProjectiles(projectiles);
    DrawMines(mines);
    DrawDeathExplosions(
        deathExplosions,
        explosionParticles
    );

    EndMode3D();

    DrawNameplates(
        combatants,
        camera,
        obstacles,
        settings
    );

    DrawHud(
        combatants,
        settings,
        timeRemaining,
        result
    );

    // Hold-to-view scoreboard.
    if (IsKeyDown(KEY_TAB))
    {
        DrawScoreboardOverlay(
            combatants,
            settings,
            timeRemaining
        );
    }

    EndTextureMode();
}


void ArenaDrawToWindow()
{
    if (!arenaInitialized)
        return;

    DrawArenaRenderTarget(
        arenaTarget
    );
}


void ArenaHandleEscape(AppState& app)
{
    if (!arenaInitialized)
    {
        app.gameView = GameView::HOME;
        ShowCursor();
        SetTargetFPS(60);
        return;
    }

    if (app.arena.active)
    {
        NetSendLine(
            "ARENA_LEAVE"
        );

        app.arena.active = false;
        app.arena.matchActive = false;
        app.arena.startSignalReceived = false;
        app.arena.players.clear();
        app.arena.worldPlayers.clear();
        app.arena.worldProjectiles.clear();
        app.arena.worldMines.clear();
        app.arena.timeRemainingSeconds = 0.0f;
        app.arena.phase = "WAITING";
        app.arena.status =
            "Left Arena lobby.";

        ResetOnlineMatchState();

        app.gameView =
            GameView::HOME;

        ShowCursor();
        SetTargetFPS(60);
        return;
    }

    if (!inSetup)
    {
        ReturnArenaToSetup();
        return;
    }

    // ESC from the Arena setup screen goes back to the normal
    // JENG CHAT game hub without opening a second application.
    app.gameView = GameView::HOME;
    ShowCursor();
    SetTargetFPS(60);
}


void ArenaShutdown()
{
    if (!arenaInitialized)
        return;

    ShowCursor();

    UnloadModel(bodyModel);
    UnloadModel(turretModel);
    UnloadRenderTexture(arenaTarget);

    obstacles.clear();
    ResetOnlineMatchState();

    arenaInitialized = false;

    SetTargetFPS(60);
}
