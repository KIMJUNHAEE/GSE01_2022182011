#include "stdafx.h"
#include "GameWorld.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    struct VillageScenery
    {
        PropKind kind;
        Vec2 offset;
        float width, depth, height;
        const char* binding;
        bool solid;
    };

    // Keep the center, residential entrances and the ship's arrival area clear.
    const VillageScenery scenery[] = {
        {PropKind::Habitat, {-5.5f, -5.5f}, 2.4f, 2.2f, 70, "Environment.Workshop", true},
        {PropKind::Archive, {5.5f, -5.5f}, 2.4f, 2.2f, 90, "Prop.Archive", true},
        {PropKind::Habitat, {0, -11}, 1.4f, 1.4f, 140, "Environment.ControlSpire", true},
        {PropKind::Habitat, {0, 15.5f}, 3, .8f, 110, "Environment.Checkpoint", false},
        {PropKind::Crate, {5.5f, 0}, .85f, .8f, 34, "Prop.Crate", true},
        {PropKind::Crate, {6.5f, .7f}, .68f, .7f, 36, "Environment.Barrel", true},
        {PropKind::Crate, {-2, 5.5f}, 1.35f, .49f, 45, "Environment.Bench", false},
        {PropKind::Tree, {2, 5.5f}, 1.2f, .65f, 42, "Environment.Planter", false},
        {PropKind::Tree, {0, -14}, 1.1f, .89f, 92, "Environment.Antenna", true},
    };
} // namespace

void GameWorld::GeneratePlanet()
{
    // Isometric grid axes have equal projected lengths. Size roads using level-one walking speed.
    const float gridUnit = Distance(Project({1, 0}), {});
    const int spacing =
        (int)std::round(PlayerStats{}.MoveSpeed() * PlanetRules::TravelSeconds / gridUnit);
    auto jitter = [&]()
    {
        return (int)(random() % 5) - 2;
    };
    rooms = {Vec2{-7, 1}, Vec2{-7.f + spacing + jitter(), 1.f + jitter()},
             Vec2{-7.f + spacing + jitter(), 1.f - spacing + jitter()},
             Vec2{-7.f + spacing * 2 + jitter(), 1.f + jitter()},
             Vec2{-7.f + spacing * 2 + jitter(), 1.f - spacing + jitter()}};
    int x0 = -30, y0 = -125, x1 = spacing * 2 + 17, y1 = 24;
    districts.push_back({x0, y0, x1, y1, 6});
    const std::pair<int, int> links[] = {{0, 1}, {1, 2}, {1, 3}, {3, 4}, {2, 4}};
    for (const auto& link : links)
    {
        Vec2 a = rooms[link.first], b = rooms[link.second];
        Vec2 corner = {b.x, a.y};
        for (const auto& segment : {std::make_pair(a, corner), std::make_pair(corner, b)})
        {
            districts.push_back({(int)(std::min)(segment.first.x, segment.second.x) - 2,
                                 (int)(std::min)(segment.first.y, segment.second.y) - 2,
                                 (int)(std::max)(segment.first.x, segment.second.x) + 2,
                                 (int)(std::max)(segment.first.y, segment.second.y) + 2, 5});
        }
        float length =
            Distance(Project(corner) - Project(a), {}) + Distance(Project(b) - Project(corner), {});
        planet.roads.push_back({link.first, link.second, length / PlayerStats{}.MoveSpeed()});
    }
    for (int i = 0; i < PlanetRules::VillageCount; ++i)
    {
        const int radius = PlanetRules::VillageRadius;
        districts.push_back({(int)rooms[i].x - radius, (int)rooms[i].y - radius,
                             (int)rooms[i].x + radius, (int)rooms[i].y + radius, i});
        planet.villages[i].id = i;
        planet.villages[i].center = rooms[i];
    }
    auto tileKind = [&](int x, int y)
    {
        int kind = 6;
        for (const auto& district : districts)
        {
            if (x >= district.x0 && x <= district.x1 && y >= district.y0 && y <= district.y1)
            {
                kind = district.kind;
            }
        }
        return kind;
    };
    // Uniform 8x8 patches retain Actor ownership without creating ~37,000 individual nodes.
    for (int y = y0; y <= y1; y += 8)
    {
        for (int x = x0; x <= x1; x += 8)
        {
            int width = (std::min)(8, x1 - x + 1), depth = (std::min)(8, y1 - y + 1);
            const int kind = tileKind(x, y);
            bool uniform = true;
            for (int dy = 0; dy < depth; ++dy)
            {
                for (int dx = 0; dx < width; ++dx)
                {
                    uniform = uniform && tileKind(x + dx, y + dy) == kind;
                }
            }
            if (uniform)
            {
                Spawn(Terrain({(float)x, (float)y}, kind, width, depth));
                continue;
            }
            for (int dy = 0; dy < depth; ++dy)
            {
                for (int dx = 0; dx < width;)
                {
                    int run = 1, tile = tileKind(x + dx, y + dy);
                    while (dx + run < width && tileKind(x + dx + run, y + dy) == tile)
                    {
                        ++run;
                    }
                    Spawn(Terrain({(float)(x + dx), (float)(y + dy)}, tile, run, 1));
                    dx += run;
                }
            }
        }
    }
    for (int y = y0 + 4; y < y1 - 4; y += 7)
    {
        for (int x = x0 + 4; x < x1 - 4; x += 7)
        {
            if (tileKind(x, y) == 6 && random() % 3 == 0)
            {
                Spawn(Prop(PropKind::Tree, {(float)x, (float)y}, .8f, .8f, 45.f + random() % 55, 0,
                           false));
            }
        }
    }
    SetPlayerPosition({-5, 1});
    SetCaptainPosition({-5.5f, 2});
}

