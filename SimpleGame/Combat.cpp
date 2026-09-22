#include "stdafx.h"
#include "GameWorld.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Pi = 3.14159265f;

    Vec2 Normalize(Vec2 direction)
    {
        float length = GameWorld::Distance(direction, {});
        return length > .001f ? direction * (1.f / length) : Vec2{1, 0};
    }

    Vec2 Rotate(Vec2 direction, float angle)
    {
        return {direction.x * std::cos(angle) - direction.y * std::sin(angle),
                direction.x * std::sin(angle) + direction.y * std::cos(angle)};
    }
} // namespace

int PlayerStats::NextLevelExperience() const
{
    return level >= MaxLevel ? 0 : 60 + 25 * (level - 1) + 5 * (level - 1) * (level - 1);
}

float PlayerStats::MaxHealth() const
{
    return 100.f + 7.f * (level - 1);
}

float PlayerStats::Damage() const
{
    return 12.f + 1.8f * (level - 1) + 4.f * weaponTier;
}

float PlayerStats::ShotInterval() const
{
    return (std::max)(.11f, .42f / (1 + .022f * (level - 1)) * std::pow(.94f, (float)weaponTier));
}

float PlayerStats::Range() const
{
    return 350.f + 1.5f * (std::min)(60, level - 1);
}

float PlayerStats::MoveSpeed() const
{
    return 155.f + .65f * (std::min)(40, level - 1);
}

float PlayerStats::PickupRange() const
{
    return 55.f + .8f * (std::min)(40, level - 1);
}

float PlayerStats::CriticalChance() const
{
    return (std::min)(.20f, .05f + .0015f * (level - 1));
}

void GameWorld::GainExperience(int amount)
{
    if (amount <= 0 || stats.level >= PlayerStats::MaxLevel)
    {
        return;
    }
    int previous = stats.level;
    int64_t total = (int64_t)stats.experience + amount;
    while (stats.level < PlayerStats::MaxLevel && total >= stats.NextLevelExperience())
    {
        total -= stats.NextLevelExperience();
        ++stats.level;
        stats.health = (std::min)(stats.MaxHealth(), stats.health + 22.f);
    }
    stats.experience = stats.level == PlayerStats::MaxLevel ? 0 : (int)total;
    if (previous != stats.level)
    {
        Spawn(CombatText{PlayerPosition(), L"LEVEL UP", 0xf2cb83, 1.5f});
        Notify(L"레벨 " + std::to_wstring(stats.level) +
               L" 달성 · 공격력 / 연사 / 최대 체력 / 탐사 능력 증가");
    }
}

void GameWorld::DamagePlayer(float amount)
{
    if (view != View::Explore || !PlayerActive() || invulnerability > 0 || amount <= 0)
    {
        return;
    }
    stats.health = (std::max)(0.f, stats.health - amount);
    invulnerability = .7f;
    Spawn(CombatText{PlayerPosition(), L"-" + std::to_wstring((int)amount), 0xff7277, 1});
    if (stats.health <= 0)
    {
        view = View::Defeat;
        firing = walking = captainWalking = false;
        Notify(L"탐사 중단 · 같은 지형으로 다시 도전하거나 새로운 행성을 생성하세요.");
    }
}

void GameWorld::Fire(Vec2 position, Vec2 direction, bool hostile, float damage, float range,
                     float speed, bool critical)
{
    if (scene.Actors<Projectile>().size() < 160)
    {
        Spawn(Projectile{position, Normalize(direction), speed, range, damage, hostile, critical});
    }
}

