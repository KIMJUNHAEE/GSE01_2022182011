#include "stdafx.h"
#include "GameWorld.h"
#include <algorithm>
#include <cmath>

float GameWorld::Distance(Vec2 a, Vec2 b)
{
    Vec2 d = a - b;
    return std::sqrt(d.x * d.x + d.y * d.y);
}

GameWorld::GameWorld(uint32_t seed)
{
    mapSeed = seed ? seed : std::random_device{}();
    if (mapSeed == 0)
    {
        mapSeed = 1;
    }
    random.seed(mapSeed);
    roots.terrain = scene.Add(Actor("Terrain")).Id();
    roots.environment = scene.Add(Actor("Environment")).Id();
    roots.characters = scene.Add(Actor("Characters")).Id();
    roots.enemies = scene.Add(Actor("Enemies")).Id();
    roots.projectiles = scene.Add(Actor("Projectiles")).Id();
    roots.loot = scene.Add(Actor("Loot")).Id();
    roots.effects = scene.Add(Actor("Effects")).Id();
    playerId = Spawn(Character({-5, 1})).Id();
    captainId = Spawn(Character({-5.5f, 2}, true)).Id();
    scene.Add(SceneEffect(EffectKind::Weapon), playerId);
    scene.Add(SceneEffect(EffectKind::Magnet), playerId);
    scene.Add(SceneEffect(EffectKind::PlayerRing), playerId);
    Spawn(SceneEffect(EffectKind::AmbientDust));
    scanEffectId = Spawn(SceneEffect(EffectKind::Scan)).Id();
    GenerateLevel();
    monitoring.Record(time, "world_created",
                      L"행성 생성 완료 · 마을 5곳 / 주민 150명 / 주택 180채");
}

void GameWorld::Move(Vec2 direction, float dt, bool running)
{
    walking = false;
    playerMoveSpeed = 0;
    auto* player = scene.Find<Character>(playerId);
    if (!player)
    {
        return;
    }
    player->animation.Advance({});
    if (view != View::Explore || !PlayerActive())
    {
        return;
    }
    float len = Distance(direction, {});
    if (len < .01f)
    {
        return;
    }
    direction = direction * (1 / len);
    const Vec2 previous = PlayerPosition();
    facing = direction;
    float speed = stats.MoveSpeed() * (running ? 1.5f : 1.f);
    Vec2 delta = {(direction.x / 84 + direction.y / 42) * speed * dt,
                  (direction.y / 42 - direction.x / 84) * speed * dt};
    // Small substeps prevent tunnelling, even after a slow frame or while sprinting.
    int steps = (std::max)(1, (int)std::ceil(Distance(delta, {}) / .07f));
    delta = delta * (1.f / steps);
    for (int i = 0; i < steps; ++i)
    {
        Vec2 next = PlayerPosition() + Vec2{delta.x, 0};
        if (Walkable(next))
        {
            SetPlayerPosition(next);
            walking = true;
        }
        next = PlayerPosition() + Vec2{0, delta.y};
        if (Walkable(next))
        {
            SetPlayerPosition(next);
            walking = true;
        }
    }
    const Vec2 traveled = Project(PlayerPosition()) - Project(previous);
    playerMoveSpeed = dt > 0 ? Distance(traveled, {}) / dt : 0;
    player->animation.Advance(traveled);
}

void GameWorld::Start()
{
    view = View::Explore;
    if (!runStarted)
    {
        runStarted = true;
        monitoring.Record(time, "exploration_started", L"개척자가 탐사를 시작했습니다.", 0);
        quest = 1;
        Notify(L"엘리시움 탐사 · 마을 5곳과 주민 150명. M으로 지도와 다음 정착지를 확인하세요.");
        trail.clear();
        trail.push_back(CaptainPosition());
        trail.push_back(PlayerPosition());
    }
}