void GameWorld::GenerateVillagers()
{
    const float slots[] = {-13, -8, -3, 3, 8, 13};
    const wchar_t* names[] = {L"루나", L"유진", L"미라", L"노아", L"리안",
                              L"테오", L"아린", L"레온", L"나린", L"이안"};
    for (int village = 0; village < PlanetRules::VillageCount; ++village)
    {
        int number = 0;
        for (float y : slots)
        {
            for (float x : slots)
            {
                Vec2 position = rooms[village] + Vec2{x, y};
                ActorId house = Spawn(Prop(PropKind::Habitat, position, 1.8f, 1.6f,
                                           58.f + random() % 28, number % 3))
                                    .Id();
                NpcHome home;
                home.actor = house;
                home.village = village;
                home.entrance = position + Vec2{0, 1.5f};
                if (number < PlanetRules::ResidentsPerVillage)
                {
                    bool story = village == 1 && number == 0;
                    Vec2 spawn = story ? rooms[village] + Vec2{-1.5f, 1.5f} : home.entrance;
                    ActorId citizen = Spawn(Prop(PropKind::Citizen, spawn, .3f, .3f, 58,
                                                 story ? 0 : number % 3 + 1, false))
                                          .Id();
                    ResidentProfile resident;
                    resident.actor = citizen;
                    resident.name = story ? L"세라"
                                          : std::wstring(names[number % 10]) + L"-" +
                                                std::to_wstring(village + 1) + L"-" +
                                                std::to_wstring(number + 1);
                    resident.village = village;
                    resident.home = house;
                    resident.disposition = (NpcDisposition)(number % 3);
                    resident.courier = number == 1;
                    resident.storyResident = story;
                    resident.actionTimer = resident.courier ? 3.f + village * 3 : 8.f + number;
                    resident.decisionTimer = 15.f + number * 2;
                    home.occupant = citizen;
                    planet.residents.emplace(citizen, resident);
                    if (story)
                    {
                        residentId = citizen;
                    }
                }
                planet.homes.emplace(house, home);
                ++number;
            }
        }
        Spawn(
            Prop(PropKind::Lamp, rooms[village] + Vec2{-1, 0}, .12f, .12f, 60, village % 2, false));
        for (const auto& item : scenery)
        {
            Prop prop(item.kind, rooms[village] + item.offset, item.width, item.depth, item.height,
                      0, item.solid);
            prop.modelBinding = item.binding;
            Spawn(prop);
        }
    }
}

bool GameWorld::ValidatePlanet() const
{
    if (planet.residents.size() != PlanetRules::VillageCount * PlanetRules::ResidentsPerVillage ||
        planet.homes.size() != PlanetRules::VillageCount * PlanetRules::HomesPerVillage)
    {
        return false;
    }
    for (const auto& village : planet.villages)
    {
        if (!Walkable(village.center) || NearestNode(village.center) < 0)
        {
            return false;
        }
    }
    for (const auto& entry : planet.homes)
    {
        const auto& home = entry.second;
        if (!scene.Find(home.actor) || !Walkable(home.entrance) || NearestNode(home.entrance) < 0)
        {
            return false;
        }
        if (home.occupant != InvalidActor)
        {
            auto resident = planet.residents.find(home.occupant);
            if (resident == planet.residents.end() || resident->second.home != home.actor ||
                resident->second.village != home.village)
            {
                return false;
            }
        }
        if (home.reservedBy != InvalidActor)
        {
            auto resident = planet.residents.find(home.reservedBy);
            if (home.occupant || resident == planet.residents.end() ||
                resident->second.activity != ResidentActivity::Migrating ||
                resident->second.reservedHome != home.actor)
            {
                return false;
            }
        }
    }
    for (const auto& entry : planet.residents)
    {
        const auto& resident = entry.second;
        auto home = planet.homes.find(resident.home);
        if (!scene.Find(resident.actor) || home == planet.homes.end() ||
            home->second.occupant != resident.actor)
        {
            return false;
        }
        if (resident.reservedHome)
        {
            auto reserved = planet.homes.find(resident.reservedHome);
            if (reserved == planet.homes.end() || reserved->second.reservedBy != resident.actor)
            {
                return false;
            }
        }
    }
    for (const auto& road : planet.roads)
    {
        if (road.walkingSeconds < 27 || road.walkingSeconds > 33)
        {
            return false;
        }
    }
    return true;
}