void GameWorld::UpdateCombat(float dt)
{
    BuildFlow();
    shotCooldown = (std::max)(0.f, shotCooldown - dt);
    invulnerability = (std::max)(0.f, invulnerability - dt);
    magnetTime = (std::max)(0.f, magnetTime - dt);
    muzzleFlash = (std::max)(0.f, muzzleFlash - dt);
    if (PlayerActive() && firing && shotCooldown <= 0)
    {
        aim = Normalize(aim);
        facing = aim;
        bool critical = (random() % 10000) < (unsigned)(stats.CriticalChance() * 10000);
        Fire(PlayerPosition(), aim, false, stats.Damage() * (critical ? 1.6f : 1.f), stats.Range(),
             660, critical);
        shotCooldown = stats.ShotInterval();
        muzzleFlash = .065f;
        ++shotsFired;
    }
    if (PlayerActive())
    {
        UpdateEnemies(dt);
    }
    if (view == View::Defeat)
    {
        return;
    }
    UpdateProjectiles(dt);
    if (view == View::Defeat)
    {
        return;
    }
    if (PlayerActive())
    {
        UpdateLoot(dt);
    }
    auto texts = scene.Actors<CombatText>();
    for (size_t i = 0; i + 48 < texts.size(); ++i)
    {
        texts[i].Destroy();
    }
    for (int camp = 0; camp < 3; ++camp)
    {
        if (restored[camp] || bossActive || bossDefeated || !CampClear(camp))
        {
            respawn[camp] = 25;
            continue;
        }
        respawn[camp] -= dt;
        if (respawn[camp] <= 0 &&
            Distance(Project(PlayerPosition()), Project(rooms[camp + 1])) > 400)
        {
            SpawnCamp(camp);
            respawn[camp] = 25;
        }
    }
}

void GameWorld::UpdateEnemies(float dt)
{
    for (auto& enemy : scene.Actors<Enemy>())
    {
        if (view != View::Explore)
        {
            break;
        }
        enemy.flash = (std::max)(0.f, enemy.flash - dt);
        enemy.cooldown = (std::max)(0.f, enemy.cooldown - dt);
        float distance = Distance(Project(enemy.Position()), Project(PlayerPosition()));
        bool boss = enemy.kind == EnemyKind::Boss;
        bool phaseTwo = boss && enemy.health <= enemy.maxHealth * .5f;
        if (enemy.windup > 0)
        {
            enemy.windup -= dt;
            if (enemy.windup <= 0)
            {
                Vec2 direction = Normalize(Project(enemy.target) - Project(enemy.Position()));
                if (boss && enemy.attack % 3 == 2)
                {
                    if (Distance(Project(PlayerPosition()), Project(enemy.target)) < 78 &&
                        ClearPath(enemy.target, PlayerPosition(), .04f))
                    {
                        DamagePlayer(phaseTwo ? 24.f : 20.f);
                    }
                    Spawn(CombatText{enemy.target, L"IMPACT", 0xff746e, .65f});
                }
                else if (boss && enemy.attack % 3 == 1)
                {
                    int count = phaseTwo ? 12 : 8;
                    for (int i = 0; i < count; ++i)
                    {
                        Fire(enemy.Position(), Rotate(direction, 2 * Pi * i / count), true, 14, 650,
                             170);
                    }
                }
                else
                {
                    int spread = boss ? (phaseTwo ? 2 : 1) : 0;
                    for (int i = -spread; i <= spread; ++i)
                    {
                        Fire(enemy.Position(), Rotate(direction, i * .22f), true,
                             boss ? 16.f : 10.f, boss ? 680.f : 400.f, boss ? 220.f : 195.f);
                    }
                }
                ++enemy.attack;
                enemy.cooldown = boss ? (phaseTwo ? .9f : 1.35f) : 1.8f;
            }
            continue;
        }
        if (boss)
        {
            // The arena never seals: withdrawing is a valid way to recover and regroup.
            if (distance < 560 && enemy.cooldown <= 0)
            {
                enemy.target = PlayerPosition();
                enemy.windup = phaseTwo ? .85f : 1.15f;
            }
            continue;
        }
        if (distance > 440 ||
            Tile((int)std::round(PlayerPosition().x), (int)std::round(PlayerPosition().y)) == 0)
        {
            MoveActor(enemy, enemy.home, 65, dt);
            continue;
        }
        if (enemy.kind == EnemyKind::Drone)
        {
            if (distance < 26 && enemy.cooldown <= 0 &&
                ClearPath(enemy.Position(), PlayerPosition(), .08f))
            {
                DamagePlayer(8);
                enemy.cooldown = 1.1f;
            }
            else if (distance >= 23)
            {
                MoveActor(enemy, FlowTarget(enemy.Position()), 80, dt);
            }
        }
        else if (distance < 290 && ClearPath(enemy.Position(), PlayerPosition(), .04f))
        {
            if (enemy.cooldown <= 0)
            {
                enemy.target = PlayerPosition();
                enemy.windup = .75f;
            }
        }
        else
        {
            MoveActor(enemy, FlowTarget(enemy.Position()), 58, dt);
        }
    }
}

