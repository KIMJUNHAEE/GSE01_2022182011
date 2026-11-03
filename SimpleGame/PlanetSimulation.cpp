#include "stdafx.h"
#include "GameWorld.h"
#include <algorithm>
#include <cmath>

namespace
{
    void MergeNews(NewsBundle& destination, const NewsBundle& source)
    {
        for (size_t i = 0; i < source.size(); ++i)
        {
            if (source[i].village == (int)i && source[i].revision > destination[i].revision)
            {
                destination[i] = source[i];
            }
        }
    }

    float Preference(const VillageNews& news, NpcDisposition disposition)
    {
        switch (disposition)
        {
            case NpcDisposition::Peaceful:
                return news.safety * .75f + news.prosperity * .15f + news.freedom * .1f;
            case NpcDisposition::Ambitious:
                return news.safety * .1f + news.prosperity * .8f + news.freedom * .1f;
            default:
                return news.safety * .1f + news.prosperity * .1f + news.freedom * .8f;
        }
    }
} // namespace

void GameWorld::RefreshVillageNews()
{
    const float safety[] = {80, 82, 90, 42, 58};
    const float prosperity[] = {65, 62, 45, 92, 58};
    const float freedom[] = {70, 52, 70, 90, 25};
    std::array<int, PlanetRules::VillageCount> threats{};
    for (const auto& enemy : scene.Actors<Enemy>())
    {
        int village = MonitoringVillageAt(*this, enemy.Position());
        if (village >= 0 && enemy.health > 0)
        {
            ++threats[village];
        }
    }
    for (int i = 0; i < PlanetRules::VillageCount; ++i)
    {
        auto& village = planet.villages[i];
        village.safety =
            std::clamp(safety[i] - threats[i] * 7.f + (bossDefeated ? 10.f : 0.f), 0.f, 100.f);
        village.prosperity = prosperity[i] + (i >= 1 && i <= 3 && restored[i - 1] ? 5.f : 0.f);
        village.freedom = std::clamp(freedom[i] + (quest >= 3 ? 20.f : 0.f), 0.f, 100.f);
        VillageNews report;
        report.revision = ++planet.newsRevision;
        report.village = i;
        report.observedAt = time;
        report.safety = village.safety;
        report.prosperity = village.prosperity;
        report.freedom = village.freedom;
        report.alignment = monitoring.villages[i].alignment;
        // A rumour is an explicitly unconfirmed report, never a fabricated confirmed crime.
        report.rumor = i == 3 && ((int)(time / PlanetRules::ReportInterval) % 3 == 1);
        report.confirmed = !report.rumor;
        if (report.rumor)
        {
            report.safety = (std::max)(0.f, report.safety - 12.f);
            monitoring.Record(time, "security_rumor",
                              L"흑항의 야간 치안이 불안하다는 소문이 돌고 있습니다.", i,
                              InvalidActor, "warning", false);
        }
        village.bulletin[i] = report;
    }
}

void GameWorld::SpreadNews(int village, const NewsBundle& news, ActorId carrier, bool letter)
{
    if (village < 0 || village >= PlanetRules::VillageCount)
    {
        return;
    }
    if (carrier != InvalidActor)
    {
        const Actor* actor = scene.Find(carrier);
        if (!actor || !actor->IsActive() ||
            MonitoringVillageAt(*this, actor->Position()) != village)
        {
            return;
        }
    }
    auto& bulletin = planet.villages[village].bulletin;
    MergeNews(bulletin, news);
    for (auto& entry : planet.residents)
    {
        auto& resident = entry.second;
        const Actor* actor = scene.Find(resident.actor);
        if (actor && actor->IsActive() && MonitoringVillageAt(*this, actor->Position()) == village)
        {
            MergeNews(resident.knowledge, bulletin);
        }
    }
    if (carrier != InvalidActor)
    {
        monitoring.Record(time, letter ? "letter_delivered" : "traveller_news",
                          letter ? L"배달부가 편지와 다른 마을의 소식을 전달했습니다."
                                 : L"여행자가 다른 마을에서 들은 소식을 전했습니다.",
                          village, carrier);
    }
}

bool GameWorld::BeginResidentRoute(ResidentProfile& resident, Vec2 target,
                                   ResidentActivity activity, int destination)
{
    Actor* actor = scene.Find(resident.actor);
    if (!actor || !actor->IsActive())
    {
        return false;
    }
    auto route = FindRoute(actor->Position(), target);
    if (route.empty())
    {
        return false;
    }
    resident.route = std::move(route);
    resident.waypoint = 0;
    resident.stalledTime = 0;
    resident.activity = activity;
    resident.destinationVillage = destination;
    resident.carriedNews = resident.knowledge;
    return true;
}