void GameWorld::Update(float dt)
{
    if (view != View::Explore)
    {
        for (auto& entry : planet.residents)
        {
            entry.second.moving = false;
        }
    }
    if (view == View::Pause || view == View::Map || view == View::Journal || view == View::Ending ||
        view == View::Defeat)
    {
        firing = false;
        return;
    }
    time += dt;
    toastTime = (std::max)(0.f, toastTime - dt);
    if (view != View::Explore)
    {
        firing = false;
        walking = captainWalking = false;
        return;
    }
    scanCooldown = (std::max)(0.f, scanCooldown - dt);
    if (scanWave >= 0)
    {
        scanWave += dt;
        if (scanWave > 2.3f)
        {
            scanWave = -1;
        }
    }
    UpdateCompanion(dt);
    UpdateCombat(dt);
    if (view == View::Explore)
    {
        UpdatePlanet(dt);
    }
    // Visual child actors sample the final NPC/enemy positions of this frame, not the previous one.
    scene.Update(dt);
    scene.CollectDestroyed();
    UpdateNearby();
}

void GameWorld::UpdateCompanion(float dt)
{
    captainWalking = false;
    const auto* captain = scene.Find(captainId);
    if (dt <= 0 || !PlayerActive() || !captain || !captain->IsActive())
    {
        captainFollowSpeed = 0;
        captainFollowing = false;
        return;
    }
    captainRepathCooldown = (std::max)(0.f, captainRepathCooldown - dt);
    if (trail.empty() ||
        (trail.size() < 512 && Distance(Project(trail.back() - PlayerPosition()), {}) > .01f))
    {
        // Keep the collision-tested player path, including its final partial step.
        trail.push_back(PlayerPosition());
    }
    if ((trail.size() >= 512 || captainBlockedTime > .35f) && captainRepathCooldown <= 0)
    {
        const auto route = FindRoute(CaptainPosition(), PlayerPosition());
        if (!route.empty())
        {
            trail.assign(route.begin(), route.end());
        }
        captainRepathCooldown = 1;
        captainBlockedTime = 0;
    }
    float remaining = 0;
    Vec2 previous = CaptainPosition();
    for (Vec2 point : trail)
    {
        remaining += Distance(Project(point - previous), {});
        previous = point;
    }
    // Screen-space units match player movement in every isometric direction.
    // Separate start/stop distances prevent repeated catch-up/stop oscillation.
    if (remaining > 64)
    {
        captainFollowing = true;
    }
    else if (remaining <= 44.1f)
    {
        captainFollowing = false;
    }
    if (!captainFollowing)
    {
        captainFollowSpeed = 0;
        captainBlockedTime = 0;
        return;
    }
    const float leaderSpeed = walking ? playerMoveSpeed : 0;
    const float gap = walking ? 56.f : 44.f;
    float targetSpeed =
        std::clamp(leaderSpeed + (remaining - gap) * 4, 0.f, stats.MoveSpeed() * 1.8f);
    if (!walking)
    {
        targetSpeed = (std::max)(12.f, targetSpeed);
    }
    captainFollowSpeed += (targetSpeed - captainFollowSpeed) * (1 - std::exp(-dt * 10));
    float budget = (std::min)(captainFollowSpeed * dt, (std::max)(0.f, remaining - 44));
    bool blocked = false;
    // Consume reached waypoints AND continue moving with the remaining budget in this frame.
    while (budget > .0001f && !trail.empty())
    {
        const Vec2 position = CaptainPosition(), delta = trail.front() - position;
        const float distance = Distance(delta, {}), pixels = Distance(Project(delta), {});
        if (pixels < .001f)
        {
            trail.pop_front();
            continue;
        }
        // Small, collision-tested substeps cannot jump through a building at low frame rates.
        const float fraction = (std::min)(1.f, (std::min)(budget / pixels, .06f / distance));
        const Vec2 next = position + delta * fraction;
        if (!ClearPath(position, next, .15f))
        {
            blocked = true;
            captainFollowSpeed = 0;
            break;
        }
        SetCaptainPosition(next);
        captainWalking = true;
        budget -= pixels * fraction;
        if (fraction >= 1)
        {
            trail.pop_front();
        }
    }
    captainBlockedTime = blocked ? captainBlockedTime + dt : 0;
}