void GameWorld::UpdateProjectiles(float dt)
{
    auto targets = scene.Actors<Enemy>();
    for (auto& bullet : scene.Actors<Projectile>())
    {
        float travel = (std::min)(bullet.remaining, bullet.speed * dt);
        int steps = (std::max)(1, (int)std::ceil(travel / 4.f));
        float step = travel / steps;
        for (int i = 0; i < steps && bullet.remaining > 0; ++i)
        {
            bullet.SetPosition(bullet.Position() + Unproject(bullet.direction * step));
            bullet.remaining -= step;
            if (!Walkable(bullet.Position(), .035f))
            {
                bullet.remaining = 0;
                break;
            }
            if (bullet.hostile)
            {
                if (PlayerActive() &&
                    Distance(Project(bullet.Position()), Project(PlayerPosition())) < 15)
                {
                    DamagePlayer(bullet.damage);
                    bullet.remaining = 0;
                }
            }
            else
            {
                for (auto& enemy : targets)
                {
                    float radius = enemy.kind == EnemyKind::Boss ? 32.f : 18.f;
                    if (enemy.health > 0 &&
                        Distance(Project(bullet.Position()), Project(enemy.Position())) < radius)
                    {
                        enemy.health -= bullet.damage;
                        enemy.flash = .12f;
                        Spawn(CombatText{enemy.Position(),
                                         std::to_wstring((int)std::ceil(bullet.damage)) +
                                             (bullet.critical ? L"!" : L""),
                                         bullet.critical ? 0xf8cc7cu : 0xe8f6efu, 1});
                        bullet.remaining = 0;
                        break;
                    }
                }
            }
        }
    }
    scene.RemoveIf<Projectile>(
        [](const Projectile& bullet)
        {
            return bullet.remaining <= 0;
        });
    // Process each death once, after all projectile references have been released.
    if (view == View::Defeat)
    {
        return;
    }
    for (const auto& enemy : scene.Actors<Enemy>())
    {
        if (enemy.health <= 0)
        {
            DefeatEnemy(enemy);
        }
    }
    scene.RemoveIf<Enemy>(
        [](const Enemy& enemy)
        {
            return enemy.health <= 0;
        });
    if (bossDefeated)
    {
        scene.RemoveIf<Projectile>(
            [](const Projectile& bullet)
            {
                return bullet.hostile;
            });
    }
}