bool GameWorld::ConsiderMigration(ActorId id)
{
    auto found = planet.residents.find(id);
    if (found == planet.residents.end())
    {
        return false;
    }
    auto& resident = found->second;
    if (resident.courier || resident.storyResident || resident.migrationCooldown > 0 ||
        resident.activity != ResidentActivity::AtHome)
    {
        return false;
    }
    int staying = 0;
    for (const auto& entry : planet.residents)
    {
        staying += entry.second.village == resident.village &&
                   entry.second.activity != ResidentActivity::Migrating;
    }
    if (staying <= PlanetRules::MinimumResidents)
    {
        return false;
    }
    const auto& current = resident.knowledge[resident.village];
    if (!current.revision)
    {
        return false;
    }
    float best = Preference(current, resident.disposition) + 12.f;
    ActorId target = InvalidActor;
    for (const auto& entry : planet.homes)
    {
        const auto& home = entry.second;
        const auto& heard = resident.knowledge[home.village];
        if (home.village == resident.village || home.occupant || home.reservedBy ||
            !heard.revision || time - heard.observedAt > PlanetRules::NewsLifetime)
        {
            continue;
        }
        // Unconfirmed stories influence preferences, but carry less weight than direct news.
        float score = Preference(heard, resident.disposition) - (heard.confirmed ? 0.f : 5.f);
        if (score > best)
        {
            best = score;
            target = home.actor;
        }
    }
    if (!target)
    {
        return false;
    }
    auto& home = planet.homes.at(target);
    if (!BeginResidentRoute(resident, home.entrance, ResidentActivity::Migrating, home.village))
    {
        return false;
    }
    home.reservedBy = resident.actor;
    resident.reservedHome = home.actor;
    monitoring.Record(time, "migration_started",
                      resident.name + L"의 이주 출발: " + monitoring.villages[home.village].name +
                          L" (전달받은 소식과 성향에 따른 결정)",
                      resident.village, resident.actor);
    return true;
}

void GameWorld::CancelResidentRoute(ResidentProfile& resident)
{
    if (resident.reservedHome)
    {
        auto home = planet.homes.find(resident.reservedHome);
        if (home != planet.homes.end() && home->second.reservedBy == resident.actor)
        {
            home->second.reservedBy = InvalidActor;
        }
        resident.reservedHome = InvalidActor;
    }
    resident.route.clear();
    resident.waypoint = 0;
    resident.moving = false;
    resident.activity = ResidentActivity::AtHome;
    resident.destinationVillage = -1;
    resident.actionTimer = 10;
    resident.migrationCooldown = 30;
}

void GameWorld::FinishResidentRoute(ResidentProfile& resident)
{
    const auto activity = resident.activity;
    resident.route.clear();
    resident.waypoint = 0;
    resident.moving = false;
    if (activity == ResidentActivity::Migrating)
    {
        auto destination = planet.homes.find(resident.reservedHome);
        auto previous = planet.homes.find(resident.home);
        if (destination == planet.homes.end() || previous == planet.homes.end() ||
            destination->second.reservedBy != resident.actor || destination->second.occupant)
        {
            CancelResidentRoute(resident);
            return;
        }
        previous->second.occupant = InvalidActor;
        auto& home = destination->second;
        home.occupant = resident.actor;
        home.reservedBy = InvalidActor;
        resident.home = home.actor;
        resident.village = home.village;
        resident.reservedHome = InvalidActor;
        resident.migrationCooldown = PlanetRules::MigrationCooldown;
        ++resident.migrations;
        ++planet.migrationsCompleted;
        SpreadNews(resident.village, resident.carriedNews, resident.actor, false);
        monitoring.Record(time, "migration_completed",
                          resident.name + L"이(가) 새 집에 입주했습니다.", resident.village,
                          resident.actor);
    }
    if (activity == ResidentActivity::Delivering)
    {
        SpreadNews(resident.destinationVillage, resident.carriedNews, resident.actor, true);
        ++resident.deliveries;
        ++planet.lettersDelivered;
        MergeNews(resident.knowledge, planet.villages[resident.destinationVillage].bulletin);
        if (BeginResidentRoute(resident, planet.homes.at(resident.home).entrance,
                               ResidentActivity::Returning, resident.village))
        {
            return;
        }
    }
    else if (activity == ResidentActivity::Returning && resident.courier)
    {
        SpreadNews(resident.village, resident.carriedNews, resident.actor, true);
        ++planet.lettersDelivered;
    }
    else if (activity == ResidentActivity::Strolling)
    {
        MergeNews(resident.knowledge, planet.villages[resident.village].bulletin);
        if (BeginResidentRoute(resident, planet.homes.at(resident.home).entrance,
                               ResidentActivity::Returning, resident.village))
        {
            return;
        }
    }
    resident.activity = ResidentActivity::AtHome;
    resident.destinationVillage = -1;
    resident.actionTimer = 16.f + resident.actor % 20;
}

