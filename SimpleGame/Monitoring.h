#pragma once
#include "Actor.h"
#include <deque>
#include <string>
#include <vector>

class GameWorld;

enum class VillageAlignment
{
    Civilian,
    Pirate,
    Neutral,
    Unknown
};

struct VillageProfile
{
    int id;
    std::wstring name;
    VillageAlignment alignment = VillageAlignment::Unknown;
};

struct WorldEvent
{
    uint64_t id;
    float gameTime;
    std::string kind;
    std::string severity;
    std::wstring message;
    int villageId;
    ActorId subject;
    bool confirmed;
};

// Copyable game state; snapshots only read this on the game thread.
class MonitoringState
{
public:

    MonitoringState();
    std::string sessionId;
    std::vector<VillageProfile> villages;
    std::deque<WorldEvent> events;
    uint64_t totalEvents = 0;
    static constexpr size_t EventCapacity = 256;
    void Record(float gameTime, const std::string& kind, const std::wstring& message,
                int villageId = -1, ActorId subject = InvalidActor,
                const std::string& severity = "info", bool confirmed = true);
};

// JSON is UTF-8; IDs are strings so JavaScript never rounds a uint64_t identifier.
int MonitoringVillageAt(const GameWorld& world, Vec2 position);
std::string BuildMonitoringSnapshot(const GameWorld& world, uint64_t sequence);
