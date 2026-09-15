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
        combatText.push_back({player, L"LEVEL UP", 0xf2cb83, 1.5f});
        Notify(L"레벨 " + std::to_wstring(stats.level) +
               L" 달성 · 공격력 / 연사 / 최대 체력 / 탐사 능력 증가");
    }
}

void GameWorld::DamagePlayer(float amount)
{
    if (view != View::Explore || invulnerability > 0 || amount <= 0)
    {
        return;
    }
    stats.health = (std::max)(0.f, stats.health - amount);
    invulnerability = .7f;
    combatText.push_back({player, L"-" + std::to_wstring((int)amount), 0xff7277, 1});
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
    if (projectiles.size() < 160)
    {
        projectiles.push_back(
            {position, Normalize(direction), speed, range, damage, hostile, critical});
    }
}

void GameWorld::UpdateCombat(float dt)
{
    BuildFlow();
    shotCooldown = (std::max)(0.f, shotCooldown - dt);
    invulnerability = (std::max)(0.f, invulnerability - dt);
    magnetTime = (std::max)(0.f, magnetTime - dt);
    muzzleFlash = (std::max)(0.f, muzzleFlash - dt);
    if (firing && shotCooldown <= 0)
    {
        aim = Normalize(aim);
        facing = aim;
        bool critical = (random() % 10000) < (unsigned)(stats.CriticalChance() * 10000);
        Fire(player, aim, false, stats.Damage() * (critical ? 1.6f : 1.f), stats.Range(), 660,
             critical);
        shotCooldown = stats.ShotInterval();
        muzzleFlash = .065f;
        ++shotsFired;
    }
    UpdateEnemies(dt);
    if (view == View::Defeat)
    {
        return;
    }
    UpdateProjectiles(dt);
    if (view == View::Defeat)
    {
        return;
    }
    UpdateLoot(dt);
    for (auto& text : combatText)
    {
        text.life -= dt;
    }
    combatText.erase(std::remove_if(combatText.begin(), combatText.end(),
                                    [](const CombatText& text)
                                    {
                                        return text.life <= 0;
                                    }),
                     combatText.end());
    if (combatText.size() > 48)
    {
        combatText.erase(combatText.begin(), combatText.end() - 48);
    }
    for (int camp = 0; camp < 3; ++camp)
    {
        if (restored[camp] || bossActive || bossDefeated || !CampClear(camp))
        {
            respawn[camp] = 25;
            continue;
        }
        respawn[camp] -= dt;
        if (respawn[camp] <= 0 && Distance(Project(player), Project(rooms[camp + 1])) > 400)
        {
            SpawnCamp(camp);
            respawn[camp] = 25;
        }
    }
}