void GameWorld::UpdatePlanet(float dt)
{
    planet.reportTimer -= dt;
    planet.gossipTimer -= dt;
    if (planet.reportTimer <= 0)
    {
        RefreshVillageNews();
        planet.reportTimer = PlanetRules::ReportInterval;
    }
    if (planet.gossipTimer <= 0)
    {
        for (int i = 0; i < PlanetRules::VillageCount; ++i)
        {
            const NewsBundle local = planet.villages[i].bulletin;
            SpreadNews(i, local, InvalidActor, false);
        }
        planet.gossipTimer = 3;
    }
    for (auto& entry : planet.residents)
    {
        auto& resident = entry.second;
        resident.moving = false;
        Actor* actor = scene.Find(resident.actor);
        if (!actor || !actor->IsActive())
        {
            if (!actor)
            {
                CancelResidentRoute(resident);
                auto home = planet.homes.find(resident.home);
                if (home != planet.homes.end())
                {
                    home->second.occupant = InvalidActor;
                }
            }
            continue;
        }
        resident.migrationCooldown = (std::max)(0.f, resident.migrationCooldown - dt);
        resident.decisionTimer -= dt;
        if (resident.storyResident)
        {
            continue;
        }
        if (!resident.route.empty())
        {
            Vec2 previous = actor->Position();
            // Consume remaining time across corners, so gait speed doesn't depend on waypoint count.
            const float speed = resident.courier ? 155.f : 115.f;
            float budget = dt * speed;
            while (resident.waypoint < resident.route.size() && budget > .001f)
            {
                Vec2 target = resident.route[resident.waypoint];
                float distance = Distance(Project(target) - Project(actor->Position()), {});
                float travel = (std::min)(budget, distance);
                if (distance > .01f)
                {
                    MoveActor(*actor, target, speed, travel / speed);
                }
                budget -= travel;
                if (Distance(Project(target) - Project(actor->Position()), {}) < .1f)
                {
                    ++resident.waypoint;
                }
                else
                {
                    break;
                }
            }
            resident.moving = Distance(previous, actor->Position()) > .0001f;
            resident.stalledTime = resident.moving ? 0.f : resident.stalledTime + dt;
            if (resident.stalledTime > 3)
            {
                Vec2 goal = resident.route.back();
                auto route = FindRoute(actor->Position(), goal);
                if (route.empty())
                {
                    monitoring.Record(time, "npc_route_blocked",
                                      L"주민의 이동 경로가 막혀 재시도합니다.", resident.village,
                                      resident.actor, "warning");
                    CancelResidentRoute(resident);
                }
                else
                {
                    resident.route = std::move(route);
                    resident.waypoint = 0;
                    resident.stalledTime = 0;
                }
            }
            else if (resident.waypoint >= resident.route.size())
            {
                FinishResidentRoute(resident);
            }
            continue;
        }
        if (resident.decisionTimer <= 0)
        {
            resident.decisionTimer = 15.f + resident.actor % 12;
            if (ConsiderMigration(resident.actor))
            {
                continue;
            }
        }
        resident.actionTimer -= dt;
        if (resident.actionTimer > 0)
        {
            continue;
        }
        resident.actionTimer = 15;
        if (resident.courier)
        {
            std::vector<int> neighbors;
            for (const auto& road : planet.roads)
            {
                if (road.from == resident.village)
                {
                    neighbors.push_back(road.to);
                }
                if (road.to == resident.village)
                {
                    neighbors.push_back(road.from);
                }
            }
            if (!neighbors.empty())
            {
                int destination = neighbors[resident.deliveries % neighbors.size()];
                if (BeginResidentRoute(resident, rooms[destination], ResidentActivity::Delivering,
                                       destination))
                {
                    monitoring.Record(time, "letter_dispatched",
                                      L"배달부가 이웃 마을로 편지를 운반합니다.", resident.village,
                                      resident.actor);
                }
            }
        }
        else
        {
            BeginResidentRoute(resident, rooms[resident.village] + Vec2{0, 1},
                               ResidentActivity::Strolling, resident.village);
        }
    }
}
