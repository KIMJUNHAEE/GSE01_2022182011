#include "stdafx.h"
#include "GameWorld.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

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
    // The tile actors, not district rectangles, are the runtime floor representation.
    for (int y = -17; y <= 5; ++y)
    {
        for (int x = -10; x <= 20; ++x)
        {
            int kind = -1;
            for (const auto& district : districts)
            {
                if (x >= district.x0 && x <= district.x1 && y >= district.y0 && y <= district.y1)
                {
                    kind = district.kind;
                }
            }
            if (kind >= 0)
            {
                Spawn(Terrain({(float)x, (float)y}, kind));
            }
        }
    }
    SetPlayerPosition({-5, 1});
    SetCaptainPosition({-5.5f, 2});
    bossHome = rooms[4];
    shipId = Spawn(Prop(PropKind::Ship, {-8, 0}, 3, 2.5f, 78)).Id();
    residentId =
        Spawn(Prop(PropKind::Citizen, rooms[1] + Vec2{-1.5f, 1.5f}, .3f, .3f, 58, 0, false)).Id();
    coreId = Spawn(Prop(PropKind::Core, rooms[4] + Vec2{-2, 1.5f}, .8f, .8f, 54)).Id();
    scene.Add(SceneEffect(EffectKind::LandingPad), shipId);
    for (int i = 0; i < 3; ++i)
    {
        relays[i] =
            Spawn(Prop(PropKind::Relay, rooms[i + 1] + Vec2{1.5f, -1.5f}, .55f, .55f, 75, i)).Id();
        memories[i] =
            Spawn(Prop(PropKind::Memory, rooms[i + 1] + Vec2{-2, -2}, .1f, .1f, 15, i, false)).Id();
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
        for (const auto& prop : scene.Actors<Prop>())
        {
            reserved = reserved || Distance(p, prop.Position()) < 2.f;
        }
        if (reserved || !Walkable(p, .8f))
        {
            continue;
        }
        PropKind kind =
            room == 2 ? PropKind::Tree : (room == 3 ? PropKind::Archive : PropKind::Habitat);
        float width = kind == PropKind::Tree ? .85f : 1.4f;
        ActorId obstacle =
            Spawn(Prop{kind, p, width, width, 65.f + (random() % 65), attempt % 3}).Id();
        BuildNavigation();
        if (!ValidateNavigation())
        {
            scene.Destroy(obstacle);
            scene.CollectDestroyed();
            BuildNavigation();
        }
    }
    for (int i = 0; i < 5; ++i)
    {
        Spawn(Prop{PropKind::Lamp, rooms[i] + Vec2{-2.5f, 2.5f}, .12f, .12f, 60, i % 2, false});
    }
    BuildFlow();
    for (int i = 0; i < 3; ++i)
    {
        SpawnCamp(i);
    }
}