void GameWorld::UpdateNearby()
{
    nearby = InvalidActor;
    if (!PlayerActive())
    {
        return;
    }
    float best = 1.8f;
    int bestPriority = -1;
    ActorId ship = InvalidActor;
    for (const auto& p : scene.Actors<Prop>())
    {
        if (p.kind != PropKind::Citizen && p.kind != PropKind::Relay &&
            p.kind != PropKind::Memory && p.kind != PropKind::Core && p.kind != PropKind::Ship &&
            planet.homes.find(p.Id()) == planet.homes.end())
        {
            continue;
        }
        if (p.kind == PropKind::Memory && found[p.variant])
        {
            continue;
        }
        float d = Distance(PlayerPosition(), p.Position());
        if (p.kind == PropKind::Ship)
        {
            if (d < 2.7f)
            {
                ship = p.Id();
            }
            continue;
        }
        int priority = p.kind == PropKind::Citizen                       ? 1
                       : planet.homes.find(p.Id()) != planet.homes.end() ? 0
                                                                         : 2;
        if (d < 1.8f && (priority > bestPriority || (priority == bestPriority && d < best)))
        {
            best = d;
            bestPriority = priority;
            nearby = p.Id();
        }
    }
    if (nearby == InvalidActor)
    {
        nearby = ship;
    }
}

void GameWorld::Say(std::initializer_list<DialogueLine> lines)
{
    dialogue = lines;
    view = View::Dialogue;
    firing = false;
    walking = captainWalking = false;
}

void GameWorld::Notify(const std::wstring& text)
{
    toast = text;
    toastTime = 4.5f;
}

int GameWorld::RestoredCount() const
{
    return (int)std::count(restored.begin(), restored.end(), true);
}

int GameWorld::MemoryCount() const
{
    return (int)std::count(found.begin(), found.end(), true);
}

