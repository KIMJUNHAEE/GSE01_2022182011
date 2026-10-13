#include "stdafx.h"
#include "Monitoring.h"
#include "GameWorld.h"
#include "RenderStats.h"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <utility>

namespace
{
    std::string Utf8(const std::wstring& value)
    {
        if (value.empty())
        {
            return {};
        }
        int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0,
                                       nullptr, nullptr);
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), (int)value.size(), result.data(), size,
                            nullptr, nullptr);
        return result;
    }

    std::string Quote(const std::string& value)
    {
        static const char* hex = "0123456789abcdef";
        std::string result = "\"";
        for (unsigned char c : value)
        {
            if (c == '"' || c == '\\')
            {
                result += '\\';
                result += (char)c;
            }
            else if (c < 32)
            {
                result += "\\u00";
                result += hex[c >> 4];
                result += hex[c & 15];
            }
            else
            {
                result += (char)c;
            }
        }
        return result + "\"";
    }

    std::string Number(double value)
    {
        if (!std::isfinite(value))
        {
            return "null";
        }
        std::ostringstream result;
        result.imbue(std::locale::classic());
        result << std::setprecision(10) << value;
        return result.str();
    }

    class Object
    {
    public:

        std::string fields;

        void Raw(const std::string& key, const std::string& value)
        {
            if (!fields.empty())
            {
                fields += ',';
            }
            fields += Quote(key) + ":" + value;
        }

        void Text(const std::string& key, const std::string& value)
        {
            Raw(key, Quote(value));
        }

        void Text(const std::string& key, const std::wstring& value)
        {
            Text(key, Utf8(value));
        }

        void Num(const std::string& key, double value)
        {
            Raw(key, Number(value));
        }

        void Bool(const std::string& key, bool value)
        {
            Raw(key, value ? "true" : "false");
        }

        std::string Json() const
        {
            return "{" + fields + "}";
        }
    };

    class Array
    {
    public:

        std::string items;

        void Add(const Object& value)
        {
            if (!items.empty())
            {
                items += ',';
            }
            items += value.Json();
        }

        std::string Json() const
        {
            return "[" + items + "]";
        }
    };

    const char* Alignment(VillageAlignment value)
    {
        switch (value)
        {
            case VillageAlignment::Civilian:
                return "civilian";
            case VillageAlignment::Pirate:
                return "pirate";
            case VillageAlignment::Neutral:
                return "neutral";
            default:
                return "unknown";
        }
    }

    const char* ViewName(View value)
    {
        switch (value)
        {
            case View::Title:
                return "title";
            case View::Explore:
                return "explore";
            case View::Dialogue:
                return "dialogue";
            case View::Map:
                return "map";
            case View::Journal:
                return "journal";
            case View::Pause:
                return "pause";
            case View::Ending:
                return "ending";
            case View::Defeat:
                return "defeat";
            default:
                return "unknown";
        }
    }

    Object Entity(const GameWorld& world, const Actor& actor, const std::wstring& name,
                  const std::string& role, const std::string& state)
    {
        Object result, position;
        result.Text("id", std::to_string(actor.Id()));
        result.Text("name", name);
        result.Text("role", role);
        result.Text("state", !actor.IsActive() ? "inactive" : state);
        result.Bool("active", actor.IsActive());
        result.Bool("visible", actor.IsVisible());
        result.Num("villageId", MonitoringVillageAt(world, actor.Position()));
        position.Num("x", actor.Position().x);
        position.Num("y", actor.Position().y);
        result.Raw("position", position.Json());
        return result;
    }
} // namespace

MonitoringState::MonitoringState()
{
    static std::atomic<uint64_t> generation{0};
    const auto ticks = std::chrono::system_clock::now().time_since_epoch();
    sessionId =
        std::to_string(GetCurrentProcessId()) + "-" +
        std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(ticks).count()) + "-" +
        std::to_string(++generation);
    // Designer metadata, not an inferred morality/reputation simulation.
    villages = {{0, L"노마드 착륙장", VillageAlignment::Neutral},
                {1, L"제7 주거구역", VillageAlignment::Civilian},
                {2, L"기억의 정원", VillageAlignment::Neutral},
                {3, L"기록보관소", VillageAlignment::Neutral},
                {4, L"중앙 첨탑", VillageAlignment::Unknown}};
}

