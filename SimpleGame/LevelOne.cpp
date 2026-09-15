#include "stdafx.h"
#include "GameWorld.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr int NeighborX[] = {1, -1, 0, 0};
    constexpr int NeighborY[] = {0, 0, 1, -1};
} // namespace

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

void GameWorld::BuildNavigation()
{
    edges.fill(0);
    flow.fill(-1);
    flowSource = -1;
    for (int i = 0; i < NavCount; ++i)
    {
        // Navigation is slightly wider than the player and all hostile collision bodies.
        navigation[i] = Walkable(NodePosition(i), .28f) ? 1 : 0;
    }
    for (int i = 0; i < NavCount; ++i)
    {
        if (!navigation[i])
        {
            continue;
        }
        for (int d = 0; d < 4; ++d)
        {
            int x = i % NavWidth + NeighborX[d];
            int y = i / NavWidth + NeighborY[d];
            if (x < 0 || x >= NavWidth || y < 0 || y >= NavHeight)
            {
                continue;
            }
            int next = y * NavWidth + x;
            if (navigation[next] && ClearPath(NodePosition(i), NodePosition(next), .28f))
            {
                edges[i] |= (1 << d);
            }
        }
    }
}

int GameWorld::NearestNode(Vec2 position) const
{
    int cx = (int)std::round((position.x + 10) * 2);
    int cy = (int)std::round((position.y + 17) * 2);
    int result = -1;
    float best = 100000;
    for (int y = (std::max)(0, cy - 2); y <= (std::min)(NavHeight - 1, cy + 2); ++y)
    {
        for (int x = (std::max)(0, cx - 2); x <= (std::min)(NavWidth - 1, cx + 2); ++x)
        {
            int i = y * NavWidth + x;
            float distance = Distance(position, NodePosition(i));
            if (navigation[i] && distance < best && ClearPath(position, NodePosition(i)))
            {
                result = i;
                best = distance;
            }
        }
    }
    return result;
}

bool GameWorld::ValidateNavigation() const
{
    int start = NearestNode(player);
    if (start < 0 || !Walkable(player) || !Walkable(captain, .15f))
    {
        return false;
    }
    std::array<bool, NavCount> visited = {};
    std::queue<int> pending;
    pending.push(start);
    visited[start] = true;
    while (!pending.empty())
    {
        int current = pending.front();
        pending.pop();
        for (int d = 0; d < 4; ++d)
        {
            if (!(edges[current] & (1 << d)))
            {
                continue;
            }
            int next = current + NeighborX[d] + NeighborY[d] * NavWidth;
            if (!visited[next])
            {
                visited[next] = true;
                pending.push(next);
            }
        }
    }
    for (int i = 0; i < NavCount; ++i)
    {
        if (navigation[i] && !visited[i])
        {
            return false;
        }
    }
    // An open floor alone is insufficient: every mandatory terminal needs a reachable approach.
    for (const auto& prop : props)
    {
        if (prop.kind != PropKind::Relay && prop.kind != PropKind::Core &&
            prop.kind != PropKind::Citizen && prop.kind != PropKind::Memory &&
            prop.kind != PropKind::Ship)
        {
            continue;
        }
        bool reachable = false;
        float radius = prop.kind == PropKind::Ship ? 2.6f : 1.5f;
        for (int i = 0; i < NavCount && !reachable; ++i)
        {
            reachable = visited[i] && Distance(NodePosition(i), prop.p) < radius;
        }
        if (!reachable)
        {
            return false;
        }
    }
    int bossNode = NearestNode(bossHome);
    return bossNode >= 0 && visited[bossNode];
}