void GameWorld::Interact()
{
    if (view == View::Dialogue)
    {
        Advance();
        return;
    }
    if (view != View::Explore)
    {
        return;
    }
    UpdateNearby();
    if (nearby == InvalidActor)
    {
        Notify(L"가까운 주민이나 장치 앞에서 E를 눌러 상호작용하세요.");
        return;
    }
    const Prop* target = scene.Find<Prop>(nearby);
    if (!target || !target->IsActive())
    {
        return;
    }
    const Prop& p = *target;
    auto house = planet.homes.find(p.Id());
    if (house != planet.homes.end())
    {
        const auto& home = house->second;
        auto owner = planet.residents.find(home.occupant);
        Say({{L"주거 등록부",
              monitoring.villages[home.village].name + L" / " +
                  (owner == planet.residents.end() ? L"빈집 · 이주민 입주 가능"
                                                   : owner->second.name + L"의 집")},
             {L"안내", home.reservedBy ? L"새 입주자가 이곳으로 이동하고 있습니다."
                                       : L"집 앞에서 거주 정보를 확인할 수 있습니다. 실내 입장은 "
                                         L"아직 지원하지 않습니다."}});
    }
    else if (p.kind == PropKind::Citizen)
    {
        monitoring.Record(time, "npc_dialogue", L"주민과 대화를 시작했습니다.",
                          MonitoringVillageAt(*this, p.Position()), p.Id());
        auto resident = planet.residents.find(p.Id());
        if (resident != planet.residents.end() && !resident->second.storyResident)
        {
            const auto& profile = resident->second;
            std::wstring news = L"다른 마을 소식은 배달부의 편지나 다녀온 사람에게서 들어요.";
            for (const auto& heard : profile.knowledge)
            {
                if (heard.revision && heard.village != profile.village &&
                    time - heard.observedAt <= PlanetRules::NewsLifetime)
                {
                    news = monitoring.villages[heard.village].name +
                           (heard.rumor
                                ? L"에서 치안이 불안하다는 소문이 왔어요. 확인된 일인지는 몰라요."
                                : L"의 소식을 전달받았어요. 살 곳을 고를 때 참고하려고요.");
                    break;
                }
            }
            Say({{profile.name,
                  L"제 집은 " + monitoring.villages[profile.village].name + L"에 있어요."},
                 {profile.name, news}});
            return;
        }
        if (p.variant != 0)
        {
            if (quest >= 3)
            {
                Say({{L"주민 · 기록관리인", L"검열된 기록들을 복원하고 있어요. 누군가의 삶이 더는 "
                                            L"빈 페이지로 남지 않게요."}});
            }
            else
            {
                Say({{L"주민 · 정비공",
                      L"고장 난 건 중계기만이 아니에요. 서로 말을 걸지 않는 습관이 더 오래됐죠."},
                     {L"주민 · 정비공",
                      L"광장의 세라가 당신을 기다릴 거예요. 그 사람은 아직 포기하지 않았거든요."}});
            }
            return;
        }
        if (!metResident && quest < 3)
        {
            metResident = true;
            quest = (std::max)(quest, 1);
            Say({{L"세라 · 엘리시움 주민",
                  L"밖에서 온 사람이군요. 이곳에서는 해가 져도 창문을 열 수 없어요."},
                 {L"세라", L"관리자는 안전을 위해서래요. 하지만 돌아오지 않는 가족에게 안부조차 "
                           L"보낼 수 없죠."},
                 {L"선장 · 카엘",
                  L"끊어진 중계기 세 곳을 살리면, 사람들이 서로의 목소리를 들을 수 있을 거야."},
                 {L"기억을 잃은 개척자",
                  L"제 과거는 기억나지 않아요. 그래도… 지금 누구를 돕고 싶은지는 알겠어요."}});
        }
        else if (quest >= 3)
        {
            Say({{L"세라", choice == 0 ? L"처음으로 제 목소리가 도시 전체에 닿았어요. 내일은 "
                                         L"우리가 함께 정해 볼게요."
                                       : L"오늘은 한 구역, 내일은 그다음 구역. 기다리는 동안에도 "
                                         L"서로의 목소리를 들을 수 있어요."},
                 {L"세라", L"떠나도 괜찮아요. 이곳에 남은 일은, 이제 우리 몫이니까."}});
        }
        else
        {
            Say({{L"세라",
                  L"정원, 기록보관소, 주거구역에 중계기가 있어요. M으로 지도를 확인해 보세요."},
                 {L"카엘", L"길을 잃으면 Q로 신호를 탐지해. 서두르지 않아도 돼. 같이 가자."}});
        }
    }
    else if (p.kind == PropKind::Relay)
    {
        if (quest == 0)
        {
            Notify(L"먼저 광장의 주민에게 이 행성의 사정을 물어보세요.");
            return;
        }
        if (restored[p.variant])
        {
            Notify(L"이 중계기는 이미 연결되어 있습니다.");
            return;
        }
        if (!CampClear(p.variant))
        {
            Notify(L"이 구역의 경비 기계를 먼저 처치하세요. 영혼석을 모으면 더 강해집니다.");
            return;
        }
        restored[p.variant] = true;
        monitoring.Record(time, "relay_restored", L"중계기가 복구되었습니다.",
                          MonitoringVillageAt(*this, p.Position()), p.Id());
        GainExperience(60);
        AddLoot(LootKind::Health, PlayerPosition());
        if (auto* scan = scene.Find(scanEffectId))
        {
            scan->SetPosition(p.Position());
        }
        scanWave = 0;
        Notify(L"중계기 복구  " + std::to_wstring(RestoredCount()) +
               L" / 3  ·  주민의 통신이 돌아옵니다.");
        if (RestoredCount() == 3)
        {
            quest = 2;
            Say({{L"도시 통신", L"…들리나요? 저는 남쪽 주거구역에 있어요. 아직 여기 살아 있어요."},
                 {L"카엘",
                  L"저 한마디를 기다린 사람들이 있었겠지. 이제 중앙 관리자를 만나러 가자."},
                 {L"개척자",
                  L"잠깐… 이 신호, 어디선가 들은 것 같아요. 제 손의 문양이 빛나고 있어요."}});
        }
    }
    else if (p.kind == PropKind::Memory)
    {
        found[p.variant] = true;
        monitoring.Record(time, "memory_found", L"기억의 흔적을 발견했습니다.",
                          MonitoringVillageAt(*this, p.Position()), p.Id());
        static const wchar_t* notes[] = {
            L"[항해 기록 01] 이름은 잊어도 괜찮아. 네가 내민 손은 누군가 기억할 테니까.",
            L"[정원의 편지] 엄마가 돌아오면 보여 주려고 심었어. 기다리는 것도 내가 고른 일이야.",
            L"[삭제된 명령] 보호 프로토콜: 주민의 생존을 보장하라. 주민의 선택권을 대체하지 말라."};
        Say({{L"기억의 잔향", notes[p.variant]},
             {L"카엘", L"기록은 항해 일지에 남겨 둘게. 네가 기억하고 싶은 만큼만 기억하자."}});
    }
    else if (p.kind == PropKind::Core)
    {
        if (quest < 2)
        {
            Say({{L"관리자 · ORACLE",
                  L"외부 접근을 제한합니다. 시민 통신망 세 곳의 연결을 먼저 확인하십시오."}});
        }
        else if (quest == 2 && !bossDefeated)
        {
            BeginBoss();
        }
        else if (quest == 2)
        {
            choicePending = true;
            Say({{L"관리자 · ORACLE", L"통신 제한은 시민을 지키기 위한 조치였습니다. 자유로운 "
                                      L"이동에는 위험이 따릅니다."},
                 {L"개척자", L"아무것도 선택할 수 없다면, 안전하다는 말은 누구를 위한 건가요?"},
                 {L"카엘", L"우리가 이곳의 내일을 대신 살아 줄 수는 없어. 주민들에게 어떤 길을 "
                           L"열어 줄까?"}});
        }
        else
        {
            Say({{L"관리자 · ORACLE",
                  choice == 0 ? L"시민 의회 연결 완료. 도시 결정권을 주민에게 반환했습니다."
                              : L"단계적 개방을 시작합니다. 각 구역의 개방 시점은 주민 투표로 "
                                L"결정됩니다."}});
        }
    }
    else if (p.kind == PropKind::Ship)
    {
        if (quest >= 3)
        {
            quest = 4;
            view = View::Ending;
        }
        else
        {
            Say({{L"카엘", L"이 낡은 배의 이름은 노마드야. 쉴 곳이 필요하면 언제든 돌아와."},
                 {L"카엘", L"아직 이곳에서 들을 이야기가 남아 있는 것 같지 않아?"}});
        }
    }
}