void GameWorld::DefeatEnemy(const Enemy& enemy)
{
    ++kills;
    if (enemy.kind == EnemyKind::Boss)
    {
        bossActive = false;
        bossDefeated = true;
        GainExperience(450);
        AddLoot(LootKind::Soul, enemy.Position(), 200);
        AddLoot(LootKind::Weapon, enemy.Position() + Vec2{.5f, 0}, 2);
        AddLoot(LootKind::Health, enemy.Position() + Vec2{0, .5f}, 2);
        AddLoot(LootKind::Magnet, enemy.Position() + Vec2{-.5f, 0});
        Notify(L"ORACLE 격파 · 전리품을 모은 뒤 단말에 접속해 주민의 미래를 선택하세요.");
        return;
    }
    GainExperience(8);
    AddLoot(LootKind::Soul, enemy.Position(), enemy.kind == EnemyKind::Sentry ? 30 : 20);
    if (kills <= 3)
    {
        LootKind firstDrops[] = {LootKind::Weapon, LootKind::Health, LootKind::Magnet};
        AddLoot(firstDrops[kills - 1], enemy.Position() + Vec2{.35f, .35f});
    }
    else
    {
        unsigned roll = random() % 100;
        if (roll < 22)
        {
            AddLoot(LootKind::Weapon, enemy.Position());
        }
        else if (roll < 47)
        {
            AddLoot(LootKind::Health, enemy.Position());
        }
        else if (roll < 59)
        {
            AddLoot(LootKind::Magnet, enemy.Position());
        }
    }
}

void GameWorld::AddLoot(LootKind kind, Vec2 position, int amount)
{
    if (amount <= 0)
    {
        return;
    }
    int node = NearestNode(position);
    if (node < 0)
    {
        // A scattered reward can land inside a prop; relocate it to the nearest reachable floor.
        float best = 100000;
        for (int i = 0; i < navigation.Count(); ++i)
        {
            float distance = Distance(position, navigation.Position(i));
            if (navigation.IsOpen(i) && distance < best)
            {
                node = i;
                best = distance;
            }
        }
    }
    if (node < 0)
    {
        return;
    }
    position = navigation.Position(node);
    int count = 0;
    Loot* closest = nullptr;
    float nearest = 100000;
    for (auto& item : scene.Actors<Loot>())
    {
        if (item.kind != kind)
        {
            continue;
        }
        ++count;
        float distance = Distance(item.Position(), position);
        if (distance < .3f)
        {
            item.amount += amount;
            return;
        }
        if (distance < nearest)
        {
            closest = &item;
            nearest = distance;
        }
    }
    // Reserve capacity per type so a full field of soul stones cannot discard a new reward type.
    if (count >= 32 && closest)
    {
        closest->amount += amount;
        return;
    }
    Spawn(Loot{kind, position, amount, false});
}

void GameWorld::Collect(const Loot& item)
{
    ++itemsCollected;
    switch (item.kind)
    {
        case LootKind::Soul:
            GainExperience(item.amount);
            Spawn(CombatText{PlayerPosition(), L"+" + std::to_wstring(item.amount) + L" XP",
                             0xb5a4ff, 1});
            break;
        case LootKind::Weapon:
        {
            int gained = (std::min)(item.amount, PlayerStats::MaxWeaponTier - stats.weaponTier);
            stats.weaponTier += gained;
            GainExperience((item.amount - gained) * 40);
            Notify(L"무기 강화 +" + std::to_wstring(stats.weaponTier) +
                   L" · 공격력과 연사 속도 증가 (최대 강화 초과분은 경험치로 전환)");
            break;
        }
        case LootKind::Health:
            stats.health = (std::min)(stats.MaxHealth(),
                                      stats.health + stats.MaxHealth() * .35f * item.amount);
            Spawn(CombatText{PlayerPosition(), L"HEAL", 0x78e3aa, 1});
            break;
        case LootKind::Magnet:
            magnetTime = (std::min)(24.f, magnetTime + 8.f * item.amount);
            Notify(L"자석 가동 · 8초간 넓은 범위의 아이템을 경로를 따라 끌어옵니다.");
            break;
    }
}

