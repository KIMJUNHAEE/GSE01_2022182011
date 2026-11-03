#include "stdafx.h"
#include "GameWorld.h"
#include <stdexcept>

void GameWorld::GenerateLevel()
{
    GeneratePlanet();
    bossHome = rooms[4];
    shipId = Spawn(Prop(PropKind::Ship, rooms[0] + Vec2{-7, .5f}, 3, 2.5f, 78)).Id();
    coreId = Spawn(Prop(PropKind::Core, rooms[4] + Vec2{-2, 1.5f}, .8f, .8f, 54)).Id();
    scene.Add(SceneEffect(EffectKind::LandingPad), shipId);
    for (int i = 0; i < 3; ++i)
    {
        relays[i] =
            Spawn(Prop(PropKind::Relay, rooms[i + 1] + Vec2{1.5f, -1.5f}, .55f, .55f, 75, i)).Id();
        memories[i] =
            Spawn(Prop(PropKind::Memory, rooms[i + 1] + Vec2{-2, -2}, .1f, .1f, 15, i, false)).Id();
    }
    GenerateVillagers();
    BuildNavigation();
    if (!ValidateNavigation() || !ValidatePlanet())
    {
        throw std::runtime_error("Planet layout has an unreachable village, home or quest target");
    }
    BuildFlow();
    for (int i = 0; i < 3; ++i)
    {
        SpawnCamp(i);
    }
    RefreshVillageNews();
    for (int i = 0; i < PlanetRules::VillageCount; ++i)
    {
        const NewsBundle local = planet.villages[i].bulletin;
        SpreadNews(i, local, InvalidActor, false);
    }
    planet.reportTimer = PlanetRules::ReportInterval;
}