void MonitoringState::Record(float gameTime, const std::string& kind, const std::wstring& message,
                             int villageId, ActorId subject, const std::string& severity,
                             bool confirmed)
{
    events.push_back(
        {++totalEvents, gameTime, kind, severity, message, villageId, subject, confirmed});
    while (events.size() > EventCapacity)
    {
        events.pop_front();
    }
}

int MonitoringVillageAt(const GameWorld& world, Vec2 position)
{
    int result = -1;
    for (const auto& district : world.districts)
    {
        if (district.kind >= 0 && district.kind < 5 && position.x >= district.x0 - .5f &&
            position.x <= district.x1 + .5f && position.y >= district.y0 - .5f &&
            position.y <= district.y1 + .5f)
        {
            result = district.kind;
        }
    }
    return result;
}

std::string BuildMonitoringSnapshot(const GameWorld& world, uint64_t sequence)
{
    Object root, counts, player, progress, performance;
    Array npcs, mobs, villages, events;
    std::map<int, int> npcCounts, mobCounts;
    int activeNpcs = 0, totalNpcs = 0, activeMobs = 0, totalMobs = 0;

    for (const auto& character : world.scene.Actors<Character>(true))
    {
        if (!character.captain)
        {
            continue;
        }
        ++totalNpcs;
        if (character.IsActive())
        {
            ++activeNpcs;
            ++npcCounts[MonitoringVillageAt(world, character.Position())];
        }
        Object npc = Entity(world, character, L"동료 선장", "companion",
                            world.captainWalking ? "following" : "idle");
        npc.Raw("health", "null");
        npc.Text("healthTracking", "not_implemented");
        npcs.Add(npc);
    }
    for (const auto& prop : world.scene.Actors<Prop>(true))
    {
        if (prop.kind != PropKind::Citizen)
        {
            continue;
        }
        ++totalNpcs;
        if (prop.IsActive())
        {
            ++activeNpcs;
            ++npcCounts[MonitoringVillageAt(world, prop.Position())];
        }
        Object npc =
            Entity(world, prop, L"주민 #" + std::to_wstring(prop.Id()), "resident",
                   world.view == View::Dialogue && world.nearby == prop.Id() ? "talking" : "idle");
        npc.Raw("health", "null");
        npc.Text("healthTracking", "not_implemented");
        npcs.Add(npc);
    }
    for (const auto& enemy : world.scene.Actors<Enemy>(true))
    {
        if (enemy.health <= 0)
        {
            continue;
        }
        ++totalMobs;
        if (enemy.IsActive())
        {
            ++activeMobs;
            ++mobCounts[MonitoringVillageAt(world, enemy.Position())];
        }
        std::wstring name = enemy.kind == EnemyKind::Boss     ? L"ORACLE"
                            : enemy.kind == EnemyKind::Sentry ? L"경비 포탑"
                                                              : L"경비 드론";
        const char* state = enemy.windup > 0 ? "telegraphing"
                            : GameWorld::Distance(enemy.Position(), enemy.home) > .3f ? "moving"
                                                                                      : "idle";
        Object mob =
            Entity(world, enemy, name, enemy.kind == EnemyKind::Boss ? "boss" : "mob", state);
        mob.Num("health", enemy.health);
        mob.Num("maxHealth", enemy.maxHealth);
        mob.Num("camp", enemy.camp);
        mobs.Add(mob);
    }
    for (const auto& profile : world.monitoring.villages)
    {
        Object village, bounds;
        village.Num("id", profile.id);
        village.Text("name", profile.name);
        village.Text("alignment", Alignment(profile.alignment));
        village.Text("alignmentSource", "designer_metadata");
        const int threats = mobCounts[profile.id];
        const bool relay = profile.id >= 1 && profile.id <= 3 && world.restored[profile.id - 1];
        const char* mood = profile.id == 4 && world.bossActive ? "crisis"
                           : threats > 0                       ? "threatened"
                           : relay || world.bossDefeated       ? "recovering"
                                                               : "quiet";
        village.Text("mood", mood);
        village.Text("moodSource", "derived_from_active_mobs_boss_and_relay");
        village.Num("npcCount", npcCounts[profile.id]);
        village.Num("mobCount", threats);
        village.Raw("relayRestored",
                    profile.id >= 1 && profile.id <= 3 ? (relay ? "true" : "false") : "null");
        bool located = false;
        for (const auto& district : world.districts)
        {
            if (district.kind != profile.id)
            {
                continue;
            }
            bounds.Num("x0", district.x0 - .5f);
            bounds.Num("y0", district.y0 - .5f);
            bounds.Num("x1", district.x1 + .5f);
            bounds.Num("y1", district.y1 + .5f);
            located = true;
            break;
        }
        village.Raw("bounds", located ? bounds.Json() : "null");
        villages.Add(village);
    }
    for (const auto& event : world.monitoring.events)
    {
        Object value;
        value.Text("id", std::to_string(event.id));
        value.Num("gameTime", event.gameTime);
        value.Text("kind", event.kind);
        value.Text("severity", event.severity);
        value.Text("message", event.message);
        value.Num("villageId", event.villageId);
        value.Text("subjectId", std::to_string(event.subject));
        value.Bool("confirmed", event.confirmed);
        value.Text("source", "game");
        events.Add(value);
    }
    const auto stats = RenderStats::Latest();
    performance.Bool("ready", stats.ready);
    performance.Raw("fps", stats.ready ? Number(stats.fps) : "null");
    performance.Raw("averageDrawCalls", stats.ready ? Number(stats.averageDrawCalls) : "null");
    performance.Num("lastDrawCalls", stats.lastDrawCalls);
    performance.Raw("averageInstancesPerDraw",
                    stats.ready ? Number(stats.averageInstancesPerDraw) : "null");
    counts.Num("npcs", activeNpcs);
    counts.Num("npcsIncludingInactive", totalNpcs);
    counts.Num("mobs", activeMobs);
    counts.Num("mobsIncludingInactive", totalMobs);
    counts.Num("projectiles", world.scene.Actors<Projectile>().size());
    counts.Num("loot", world.scene.Actors<Loot>().size());
    counts.Num("actors", world.scene.Size());
    counts.Num("unassignedNpcs", npcCounts[-1]);
    counts.Num("unassignedMobs", mobCounts[-1]);
    player.Num("level", world.stats.level);
    player.Num("health", world.stats.health);
    player.Num("maxHealth", world.stats.MaxHealth());
    player.Num("experience", world.stats.experience);
    player.Num("weaponTier", world.stats.weaponTier);
    player.Num("kills", world.kills);
    player.Num("itemsCollected", world.itemsCollected);
    player.Num("shotsFired", world.shotsFired);
    player.Bool("active", world.PlayerActive());
    Object position;
    // Required level actors are guaranteed by GameWorld's existing contract.
    position.Num("x", world.PlayerPosition().x);
    position.Num("y", world.PlayerPosition().y);
    player.Raw("position", position.Json());
    progress.Num("quest", world.quest);
    progress.Num("restoredRelays", world.RestoredCount());
    progress.Num("memoriesFound", world.MemoryCount());
    progress.Bool("bossActive", world.bossActive);
    progress.Bool("bossDefeated", world.bossDefeated);
    progress.Text("objective", world.ObjectiveText());
    progress.Text("region", world.Region());
    root.Num("schemaVersion", 1);
    root.Text("type", "world_snapshot");
    root.Text("sessionId", world.monitoring.sessionId);
    root.Text("sequence", std::to_string(sequence));
    root.Num("mapSeed", world.mapSeed);
    root.Num("gameTime", world.time);
    root.Text("view", ViewName(world.view));
    root.Raw("counts", counts.Json());
    root.Raw("player", player.Json());
    root.Raw("progress", progress.Json());
    root.Raw("performance", performance.Json());
    root.Raw("villages", villages.Json());
    root.Raw("npcs", npcs.Json());
    root.Raw("mobs", mobs.Json());
    root.Raw("events", events.Json());
    root.Text("eventsTotal", std::to_string(world.monitoring.totalEvents));
    return root.Json();
}
