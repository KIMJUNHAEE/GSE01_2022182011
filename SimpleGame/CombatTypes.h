#pragma once
#include "Actor.h"

struct PlayerStats
{
    static constexpr int MaxLevel = 80;
    static constexpr int MaxWeaponTier = 20;

    int level = 1;
    int experience = 0;
    int weaponTier = 0;
    float health = 100;

    int NextLevelExperience() const;
    float MaxHealth() const;
    float Damage() const;
    float ShotInterval() const;
    float Range() const;
    float MoveSpeed() const;
    float PickupRange() const;
    float CriticalChance() const;
};

enum class EnemyKind
{
    Drone,
    Sentry,
    Boss
};

class Enemy : public Actor
{
public:

    std::unique_ptr<Actor> Clone() const override;
    void Draw(ActorRenderer& renderer) const override;
    Enemy();
    EnemyKind kind = EnemyKind::Drone;
    Vec2 home;
    Vec2 target;
    float health = 32;
    float maxHealth = 32;
    float cooldown = 1;
    float windup = 0;
    float flash = 0;
    int attack = 0;
    int camp = 0;
};

class Projectile : public Actor
{
public:

    std::unique_ptr<Actor> Clone() const override;
    void Draw(ActorRenderer& renderer) const override;
    Projectile(Vec2 position = {}, Vec2 direction = {}, float speed = 660, float range = 350,
               float damage = 12, bool hostile = false, bool critical = false);
    Vec2 direction;
    float speed = 660;
    float remaining = 350;
    float damage = 12;
    bool hostile = false;
    bool critical = false;
};

enum class LootKind
{
    Weapon,
    Health,
    Soul,
    Magnet
};

class Loot : public Actor
{
public:

    std::unique_ptr<Actor> Clone() const override;
    void Draw(ActorRenderer& renderer) const override;
    Loot(LootKind kind = LootKind::Soul, Vec2 position = {}, int amount = 1,
         bool attracted = false);
    LootKind kind = LootKind::Soul;
    int amount = 1;
    bool attracted = false;
};

class CombatText : public Actor
{
public:

    std::unique_ptr<Actor> Clone() const override;
    void Draw(ActorRenderer& renderer) const override;
    CombatText(Vec2 position = {}, std::wstring text = L"", unsigned color = 0xffffff,
               float life = 1);
    void Update(float dt) override;
    std::wstring text;
    unsigned color = 0xffffff;
    float life = 1;
};