void GameWorld::GenerateLevel()
{
    auto jitter = [&]()
    {
        return (int)(random() % 3) - 1;
    };
    rooms = {Vec2{-7, 1}, Vec2{3.f + jitter(), 1.f + jitter()},
             Vec2{3.f + jitter(), -12.f + jitter()}, Vec2{16.f + jitter(), jitter() * 1.f},
             Vec2{16.f + jitter(), -13.f + jitter()}};
    auto corridor = [&](Vec2 a, Vec2 b)
    {
        Vec2 corner = random() % 2 ? Vec2{b.x, a.y} : Vec2{a.x, b.y};
        for (const auto& segment : {std::make_pair(a, corner), std::make_pair(corner, b)})
        {
            int x0 = (int)(std::min)(segment.first.x, segment.second.x) - 1;
            int y0 = (int)(std::min)(segment.first.y, segment.second.y) - 1;
            int x1 = (int)(std::max)(segment.first.x, segment.second.x) + 1;
            int y1 = (int)(std::max)(segment.first.y, segment.second.y) + 1;
            districts.push_back({x0, y0, x1, y1, 5});
        }
    };
    corridor(rooms[0], rooms[1]);
    corridor(rooms[1], rooms[2]);
    corridor(rooms[1], rooms[3]);
    corridor(rooms[3], rooms[4]);
    corridor(rooms[2], rooms[4]);
    for (int i = 0; i < 5; ++i)
    {
        int halfWidth = i == 0 ? 3 : 3 + (int)(random() % 2);
        int halfHeight = i == 0 ? 4 : 3;
        districts.push_back({(std::max)(-10, (int)rooms[i].x - halfWidth),
                             (std::max)(-17, (int)rooms[i].y - halfHeight),
                             (std::min)(20, (int)rooms[i].x + halfWidth),
                             (std::min)(5, (int)rooms[i].y + halfHeight), i});
    }
    player = {-5, 1};
    captain = {-5.5f, 2};
    shipPosition = {-8, 0};
    residentPosition = rooms[1] + Vec2{-1.5f, 1.5f};
    bossHome = rooms[4];
    corePosition = rooms[4] + Vec2{-2, 1.5f};
    props = {{PropKind::Ship, shipPosition, 3, 2.5f, 78},
             {PropKind::Citizen, residentPosition, .3f, .3f, 58, 0, false},
             {PropKind::Core, corePosition, .8f, .8f, 54}};
    for (int i = 0; i < 3; ++i)
    {
        relays[i] = rooms[i + 1] + Vec2{1.5f, -1.5f};
        memories[i] = rooms[i + 1] + Vec2{-2, -2};
        props.push_back({PropKind::Relay, relays[i], .55f, .55f, 75, i});
        props.push_back({PropKind::Memory, memories[i], .1f, .1f, 15, i, false});
    }
    BuildNavigation();
    if (!ValidateNavigation())
    {
        throw std::runtime_error("Level 1 base layout has no connected route");
    }
    // Reject individual obstacles that isolate even one navigation pocket.
    for (int attempt = 0; attempt < 32; ++attempt)
    {
        int room = 1 + (int)(random() % 3);
        Vec2 p = rooms[room] +
                 Vec2{(int)(random() % 11) * .5f - 2.5f, (int)(random() % 11) * .5f - 2.5f};
        bool reserved = Distance(p, rooms[room]) < 1.6f;
        for (const auto& prop : props)
        {
            reserved = reserved || Distance(p, prop.p) < 2.f;
        }
        if (reserved || !Walkable(p, .8f))
        {
            continue;
        }
        PropKind kind =
            room == 2 ? PropKind::Tree : (room == 3 ? PropKind::Archive : PropKind::Habitat);
        float width = kind == PropKind::Tree ? .85f : 1.4f;
        props.push_back({kind, p, width, width, 65.f + (random() % 65), attempt % 3});
        BuildNavigation();
        if (!ValidateNavigation())
        {
            props.pop_back();
            BuildNavigation();
        }
    }
    for (int i = 0; i < 5; ++i)
    {
        props.push_back(
            {PropKind::Lamp, rooms[i] + Vec2{-2.5f, 2.5f}, .12f, .12f, 60, i % 2, false});
    }
    BuildFlow();
    for (int i = 0; i < 3; ++i)
    {
        SpawnCamp(i);
    }
}

void GameWorld::BuildFlow()
{
    int source = NearestNode(player);
    if (source == flowSource)
    {
        return;
    }
    flowSource = source;
    flow.fill(-1);
    if (source < 0)
    {
        return;
    }
    std::queue<int> pending;
    pending.push(source);
    flow[source] = 0;
    while (!pending.empty())
    {
        int current = pending.front();
        pending.pop();
        for (int d = 0; d < 4; ++d)
        {
            if (!(edges[current] & (1 << d)))
            {
                continue;
            }
            int next = current + NeighborX[d] + NeighborY[d] * NavWidth;
            if (flow[next] < 0)
            {
                flow[next] = flow[current] + 1;
                pending.push(next);
            }
        }
    }
}

Vec2 GameWorld::FlowTarget(Vec2 position) const
{
    if (ClearPath(position, player))
    {
        return player;
    }
    int current = NearestNode(position);
    if (current < 0 || flow[current] < 0)
    {
        return position;
    }
    int best = current;
    for (int d = 0; d < 4; ++d)
    {
        if (!(edges[current] & (1 << d)))
        {
            continue;
        }
        int next = current + NeighborX[d] + NeighborY[d] * NavWidth;
        if (flow[next] >= 0 && flow[next] < flow[best] && ClearPath(position, NodePosition(next)))
        {
            best = next;
        }
    }
    return NodePosition(best);
}

void GameWorld::MoveActor(Vec2& position, Vec2 target, float speed, float dt)
{
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
}

void GameWorld::SpawnCamp(int camp)
{
    if (enemies.size() >= 18)
    {
        return;
    }
    std::vector<Vec2> candidates;
    for (int i = 0; i < NavCount; ++i)
    {
        Vec2 p = NodePosition(i);
        if (navigation[i] && Distance(p, rooms[camp + 1]) < 2.8f &&
            Distance(Project(p), Project(player)) > 240)
        {
            candidates.push_back(p);
        }
    }
    std::shuffle(candidates.begin(), candidates.end(), random);
    int spawned = 0;
    for (Vec2 p : candidates)
    {
        bool occupied = false;
        for (const auto& enemy : enemies)
        {
            occupied = occupied || Distance(enemy.p, p) < .8f;
        }
        if (occupied)
        {
            continue;
        }
        Enemy enemy;
        enemy.kind = spawned % 3 == 2 ? EnemyKind::Sentry : EnemyKind::Drone;
        enemy.p = enemy.home = p;
        enemy.camp = camp;
        enemy.health = enemy.maxHealth = enemy.kind == EnemyKind::Sentry ? 48.f : 32.f;
        enemy.cooldown = 1.f + spawned * .25f;
        enemies.push_back(enemy);
        if (++spawned >= 4 + (camp > 0 ? 1 : 0))
        {
            break;
        }
    }
}

bool GameWorld::CampClear(int camp) const
{
    for (const auto& enemy : enemies)
    {
        if (enemy.kind != EnemyKind::Boss && enemy.camp == camp && enemy.health > 0)
        {
            return false;
        }
    }
    return true;
}