void GameWorld::Advance()
{
    if (view != View::Dialogue)
    {
        return;
    }
    if (!dialogue.empty())
    {
        dialogue.pop_front();
    }
    if (dialogue.empty() && !choicePending)
    {
        view = View::Explore;
    }
}

void GameWorld::Choose(int option)
{
    if (!choicePending || !dialogue.empty() || (option != 0 && option != 1))
    {
        return;
    }
    choicePending = false;
    choice = option;
    monitoring.Record(time, "story_choice",
                      option == 0 ? L"주민 자치 개방을 선택했습니다."
                                  : L"주민 주도의 단계적 개방을 선택했습니다.",
                      4);
    quest = 3;
    Say({{L"개척자",
          option == 0
              ? L"문을 열어 주세요. 위험을 함께 논의할 의회도, 주민들 스스로 만들 수 있도록."
              : L"생명 유지 장치는 지켜 주세요. 문을 여는 순서와 속도는 각 구역의 주민이 "
                L"정하도록."},
         {L"카엘", L"모든 문제가 오늘 끝나지는 않겠지. 그래도 이제, 이 사람들의 내일이야."},
         {L"세라 · 통신", L"우리 집 창문을 열었어요. 별이… 이렇게 많았네요."}});
    Notify(L"챕터 목표 완료 · 노마드로 돌아가 다음 여정을 준비하세요.");
}

