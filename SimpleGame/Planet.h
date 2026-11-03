#pragma once
#include "Monitoring.h"
#include <array>
#include <map>

namespace PlanetRules
{
    constexpr int VillageCount = 5;
    constexpr int ResidentsPerVillage = 30;
    constexpr int MinimumResidents = 24;
    constexpr int HomesPerVillage = 36;
    constexpr int VillageRadius = 17;
    constexpr float TravelSeconds = 30.f;
    constexpr float ReportInterval = 20.f;
    constexpr float NewsLifetime = 240.f;
    constexpr float MigrationCooldown = 120.f;
} // namespace PlanetRules

enum class NpcDisposition
{
    Peaceful,
    Ambitious,
    FreeSpirit
};

enum class ResidentActivity
{
    AtHome,
    Strolling,
    Returning,
    Delivering,
    Migrating
};

// Only observations carried by letters or travellers enter another village's knowledge.
struct VillageNews
{
    uint64_t revision = 0;
    int village = -1;
    float observedAt = 0;
    float safety = 0, prosperity = 0, freedom = 0;
    VillageAlignment alignment = VillageAlignment::Unknown;
    bool rumor = false;
    bool confirmed = true;
};

using NewsBundle = std::array<VillageNews, PlanetRules::VillageCount>;

struct VillageState
{
    int id = -1;
    Vec2 center;
    float safety = 0, prosperity = 0, freedom = 0;
    NewsBundle bulletin;
};

struct NpcHome
{
    ActorId actor = InvalidActor;
    int village = -1;
    Vec2 entrance;
    ActorId occupant = InvalidActor;
    ActorId reservedBy = InvalidActor;
};

struct ResidentProfile
{
    ActorId actor = InvalidActor;
    std::wstring name;
    int village = -1;
    ActorId home = InvalidActor;
    NpcDisposition disposition = NpcDisposition::Peaceful;
    ResidentActivity activity = ResidentActivity::AtHome;
    bool courier = false, storyResident = false, moving = false;
    int destinationVillage = -1, deliveries = 0, migrations = 0;
    ActorId reservedHome = InvalidActor;
    float actionTimer = 0, decisionTimer = 0, migrationCooldown = 0, stalledTime = 0;
    std::vector<Vec2> route;
    size_t waypoint = 0;
    NewsBundle knowledge;
    NewsBundle carriedNews;
};

struct VillageRoad
{
    int from, to;
    float walkingSeconds;
};

struct PlanetState
{
    std::array<VillageState, PlanetRules::VillageCount> villages;
    std::map<ActorId, ResidentProfile> residents;
    std::map<ActorId, NpcHome> homes;
    std::vector<VillageRoad> roads;
    float reportTimer = 0, gossipTimer = 0;
    uint64_t newsRevision = 0, lettersDelivered = 0, migrationsCompleted = 0;
};