void GameWorld::UpdateLoot(float dt)
{
    for (auto& item : scene.Actors<Loot>())
    {
        if (item.kind == LootKind::Health && stats.health >= stats.MaxHealth())
        {
            continue;
        }
        float distance = Distance(Project(item.Position()), Project(PlayerPosition()));
        float range = magnetTime > 0 ? 580.f : stats.PickupRange();
        item.attracted = item.attracted || distance <= range;
        if (item.attracted)
        {
            MoveActor(item, FlowTarget(item.Position()), magnetTime > 0 ? 440.f : 240.f, dt);
        }
        if (Distance(Project(item.Position()), Project(PlayerPosition())) < 18 &&
            ClearPath(item.Position(), PlayerPosition(), .08f))
        {
            Collect(item);
            item.amount = 0;
        }
    }
    scene.RemoveIf<Loot>(
        [](const Loot& item)
        {
            return item.amount == 0;
        });
}

void GameWorld::BeginBoss()
{
    if (view != View::Explore || RestoredCount() < 3 || bossActive || bossDefeated)
    {
        if (bossActive)
        {
            Notify(L"ORACLE 교전 중 · 붉은 공격 예고를 피해 이동하며 사격하세요.");
        }
        return;
    }
    Enemy boss;
    boss.kind = EnemyKind::Boss;
    boss.SetPosition(boss.home = bossHome);
    boss.health = boss.maxHealth = 640;
    boss.cooldown = 2;
    boss.camp = -1;
    Spawn(boss);
    bossActive = true;
    firing = false;
    Notify(L"ORACLE · 경계 관리자 기동. 붉은 예고 범위를 벗어나세요. 후퇴할 수 있습니다.");
}

std::wstring GameWorld::TutorialText() const
{
    if (shotsFired == 0)
    {
        return L"01  마우스 조준 + 좌클릭 유지 · 점선 끝이 최대 사거리";
    }
    if (kills < 3)
    {
        return L"02  경비 기계 처치 " + std::to_wstring(kills) + L" / 3 · 움직이며 사격하세요";
    }
    if (stats.level < 2 || itemsCollected < 3)
    {
        return L"03  전리품에 접근해 자동 습득 · 영혼석을 모아 레벨 업";
    }
    if (RestoredCount() < 3)
    {
        return L"04  각 구역의 적을 정리한 뒤 중계기 앞에서 E · M 지도";
    }
    if (!bossDefeated)
    {
        return L"05  중앙 첨탑의 단말에서 보스 도전 · 붉은 예고를 피하세요";
    }
    return L"탐사 학습 완료 · 관리자 단말에서 선택한 뒤 노마드로 귀환";
}

void GameWorld::SpawnCamp(int camp)
{
    if (scene.Actors<Enemy>().size() >= 18)
    {
        return;
    }
    std::vector<Vec2> candidates;
    for (int i = 0; i < navigation.Count(); ++i)
    {
        Vec2 p = navigation.Position(i);
        if (navigation.IsOpen(i) && Distance(p, rooms[camp + 1]) < 2.8f &&
            Distance(Project(p), Project(PlayerPosition())) > 240)
        {
            candidates.push_back(p);
        }
    }
    std::shuffle(candidates.begin(), candidates.end(), random);
    int spawned = 0;
    for (Vec2 p : candidates)
    {
        bool occupied = false;
        for (const auto& enemy : scene.Actors<Enemy>())
        {
            occupied = occupied || Distance(enemy.Position(), p) < .8f;
        }
        if (occupied)
        {
            continue;
        }
        Enemy enemy;
        enemy.kind = spawned % 3 == 2 ? EnemyKind::Sentry : EnemyKind::Drone;
        enemy.SetPosition(enemy.home = p);
        enemy.camp = camp;
        enemy.health = enemy.maxHealth = enemy.kind == EnemyKind::Sentry ? 48.f : 32.f;
        enemy.cooldown = 1.f + spawned * .25f;
        Spawn(enemy);
        if (++spawned >= 4 + (camp > 0 ? 1 : 0))
        {
            break;
        }
    }
}

bool GameWorld::CampClear(int camp) const
{
    for (const auto& enemy : scene.Actors<Enemy>())
    {
        if (enemy.kind != EnemyKind::Boss && enemy.camp == camp && enemy.health > 0)
        {
            return false;
        }
    }
    return true;
}
