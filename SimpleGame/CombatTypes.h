#pragma once
#include "Canvas.h"

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

struct Enemy
{
    EnemyKind kind = EnemyKind::Drone;
    Vec2 p;
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

struct Projectile
{
    Vec2 p;
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

struct Loot
{
    LootKind kind = LootKind::Soul;
    Vec2 p;
    int amount = 1;
    bool attracted = false;
};

struct CombatText
{
    Vec2 p;
    std::wstring text;
    unsigned color = 0xffffff;
    float life = 1;
};
