#include "stdafx.h"
#include "GameWorld.h"
#include <algorithm>
#include <cmath>

void GameWorld::EnsureCollision() const
{
    if (collisionRevision == scene.NavigationRevision())
    {
        return;
    }
    floorTiles.clear();
    obstacles.clear();
    for (const auto& tile : scene.Actors<Terrain>())
    {
        Vec2 p = tile.Position();
        floorTiles[{(int)std::round(p.x), (int)std::round(p.y)}] = tile.kind;
    }
    for (const auto& prop : scene.Actors<Prop>())
    {
        if (prop.BlocksNavigation())
        {
            obstacles.push_back({prop.Position(), prop.Width(), prop.Depth()});
        }
    }
    collisionRevision = scene.NavigationRevision();
}

int GameWorld::Tile(int x, int y) const
{
    EnsureCollision();
    auto found = floorTiles.find({x, y});
    return found == floorTiles.end() ? -1 : found->second;
}

bool GameWorld::Walkable(Vec2 p, float radius) const
{
    EnsureCollision();
    for (float dx : {-radius, radius})
    {
        for (float dy : {-radius, radius})
        {
            if (Tile((int)std::floor(p.x + dx + .5f), (int)std::floor(p.y + dy + .5f)) < 0)
            {
                return false;
            }
        }
    }
    for (const auto& obstacle : obstacles)
    {
        if (std::abs(p.x - obstacle.position.x) < obstacle.width * .5f + radius &&
            std::abs(p.y - obstacle.position.y) < obstacle.depth * .5f + radius)
        {
            return false;
        }
    }
    return true;
}

bool GameWorld::ClearPath(Vec2 from, Vec2 to, float radius) const
{
    int steps = (std::max)(1, (int)std::ceil(Distance(from, to) / .12f));
    for (int i = 0; i <= steps; ++i)
    {
        if (!Walkable(from + (to - from) * ((float)i / steps), radius))
        {
            return false;
        }
    }
    return true;
}

void GameWorld::BuildNavigation() const
{
    navigation.Rebuild(
        [this](Vec2 position, float radius)
        {
            return Walkable(position, radius);
        });
    navigationRevision = scene.NavigationRevision();
}

int GameWorld::NearestNode(Vec2 position) const
{
    EnsureNavigation();
    return navigation.Nearest(position,
                              [this](Vec2 a, Vec2 b, float radius)
                              {
                                  return ClearPath(a, b, radius);
                              });
}

bool GameWorld::ValidateNavigation() const
{
    EnsureNavigation();
    if (!Walkable(PlayerPosition()) || !Walkable(CaptainPosition(), .15f) ||
        !navigation.Connected(PlayerPosition(),
                              [this](Vec2 a, Vec2 b, float radius)
                              {
                                  return ClearPath(a, b, radius);
                              }))
    {
        return false;
    }
    for (const auto& prop : scene.Actors<Prop>())
    {
        if (prop.kind != PropKind::Relay && prop.kind != PropKind::Core &&
            prop.kind != PropKind::Citizen && prop.kind != PropKind::Memory &&
            prop.kind != PropKind::Ship)
        {
            continue;
        }
        bool reachable = false;
        float radius = prop.kind == PropKind::Ship ? 2.6f : 1.5f;
        for (int i = 0; i < navigation.Count() && !reachable; ++i)
        {
            reachable =
                navigation.IsOpen(i) && Distance(navigation.Position(i), prop.Position()) < radius;
        }
        if (!reachable)
        {
            return false;
        }
    }
    return NearestNode(bossHome) >= 0;
}

void GameWorld::EnsureNavigation() const
{
    if (navigationRevision != scene.NavigationRevision())
    {
        BuildNavigation();
    }
}

void GameWorld::BuildFlow()
{
    EnsureNavigation();
    navigation.BuildFlow(PlayerPosition(),
                         [this](Vec2 a, Vec2 b, float radius)
                         {
                             return ClearPath(a, b, radius);
                         });
}

Vec2 GameWorld::FlowTarget(Vec2 position) const
{
    return navigation.NextTarget(position, PlayerPosition(),
                                 [this](Vec2 a, Vec2 b, float radius)
                                 {
                                     return ClearPath(a, b, radius);
                                 });
}

void GameWorld::MoveActor(Actor& actor, Vec2 target, float speed, float dt)
{
    Vec2 position = actor.Position();
    Vec2 direction = Project(target) - Project(position);
    float distance = Distance(direction, {});
    if (distance < .01f)
    {
        return;
    }
    Vec2 delta = Unproject(direction * ((std::min)(distance, speed * dt) / distance));
    int steps = (std::max)(1, (int)std::ceil(Distance(delta, {}) / .06f));
    delta = delta * (1.f / steps);
    for (int i = 0; i < steps; ++i)
    {
        if (Walkable(position + delta))
        {
            position = position + delta;
        }
    }
    actor.SetPosition(position);
}