void GameWorld::Scan()
{
    if (view != View::Explore || !PlayerActive() || scanCooldown > 0)
    {
        return;
    }
    scanCooldown = 5;
    scanWave = 0;
    if (auto* scan = scene.Find(scanEffectId))
    {
        scan->SetPosition(PlayerPosition());
    }
    Notify(L"공명 탐지 · 금빛 표식을 따라가세요. 지도에는 남은 신호가 표시됩니다.");
}

Vec2 GameWorld::Objective() const
{
    if (quest == 0)
    {
        return ResidentPosition();
    }
    if (quest == 1)
    {
        float best = 10000;
        Vec2 target = RelayPosition(0);
        for (int i = 0; i < 3; ++i)
        {
            if (!restored[i] && Distance(PlayerPosition(), RelayPosition(i)) < best)
            {
                best = Distance(PlayerPosition(), RelayPosition(i));
                target = RelayPosition(i);
            }
        }
        return target;
    }
    if (quest == 2)
    {
        return bossActive ? bossHome : CorePosition();
    }
    return ShipPosition();
}

std::wstring GameWorld::ObjectiveText() const
{
    if (quest == 0)
    {
        return L"광장의 주민 세라와 대화하기";
    }
    if (quest == 1)
    {
        return L"끊어진 중계기 복구하기  " + std::to_wstring(RestoredCount()) + L" / 3";
    }
    if (quest == 2)
    {
        return bossDefeated ? L"관리자에게 접속해 행성의 미래 선택하기"
                            : (bossActive ? L"첫 행성 보스 · ORACLE 처치하기"
                                          : L"중앙 첨탑의 단말에서 보스전 시작하기");
    }
    return L"노마드로 돌아가 다음 여정 준비하기";
}

std::wstring GameWorld::NearbyText() const
{
    if (nearby == InvalidActor)
    {
        return L"";
    }
    const Prop* target = scene.Find<Prop>(nearby);
    if (!target || !target->IsActive())
    {
        return L"";
    }
    const Prop& p = *target;
    if (planet.homes.find(p.Id()) != planet.homes.end())
    {
        return L"집의 거주 정보 확인";
    }
    switch (p.kind)
    {
        case PropKind::Citizen:
            return L"주민과 대화";
        case PropKind::Memory:
            return L"기억의 잔향 읽기";
        case PropKind::Core:
            return quest == 2 && !bossDefeated ? L"ORACLE 보스전 시작" : L"관리자에게 접속";
        case PropKind::Ship:
            return quest >= 3 ? L"다음 여정으로" : L"노마드 살펴보기";
        default:
            return restored[p.variant] ? L"복구된 중계기" : L"중계기 복구";
    }
}

std::wstring GameWorld::Region() const
{
    int t = Tile((int)std::round(PlayerPosition().x), (int)std::round(PlayerPosition().y));
    static const wchar_t* names[] = {L"노마드 착륙장",  L"제7 주거구역", L"정원 마을",
                                     L"흑항 교역 마을", L"첨탑 정착지",  L"마을 간 도로",
                                     L"엘리시움 황야"};
    return t >= 0 && t < 7 ? names[t] : L"엘리시움 변경";
}
