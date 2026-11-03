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
    obstacleBuckets.clear();
    auto terrain = scene.Actors<Terrain>();
    floorWidth = floorHeight = 0;
    int x1 = 0, y1 = 0;
    bool first = true;
    for (const auto& tile : terrain)
    {
        Vec2 p = tile.Position();
        int x = (int)std::round(p.x), y = (int)std::round(p.y);
        floorX = first ? x : (std::min)(floorX, x);
        floorY = first ? y : (std::min)(floorY, y);
        x1 = first ? x + tile.width : (std::max)(x1, x + tile.width);
        y1 = first ? y + tile.depth : (std::max)(y1, y + tile.depth);
        first = false;
    }
    if (!first)
    {
        floorWidth = x1 - floorX;
        floorHeight = y1 - floorY;
        floorTiles.assign(floorWidth * floorHeight, -1);
        for (const auto& tile : terrain)
        {
            int x = (int)std::round(tile.Position().x) - floorX;
            int y = (int)std::round(tile.Position().y) - floorY;
            for (int dy = 0; dy < tile.depth; ++dy)
            {
                for (int dx = 0; dx < tile.width; ++dx)
                {
                    floorTiles[(y + dy) * floorWidth + x + dx] = tile.kind;
                }
            }
        }
    }
    for (const auto& prop : scene.Actors<Prop>())
    {
        if (prop.BlocksNavigation())
        {
            obstacles.push_back({prop.Position(), prop.Width(), prop.Depth()});
            Vec2 p = prop.Position();
            for (int y = (int)std::floor((p.y - prop.Depth() * .5f) / 8);
                 y <= (int)std::floor((p.y + prop.Depth() * .5f) / 8); ++y)
            {
                for (int x = (int)std::floor((p.x - prop.Width() * .5f) / 8);
                     x <= (int)std::floor((p.x + prop.Width() * .5f) / 8); ++x)
                {
                    obstacleBuckets[{x, y}].push_back(obstacles.size() - 1);
                }
            }
        }
    }
    collisionRevision = scene.NavigationRevision();
}

int GameWorld::Tile(int x, int y) const
{
    EnsureCollision();
    x -= floorX;
    y -= floorY;
    return x < 0 || y < 0 || x >= floorWidth || y >= floorHeight ? -1
                                                                 : floorTiles[y * floorWidth + x];
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
    for (int y = (int)std::floor((p.y - radius) / 8); y <= (int)std::floor((p.y + radius) / 8); ++y)
    {
        for (int x = (int)std::floor((p.x - radius) / 8); x <= (int)std::floor((p.x + radius) / 8);
             ++x)
        {
            auto bucket = obstacleBuckets.find({x, y});
            if (bucket == obstacleBuckets.end())
            {
                continue;
            }
            for (size_t index : bucket->second)
            {
                const auto& obstacle = obstacles[index];
                if (std::abs(p.x - obstacle.position.x) < obstacle.width * .5f + radius &&
                    std::abs(p.y - obstacle.position.y) < obstacle.depth * .5f + radius)
                {
                    return false;
                }
            }
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
    EnsureCollision();
    navigation = NavigationGrid({(float)floorX, (float)floorY}, (std::max)(1, floorWidth),
                                (std::max)(1, floorHeight), 1.f);
    navigation.Rebuild(
        [this](Vec2 position, float radius)
        {
            return Walkable(position, radius);
        });
    navigationRevision = scene.NavigationRevision();
}

std::vector<Vec2> GameWorld::FindRoute(Vec2 from, Vec2 to) const
{
    EnsureNavigation();
    return navigation.FindPath(from, to,
                               [this](Vec2 a, Vec2 b, float radius)
                               {
                                   return ClearPath(a, b, radius);
                               });
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
        // Match interaction range; a one-tile grid can place the closest node 1.58 tiles away
        // from a half-tile relay, even though its collision-tested approach is reachable.
        float radius = prop.kind == PropKind::Ship ? 2.6f : 1.8f;
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
