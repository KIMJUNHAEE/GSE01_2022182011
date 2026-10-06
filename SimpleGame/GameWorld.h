#pragma once
#include "WorldActors.h"
#include "SceneGraph.h"
#include "NavigationGrid.h"
#include <array>
#include <deque>
#include <cstdint>
#include <random>

struct District
{
    int x0, y0, x1, y1, kind;
};

struct DialogueLine
{
    std::wstring speaker, text;
};
enum class View
{
    Title,
    Explore,
    Dialogue,
    Map,
    Journal,
    Pause,
    Ending,
    Defeat
};

class GameWorld
{
public:

    explicit GameWorld(uint32_t seed = 0);
    View view = View::Title;
    SceneGraph scene;
    Vec2 facing = {1, 0};
    std::vector<District> districts;
    std::array<ActorId, 3> relays = {};
    std::array<bool, 3> restored = {false, false, false};
    std::array<ActorId, 3> memories = {};
    std::array<bool, 3> found = {false, false, false};
    std::deque<DialogueLine> dialogue;
    std::deque<Vec2> trail;
    int quest = 0, choice = -1;
    ActorId nearby = InvalidActor;
    bool choicePending = false, walking = false, captainWalking = false;
    float time = 0, scanCooldown = 0, scanWave = -1, toastTime = 0;
    std::wstring toast;
    PlayerStats stats;
    std::array<Vec2, 5> rooms;
    Vec2 bossHome;
    Vec2 aim = {1, 0};
    uint32_t mapSeed = 0;
    int kills = 0, itemsCollected = 0, shotsFired = 0;
    bool firing = false, bossActive = false, bossDefeated = false;
    float shotCooldown = 0, invulnerability = 0, magnetTime = 0, muzzleFlash = 0;

    struct SceneRoots
    {
        ActorId terrain = 0, environment = 0, characters = 0, enemies = 0;
        ActorId projectiles = 0, loot = 0, effects = 0;
    } roots;

    Vec2 PlayerPosition() const;
    bool PlayerActive() const;
    Vec2 CaptainPosition() const;
    void SetPlayerPosition(Vec2 position);
    void SetCaptainPosition(Vec2 position);
    Vec2 RelayPosition(int index) const;
    Vec2 MemoryPosition(int index) const;
    Vec2 ShipPosition() const;
    Vec2 CorePosition() const;
    Vec2 ResidentPosition() const;

    template <typename T> T& Spawn(const T& prototype)
    {
        ActorId parent = roots.effects;
        if constexpr (std::is_same_v<T, Prop>)
        {
            parent = roots.environment;
        }
        else if constexpr (std::is_same_v<T, Terrain>)
        {
            parent = roots.terrain;
        }
        else if constexpr (std::is_same_v<T, Enemy>)
        {
            parent = roots.enemies;
        }
        else if constexpr (std::is_same_v<T, Projectile>)
        {
            parent = roots.projectiles;
        }
        else if constexpr (std::is_same_v<T, Loot>)
        {
            parent = roots.loot;
        }
        else if constexpr (std::is_same_v<T, Character>)
        {
            parent = roots.characters;
        }
        T& actor = scene.Add(prototype, parent);
        actor.SetPosition(prototype.Position());
        if constexpr (std::is_same_v<T, Prop>)
        {
            scene.Add(SceneEffect(EffectKind::PropGround), actor.Id());
        }
        if constexpr (std::is_same_v<T, Enemy>)
        {
            scene.Add(SceneEffect(EffectKind::EnemyWarning), actor.Id());
        }
        return actor;
    }

    void GainExperience(int amount);
    void AddLoot(LootKind kind, Vec2 position, int amount = 1);
    void DamagePlayer(float amount);
    void BeginBoss();
    bool ValidateNavigation() const;
    bool ClearPath(Vec2 from, Vec2 to, float radius = .22f) const;
    std::wstring TutorialText() const;

    static Vec2 Unproject(Vec2 screen)
    {
        return {screen.x / 84 + screen.y / 42, screen.y / 42 - screen.x / 84};
    }

    int Tile(int x, int y) const;
    bool Walkable(Vec2 p, float radius = .22f) const;
    void Move(Vec2 screenDirection, float dt, bool running);
    void Update(float dt);
    void Start();
    void Interact();
    void Advance();
    void Choose(int option);
    void Scan();
    void Notify(const std::wstring& text);
    int RestoredCount() const;
    int MemoryCount() const;
    Vec2 Objective() const;
    std::wstring ObjectiveText() const;
    std::wstring NearbyText() const;
    std::wstring Region() const;
    static float Distance(Vec2 a, Vec2 b);

    static Vec2 Project(Vec2 grid)
    {
        return {(grid.x - grid.y) * 42, (grid.x + grid.y) * 21};
    }

private:

    void Say(std::initializer_list<DialogueLine> lines);
    void UpdateNearby();
    void GenerateLevel();
    void BuildNavigation() const;
    void EnsureNavigation() const;
    void BuildFlow();
    void UpdateCombat(float dt);
    void UpdateEnemies(float dt);
    void UpdateProjectiles(float dt);
    void UpdateLoot(float dt);
    void SpawnCamp(int camp);
    void Fire(Vec2 position, Vec2 direction, bool hostile, float damage, float range, float speed,
              bool critical = false);
    void DefeatEnemy(const Enemy& enemy);
    void Collect(const Loot& item);
    void MoveActor(Actor& actor, Vec2 target, float speed, float dt);
    Vec2 FlowTarget(Vec2 position) const;
    int NearestNode(Vec2 position) const;
    bool CampClear(int camp) const;

    mutable NavigationGrid navigation;
    mutable uint64_t navigationRevision = 0;
    mutable uint64_t collisionRevision = 0;
    mutable std::map<std::pair<int, int>, int> floorTiles;

    struct Obstacle
    {
        Vec2 position;
        float width, depth;
    };

    mutable std::vector<Obstacle> obstacles;
    void EnsureCollision() const;
    ActorId playerId = 0, captainId = 0, shipId = 0, coreId = 0, residentId = 0;
    ActorId scanEffectId = 0;
    std::array<float, 3> respawn = {25, 25, 25};
    std::mt19937 random;
    bool runStarted = false, metResident = false;
};
