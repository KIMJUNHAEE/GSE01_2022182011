#pragma once
#include "CombatTypes.h"

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

class Prop : public Actor
{
public:

    Prop(PropKind kind, Vec2 position, float width, float depth, float height, int variant = 0,
         bool solid = true);
    std::unique_ptr<Actor> Clone() const override;
    void Draw(ActorRenderer& renderer) const override;
    bool BlocksNavigation() const override;
    float Width() const;
    float Depth() const;
    void SetFootprint(float width, float depth);
    void SetSolid(bool value);
    PropKind kind;
    float h;
    int variant;

private:

    float w, d;
    bool solid;
};

class Character : public Actor
{
public:

    Character(Vec2 position, bool captain = false);
    std::unique_ptr<Actor> Clone() const override;
    void Draw(ActorRenderer& renderer) const override;
    bool captain;
};

class Terrain : public Actor
{
public:

    Terrain(Vec2 position, int kind);
    std::unique_ptr<Actor> Clone() const override;
    void Draw(ActorRenderer& renderer) const override;
    bool BlocksNavigation() const override;
    const int kind;
};

enum class EffectKind
{
    LandingPad,
    AmbientDust,
    Scan,
    PlayerRing,
    Weapon,
    Magnet,
    EnemyWarning,
    PropGround
};

class SceneEffect : public Actor
{
public:

    SceneEffect(EffectKind kind, Vec2 position = {});
    std::unique_ptr<Actor> Clone() const override;
    void Draw(ActorRenderer& renderer) const override;
    EffectKind kind;
};