void GameWorld::UpdateEnemies(float dt)
{
    for (auto& enemy : enemies)
    {
        if (view != View::Explore)
        {
            break;
        }
        enemy.flash = (std::max)(0.f, enemy.flash - dt);
        enemy.cooldown = (std::max)(0.f, enemy.cooldown - dt);
        float distance = Distance(Project(enemy.p), Project(player));
        bool boss = enemy.kind == EnemyKind::Boss;
        bool phaseTwo = boss && enemy.health <= enemy.maxHealth * .5f;
        if (enemy.windup > 0)
        {
            enemy.windup -= dt;
            if (enemy.windup <= 0)
            {
                Vec2 direction = Normalize(Project(enemy.target) - Project(enemy.p));
                if (boss && enemy.attack % 3 == 2)
                {
                    if (Distance(Project(player), Project(enemy.target)) < 78 &&
                        ClearPath(enemy.target, player, .04f))
                    {
                        DamagePlayer(phaseTwo ? 24.f : 20.f);
                    }
                    combatText.push_back({enemy.target, L"IMPACT", 0xff746e, .65f});
                }
                else if (boss && enemy.attack % 3 == 1)
                {
                    int count = phaseTwo ? 12 : 8;
                    for (int i = 0; i < count; ++i)
                    {
                        Fire(enemy.p, Rotate(direction, 2 * Pi * i / count), true, 14, 650, 170);
                    }
                }
                else
                {
                    int spread = boss ? (phaseTwo ? 2 : 1) : 0;
                    for (int i = -spread; i <= spread; ++i)
                    {
                        Fire(enemy.p, Rotate(direction, i * .22f), true, boss ? 16.f : 10.f,
                             boss ? 680.f : 400.f, boss ? 220.f : 195.f);
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
                enemy.target = player;
                enemy.windup = phaseTwo ? .85f : 1.15f;
            }
            continue;
        }
        if (distance > 440 || Tile((int)std::round(player.x), (int)std::round(player.y)) == 0)
        {
            MoveActor(enemy.p, enemy.home, 65, dt);
            continue;
        }
        if (enemy.kind == EnemyKind::Drone)
        {
            if (distance < 26 && enemy.cooldown <= 0 && ClearPath(enemy.p, player, .08f))
            {
                DamagePlayer(8);
                enemy.cooldown = 1.1f;
            }
            else if (distance >= 23)
            {
                MoveActor(enemy.p, FlowTarget(enemy.p), 80, dt);
            }
        }
        else if (distance < 290 && ClearPath(enemy.p, player, .04f))
        {
            if (enemy.cooldown <= 0)
            {
                enemy.target = player;
                enemy.windup = .75f;
            }
        }
        else
        {
            MoveActor(enemy.p, FlowTarget(enemy.p), 58, dt);
        }
    }
}

void GameWorld::UpdateProjectiles(float dt)
{
    for (auto& bullet : projectiles)
    {
        float travel = (std::min)(bullet.remaining, bullet.speed * dt);
        int steps = (std::max)(1, (int)std::ceil(travel / 4.f));
        float step = travel / steps;
        for (int i = 0; i < steps && bullet.remaining > 0; ++i)
        {
            bullet.p = bullet.p + Unproject(bullet.direction * step);
            bullet.remaining -= step;
            if (!Walkable(bullet.p, .035f))
            {
                bullet.remaining = 0;
                break;
            }
            if (bullet.hostile)
            {
                if (Distance(Project(bullet.p), Project(player)) < 15)
                {
                    DamagePlayer(bullet.damage);
                    bullet.remaining = 0;
                }
            }
            else
            {
                for (auto& enemy : enemies)
                {
                    float radius = enemy.kind == EnemyKind::Boss ? 32.f : 18.f;
                    if (enemy.health > 0 && Distance(Project(bullet.p), Project(enemy.p)) < radius)
                    {
                        enemy.health -= bullet.damage;
                        enemy.flash = .12f;
                        combatText.push_back({enemy.p,
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
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
                                     [](const Projectile& bullet)
                                     {
                                         return bullet.remaining <= 0;
                                     }),
                      projectiles.end());
    // Process each death once, after all projectile references have been released.
    if (view == View::Defeat)
    {
        return;
    }
    for (const auto& enemy : enemies)
    {
        if (enemy.health <= 0)
        {
            DefeatEnemy(enemy);
        }
    }
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                                 [](const Enemy& enemy)
                                 {
                                     return enemy.health <= 0;
                                 }),
                  enemies.end());
    if (bossDefeated)
    {
        projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
                                         [](const Projectile& bullet)
                                         {
                                             return bullet.hostile;
                                         }),
                          projectiles.end());
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
        AddLoot(LootKind::Soul, enemy.p, 200);
        AddLoot(LootKind::Weapon, enemy.p + Vec2{.5f, 0}, 2);
        AddLoot(LootKind::Health, enemy.p + Vec2{0, .5f}, 2);
        AddLoot(LootKind::Magnet, enemy.p + Vec2{-.5f, 0});
        Notify(L"ORACLE 격파 · 전리품을 모은 뒤 단말에 접속해 주민의 미래를 선택하세요.");
        return;
    }
    GainExperience(8);
    AddLoot(LootKind::Soul, enemy.p, enemy.kind == EnemyKind::Sentry ? 30 : 20);
    if (kills <= 3)
    {
        LootKind firstDrops[] = {LootKind::Weapon, LootKind::Health, LootKind::Magnet};
        AddLoot(firstDrops[kills - 1], enemy.p + Vec2{.35f, .35f});
    }
    else
    {
        unsigned roll = random() % 100;
        if (roll < 22)
        {
            AddLoot(LootKind::Weapon, enemy.p);
        }
        else if (roll < 47)
        {
            AddLoot(LootKind::Health, enemy.p);
        }
        else if (roll < 59)
        {
            AddLoot(LootKind::Magnet, enemy.p);
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
        for (int i = 0; i < NavCount; ++i)
        {
            float distance = Distance(position, NodePosition(i));
            if (navigation[i] && distance < best)
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
    position = NodePosition(node);
    int count = 0;
    Loot* closest = nullptr;
    float nearest = 100000;
    for (auto& item : loot)
    {
        if (item.kind != kind)
        {
            continue;
        }
        ++count;
        float distance = Distance(item.p, position);
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
    loot.push_back({kind, position, amount, false});
}

void GameWorld::Collect(const Loot& item)
{
    ++itemsCollected;
    switch (item.kind)
    {
        case LootKind::Soul:
            GainExperience(item.amount);
            combatText.push_back(
                {player, L"+" + std::to_wstring(item.amount) + L" XP", 0xb5a4ff, 1});
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
            combatText.push_back({player, L"HEAL", 0x78e3aa, 1});
            break;
        case LootKind::Magnet:
            magnetTime = (std::min)(24.f, magnetTime + 8.f * item.amount);
            Notify(L"자석 가동 · 8초간 넓은 범위의 아이템을 경로를 따라 끌어옵니다.");
            break;
    }
}

void GameWorld::UpdateLoot(float dt)
{
    for (auto& item : loot)
    {
        if (item.kind == LootKind::Health && stats.health >= stats.MaxHealth())
        {
            continue;
        }
        float distance = Distance(Project(item.p), Project(player));
        float range = magnetTime > 0 ? 580.f : stats.PickupRange();
        item.attracted = item.attracted || distance <= range;
        if (item.attracted)
        {
            MoveActor(item.p, FlowTarget(item.p), magnetTime > 0 ? 440.f : 240.f, dt);
        }
        if (Distance(Project(item.p), Project(player)) < 18 && ClearPath(item.p, player, .08f))
        {
            Collect(item);
            item.amount = 0;
        }
    }
    loot.erase(std::remove_if(loot.begin(), loot.end(),
                              [](const Loot& item)
                              {
                                  return item.amount == 0;
                              }),
               loot.end());
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
    boss.p = boss.home = bossHome;
    boss.health = boss.maxHealth = 640;
    boss.cooldown = 2;
    boss.camp = -1;
    enemies.push_back(boss);
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
