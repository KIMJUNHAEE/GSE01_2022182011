#pragma once
#include "Canvas.h"
#include "CombatTypes.h"
#include <array>
#include <deque>
#include <cstdint>
#include <random>

enum class PropKind
{
    Habitat,
    Archive,
    Tree,
    Crate,
    Lamp,
    Relay,
    Core,
    Ship,
    Citizen,
    Memory
};

struct Prop
{
    PropKind kind;
    Vec2 p;
    float w, d, h;
    int variant = 0;
    bool solid = true;
};

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
    Vec2 player = {-5, 1}, captain = {-5.7f, 2.2f}, facing = {1, 0};
    std::vector<District> districts;
    std::vector<Prop> props;
    std::array<Vec2, 3> relays = {Vec2{5, -11}, Vec2{15, 1}, Vec2{5, 3}};
    std::array<bool, 3> restored = {false, false, false};
    std::array<Vec2, 3> memories = {Vec2{-8, -3}, Vec2{1, -12}, Vec2{18, -3}};
    std::array<bool, 3> found = {false, false, false};
    std::deque<DialogueLine> dialogue;
    std::deque<Vec2> trail;
    int quest = 0, choice = -1, nearby = -1;
    bool choicePending = false, walking = false, captainWalking = false;
    float time = 0, scanCooldown = 0, scanWave = -1, toastTime = 0;
    std::wstring toast;
    Vec2 scanOrigin;
    PlayerStats stats;
    std::vector<Enemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<Loot> loot;
    std::vector<CombatText> combatText;
    std::array<Vec2, 5> rooms;
    Vec2 shipPosition, corePosition, residentPosition, bossHome;
    Vec2 aim = {1, 0};
    uint32_t mapSeed = 0;
    int kills = 0, itemsCollected = 0, shotsFired = 0;
    bool firing = false, bossActive = false, bossDefeated = false;
    float shotCooldown = 0, invulnerability = 0, magnetTime = 0, muzzleFlash = 0;

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
    void BuildNavigation();
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
    void MoveActor(Vec2& position, Vec2 target, float speed, float dt);
    Vec2 FlowTarget(Vec2 position) const;
    int NearestNode(Vec2 position) const;
    bool CampClear(int camp) const;

    static constexpr int NavWidth = 61;
    static constexpr int NavHeight = 45;
    static constexpr int NavCount = NavWidth * NavHeight;
    std::array<unsigned char, NavCount> navigation = {};
    std::array<unsigned char, NavCount> edges = {};
    std::array<int, NavCount> flow = {};
    std::array<float, 3> respawn = {25, 25, 25};
    std::mt19937 random;
    int flowSource = -1;
    bool runStarted = false, metResident = false;

    static Vec2 NodePosition(int index)
    {
        return {-10.f + (index % NavWidth) * .5f, -17.f + (index / NavWidth) * .5f};
    }
};
