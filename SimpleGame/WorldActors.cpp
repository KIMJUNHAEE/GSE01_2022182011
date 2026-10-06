#include "stdafx.h"
#include "WorldActors.h"
#include "ActorRenderer.h"
#include <utility>

Prop::Prop(PropKind kind, Vec2 position, float width, float depth, float height, int variant,
           bool solid)
    : Actor("Prop", position), kind(kind), h(height), variant(variant), w(width), d(depth),
      solid(solid)
{
}

bool Prop::BlocksNavigation() const
{
    return solid;
}

float Prop::Width() const
{
    return w;
}

float Prop::Depth() const
{
    return d;
}

void Prop::SetFootprint(float width, float depth)
{
    w = width;
    d = depth;
    InvalidateNavigation();
}

void Prop::SetSolid(bool value)
{
    if (solid != value)
    {
        InvalidateNavigation();
        solid = value;
        InvalidateNavigation();
    }
}

Character::Character(Vec2 position, bool captain)
    : Actor(captain ? "Captain" : "Player", position), captain(captain)
{
}

Terrain::Terrain(Vec2 position, int kind)
    : Actor("Terrain", position, RenderLayer::Terrain), kind(kind)
{
}

bool Terrain::BlocksNavigation() const
{
    // Moving or disabling a floor tile also invalidates the navigation grid.
    return true;
}

SceneEffect::SceneEffect(EffectKind kind, Vec2 position)
    : Actor("Effect", position,
            kind == EffectKind::Weapon
                ? RenderLayer::World
                : (kind == EffectKind::LandingPad || kind == EffectKind::Magnet ||
                           kind == EffectKind::EnemyWarning || kind == EffectKind::PropGround
                       ? RenderLayer::GroundEffect
                       : RenderLayer::Overlay)),
      kind(kind)
{
}

Enemy::Enemy() : Actor("Enemy")
{
}

Projectile::Projectile(Vec2 position, Vec2 direction, float speed, float range, float damage,
                       bool hostile, bool critical)
    : Actor("Projectile", position, RenderLayer::Projectile), direction(direction), speed(speed),
      remaining(range), damage(damage), hostile(hostile), critical(critical)
{
}

Loot::Loot(LootKind kind, Vec2 position, int amount, bool attracted)
    : Actor("Loot", position, RenderLayer::GroundEffect), kind(kind), amount(amount),
      attracted(attracted)
{
}

CombatText::CombatText(Vec2 position, std::wstring text, unsigned color, float life)
    : Actor("CombatText", position, RenderLayer::Overlay), text(std::move(text)), color(color),
      life(life)
{
}

void CombatText::Update(float dt)
{
    life -= dt;
    if (life <= 0)
    {
        Destroy();
    }
}

std::unique_ptr<Actor> Prop::Clone() const
{
    return std::make_unique<Prop>(*this);
}

void Prop::Draw(ActorRenderer& renderer) const
{
    renderer.Draw(*this);
}

std::unique_ptr<Actor> Character::Clone() const
{
    return std::make_unique<Character>(*this);
}

void Character::Draw(ActorRenderer& renderer) const
{
    renderer.Draw(*this);
}

std::unique_ptr<Actor> Terrain::Clone() const
{
    return std::make_unique<Terrain>(*this);
}

void Terrain::Draw(ActorRenderer& renderer) const
{
    renderer.Draw(*this);
}

std::unique_ptr<Actor> SceneEffect::Clone() const
{
    return std::make_unique<SceneEffect>(*this);
}

void SceneEffect::Draw(ActorRenderer& renderer) const
{
    renderer.Draw(*this);
}

std::unique_ptr<Actor> Enemy::Clone() const
{
    return std::make_unique<Enemy>(*this);
}

void Enemy::Draw(ActorRenderer& renderer) const
{
    renderer.Draw(*this);
}

std::unique_ptr<Actor> Projectile::Clone() const
{
    return std::make_unique<Projectile>(*this);
}

void Projectile::Draw(ActorRenderer& renderer) const
{
    renderer.Draw(*this);
}

std::unique_ptr<Actor> Loot::Clone() const
{
    return std::make_unique<Loot>(*this);
}

void Loot::Draw(ActorRenderer& renderer) const
{
    renderer.Draw(*this);
}

std::unique_ptr<Actor> CombatText::Clone() const
{
    return std::make_unique<CombatText>(*this);
}

void CombatText::Draw(ActorRenderer& renderer) const
{
    renderer.Draw(*this);
}
