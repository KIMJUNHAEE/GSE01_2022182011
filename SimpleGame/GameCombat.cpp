#include "stdafx.h"
#include "Game.h"
#include <algorithm>
#include <cmath>

namespace
{
    const Color Ink(0x08151c), White(0xe9eee6), Muted(0x879b9f), Teal(0x70d7c7);
    const Color Gold(0xe8bd7b), Red(0xf17d7c), Purple(0xb5a4ff), Green(0x78e3aa);
    constexpr float Pi = 3.14159265f;

    Color LootColor(LootKind kind)
    {
        switch (kind)
        {
            case LootKind::Weapon:
                return Gold;
            case LootKind::Health:
                return Green;
            case LootKind::Magnet:
                return Teal;
            default:
                return Purple;
        }
    }
} // namespace

void Game::DrawWeapon(const SceneEffect& effect)
{
    Vec2 origin = P(effect.Position(), 24);
    Vec2 direction = world.aim;
    Vec2 grip = origin + direction * (9 * zoom);
    Vec2 muzzle = origin + direction * (26 * zoom);
    canvas.Line(grip + Vec2{0, 3 * zoom}, muzzle + Vec2{0, 3 * zoom}, Ink, 10 * zoom);
    canvas.Line(grip, muzzle, Color(0x70858b), 8 * zoom);
    canvas.Line(grip - direction * (4 * zoom), muzzle - direction * (5 * zoom), Teal, 2 * zoom);
    canvas.Ellipse(muzzle, 3 * zoom, 3 * zoom, Teal);
    if (world.muzzleFlash > 0)
    {
        canvas.Glow(muzzle, 26 * zoom, 23 * zoom, Gold.Fade(.65f));
        canvas.Line(muzzle, muzzle + direction * (16 * zoom), White, 4 * zoom);
    }
    if (world.invulnerability > 0)
    {
        canvas.Ellipse(P(effect.Position(), 25), 23 * zoom, 34 * zoom,
                       Red.Fade(.35f + .3f * std::sin(world.time * 35)), false, 2 * zoom);
    }
}

void Game::DrawEnemy(const Enemy& enemy)
{
    bool boss = enemy.kind == EnemyKind::Boss;
    float size = boss ? 2.f : 1.f;
    Vec2 foot = P(enemy.Position());
    float hover = std::sin(world.time * 3 + enemy.home.x) * 3;
    Vec2 body = P(enemy.Position(), (boss ? 42.f : 27.f) + hover);
    Color shell = enemy.flash > 0 ? White : Color(boss ? 0x4a3d55 : 0x425362);
    Color energy = boss && enemy.health < enemy.maxHealth * .5f ? Gold : Red;
    canvas.Ellipse(foot, 23 * size * zoom, 10 * size * zoom, Ink.Fade(.68f));
    canvas.Glow(body, 27 * size * zoom, 23 * size * zoom, energy.Fade(.16f));
    if (enemy.kind == EnemyKind::Sentry)
    {
        for (int side : {-1, 1})
        {
            Vec2 joint = body + Vec2{side * 16.f * zoom, 10 * zoom};
            canvas.Line(body, joint, shell, 7 * zoom);
            canvas.Line(joint, foot + Vec2{side * 24.f * zoom, 0}, Color(0x64727c), 4 * zoom);
        }
    }
    else
    {
        for (int side : {-1, 1})
        {
            Vec2 wing = body + Vec2{side * 22.f * size * zoom, 3 * zoom};
            canvas.Line(body, wing, Color(0x243642), 9 * size * zoom);
            canvas.Line(wing + Vec2{0, -8 * zoom}, wing + Vec2{0, 8 * zoom}, shell,
                        6 * size * zoom);
            canvas.Glow(wing + Vec2{0, 8 * zoom}, 6 * size * zoom, 13 * size * zoom,
                        energy.Fade(.45f));
        }
    }
    canvas.Quad(body + Vec2{0, -17 * size * zoom}, body + Vec2{19 * size * zoom, 0},
                body + Vec2{0, 19 * size * zoom}, body + Vec2{-19 * size * zoom, 0}, Ink);
    canvas.Quad(body + Vec2{0, -14 * size * zoom}, body + Vec2{16 * size * zoom, 0},
                body + Vec2{0, 15 * size * zoom}, body + Vec2{-16 * size * zoom, 0}, shell);
    canvas.Line(body + Vec2{-10 * size * zoom, 0}, body + Vec2{10 * size * zoom, 0}, energy,
                3 * zoom);
    canvas.Ellipse(body, 5 * size * zoom, 5 * size * zoom, energy);
    canvas.Ellipse(body, 2 * size * zoom, 2 * size * zoom, White);
    if (boss)
    {
        canvas.Ellipse(body, 37 * zoom, 50 * zoom, energy.Fade(.55f), false, 2 * zoom);
        for (int i = 0; i < 3; ++i)
        {
            float angle = world.time * .6f + i * 2 * Pi / 3;
            Marker(body + Vec2{std::cos(angle) * 39 * zoom, std::sin(angle) * 50 * zoom}, energy,
                   6 * zoom);
        }
    }
    else
    {
        Vec2 hp = P(enemy.Position(), 62);
        canvas.Rect(hp.x - 22 * zoom, hp.y, 44 * zoom, 4 * zoom, Ink);
        canvas.Rect(hp.x - 22 * zoom, hp.y,
                    44 * zoom * (std::max)(0.f, enemy.health / enemy.maxHealth), 4 * zoom, energy);
    }
}

void Game::DrawEnemyWarning(const Enemy& enemy)
{
    if (enemy.windup <= 0)
    {
        return;
    }
    bool boss = enemy.kind == EnemyKind::Boss;
    Color warning = Red.Fade(.55f + .2f * std::sin(world.time * 17));
    if (boss && enemy.attack % 3 == 2)
    {
        Vec2 target = P(enemy.target);
        canvas.Ellipse(target, 78 * zoom, 78 * zoom, Red.Fade(.13f));
        canvas.Ellipse(target, 78 * zoom, 78 * zoom, warning, false, 2 * zoom);
        float duration = enemy.health <= enemy.maxHealth * .5f ? .85f : 1.15f;
        float progress = (std::max)(0.f, 1 - enemy.windup / duration);
        canvas.Ellipse(target, 78 * zoom * progress, 78 * zoom * progress, warning, false);
        canvas.Line(target + Vec2{-12, 0}, target + Vec2{12, 0}, warning, 2);
        canvas.Line(target + Vec2{0, -12}, target + Vec2{0, 12}, warning, 2);
    }
    else if (boss && enemy.attack % 3 == 1)
    {
        canvas.Ellipse(P(enemy.Position()), 92 * zoom, 92 * zoom, Red.Fade(.1f));
        canvas.Ellipse(P(enemy.Position()), 92 * zoom, 92 * zoom, warning, false, 2 * zoom);
        CenterText(P(enemy.Position()).x, P(enemy.Position()).y + 45 * zoom, L"방사 탄막", 12, Red);
    }
    else
    {
        Vec2 source = P(enemy.Position(), 24);
        Vec2 direction = P(enemy.target, 24) - source;
        float length = GameWorld::Distance(direction, {});
        direction = length > 1 ? direction * (1 / length) : Vec2{1, 0};
        int spread = boss ? (enemy.health <= enemy.maxHealth * .5f ? 2 : 1) : 0;
        for (int i = -spread; i <= spread; ++i)
        {
            float angle = i * .22f;
            Vec2 ray = {direction.x * std::cos(angle) - direction.y * std::sin(angle),
                        direction.x * std::sin(angle) + direction.y * std::cos(angle)};
            canvas.Line(source, source + ray * (boss ? 360.f : 290.f) * zoom, warning, 1.5f);
        }
    }
}

void Game::Draw(const Loot& item)
{
    Vec2 p = P(item.Position(), 9 + std::sin(world.time * 4 + item.Position().x) * 3);
    Color color = LootColor(item.kind);
    canvas.Ellipse(P(item.Position()), 10 * zoom, 5 * zoom, color.Fade(.2f));
    canvas.Glow(p, 18 * zoom, 21 * zoom, color.Fade(.24f));
    canvas.Ellipse(p, 10 * zoom, 10 * zoom, Ink.Fade(.9f));
    if (item.kind == LootKind::Soul)
    {
        Marker(p, color, 7 * zoom);
    }
    else if (item.kind == LootKind::Health)
    {
        canvas.Line(p + Vec2{-6 * zoom, 0}, p + Vec2{6 * zoom, 0}, color, 4 * zoom);
        canvas.Line(p + Vec2{0, -6 * zoom}, p + Vec2{0, 6 * zoom}, color, 4 * zoom);
    }
    else if (item.kind == LootKind::Weapon)
    {
        canvas.Line(p + Vec2{-6 * zoom, 4 * zoom}, p + Vec2{6 * zoom, -4 * zoom}, color, 4 * zoom);
        canvas.Line(p + Vec2{0, -6 * zoom}, p + Vec2{0, 6 * zoom}, White, 1.5f * zoom);
    }
    else
    {
        canvas.Line(p + Vec2{-5 * zoom, -5 * zoom}, p + Vec2{-5 * zoom, 5 * zoom}, color, 3 * zoom);
        canvas.Line(p + Vec2{5 * zoom, -5 * zoom}, p + Vec2{5 * zoom, 5 * zoom}, color, 3 * zoom);
        canvas.Line(p + Vec2{-5 * zoom, 5 * zoom}, p + Vec2{5 * zoom, 5 * zoom}, color, 3 * zoom);
    }
}

void Game::Draw(const Projectile& bullet)
{
    Vec2 p = P(bullet.Position(), 24);
    Color color = bullet.hostile ? Red : (bullet.critical ? Gold : Teal);
    canvas.Glow(p, 10 * zoom, 10 * zoom, color.Fade(.45f));
    canvas.Line(p - bullet.direction * (bullet.hostile ? 10.f : 18.f) * zoom, p, color,
                (bullet.hostile ? 4.f : 3.f) * zoom);
    canvas.Ellipse(p, 2 * zoom, 2 * zoom, White);
}

void Game::Draw(const CombatText& text)
{
    Vec2 p = P(text.Position(), 65 + (1.5f - text.life) * 24);
    CenterText(p.x, p.y, text.text, 16, Color(text.color, (std::min)(1.f, text.life * 2)), true);
}

void Game::DrawAimGuide()
{
    if (world.PlayerActive() && mouseInside && world.view == View::Explore &&
        !(mouse.x > 1180 && mouse.y < 226))
    {
        Vec2 origin = P(world.PlayerPosition(), 24);
        for (float distance = 38; distance < world.stats.Range(); distance += 20)
        {
            Vec2 grid = world.PlayerPosition() + GameWorld::Unproject(world.aim * distance);
            if (!world.Walkable(grid, .035f))
            {
                break;
            }
            canvas.Ellipse(origin + world.aim * (distance * zoom), 1.1f, 1.1f, Teal.Fade(.33f));
        }
        Vec2 end = origin + world.aim * (world.stats.Range() * zoom);
        Vec2 perpendicular = {-world.aim.y, world.aim.x};
        canvas.Line(end - perpendicular * 6, end + perpendicular * 6, Teal.Fade(.5f));
    }
}

void Game::CombatHUD()
{
    const auto& stats = world.stats;
    Panel(42, 317, 306, 136, .8f);
    canvas.Text(60, 328, L"탐사 장비  /  강화 +" + std::to_wstring(stats.weaponTier), 13, Gold);
    canvas.Text(60, 356,
                L"공격 " + std::to_wstring((int)stats.Damage()) + L"   연사 " +
                    std::to_wstring((int)std::round(1000 * stats.ShotInterval())) + L" ms",
                13, White);
    canvas.Text(60, 382,
                L"처치 " + std::to_wstring(world.kills) + L"   습득 " +
                    std::to_wstring(world.itemsCollected),
                12, Muted);
    canvas.Text(60, 415,
                world.magnetTime > 0
                    ? L"자석 활성  " + std::to_wstring((int)std::ceil(world.magnetTime)) + L"초"
                    : L"근처 전리품 자동 습득",
                12, Teal);
    canvas.Text(1197, 242, L"전리품 안내", 12, Muted);
    const wchar_t* labels[] = {L"무기 강화 · 공격 / 연사", L"회복 · 최대 HP의 35%",
                               L"영혼석 · 경험치", L"자석 · 8초 광역 습득"};
    for (int i = 0; i < 4; ++i)
    {
        Color color = LootColor((LootKind)i);
        Marker({1204, 279.f + i * 29}, color, 4);
        canvas.Text(1217, 269.f + i * 29, labels[i], 11, color);
    }
    float ratio = stats.level == PlayerStats::MaxLevel
                      ? 1.f
                      : (float)stats.experience / stats.NextLevelExperience();
    canvas.Rect(42, 879, 1355, 5, Color(0x263940));
    canvas.Rect(42, 879, 1355 * ratio, 5, Purple);
    std::wstring xp = stats.level == PlayerStats::MaxLevel
                          ? L"Lv.80  MAX"
                          : L"Lv." + std::to_wstring(stats.level) + L" / 80   XP " +
                                std::to_wstring(stats.experience) + L" / " +
                                std::to_wstring(stats.NextLevelExperience());
    CenterText(720, 861, xp, 11, Purple);
    if (world.view == View::Explore)
    {
        CenterText(720, 759, world.TutorialText(), 13, Gold);
    }
    for (const auto& enemy : world.scene.Actors<Enemy>())
    {
        if (enemy.kind != EnemyKind::Boss)
        {
            continue;
        }
        Panel(454, 20, 530, 58, .92f);
        canvas.Text(472, 28, L"ORACLE  /  경계 관리자", 15, White, true);
        canvas.Text(845, 30, enemy.health <= enemy.maxHealth * .5f ? L"PHASE 02" : L"PHASE 01", 12,
                    Red);
        canvas.Rect(471, 58, 496, 6, Color(0x3c2836));
        canvas.Rect(471, 58, 496 * (std::max)(0.f, enemy.health / enemy.maxHealth), 6, Red);
    }
    if (mouseInside && world.view == View::Explore && !(mouse.x > 1180 && mouse.y < 226))
    {
        Color color =
            GameWorld::Distance(mouse, P(world.PlayerPosition(), 24)) > stats.Range() * zoom ? Muted
                                                                                             : Teal;
        canvas.Ellipse(mouse, 8, 8, Ink.Fade(.7f), false, 4);
        canvas.Ellipse(mouse, 8, 8, color, false, 1);
        canvas.Line(mouse + Vec2{-14, 0}, mouse + Vec2{-5, 0}, color, 1.3f);
        canvas.Line(mouse + Vec2{5, 0}, mouse + Vec2{14, 0}, color, 1.3f);
        canvas.Line(mouse + Vec2{0, -14}, mouse + Vec2{0, -5}, color, 1.3f);
        canvas.Line(mouse + Vec2{0, 5}, mouse + Vec2{0, 14}, color, 1.3f);
    }
}

void Game::Defeat()
{
    canvas.Rect(0, 0, 1440, 900, Ink.Fade(.82f));
    Panel(412, 250, 616, 400, .97f);
    CenterText(720, 280, L"E X P E D I T I O N   I N T E R R U P T E D", 13, Red);
    CenterText(720, 322, L"여정은 아직 끝나지 않았다", 28, White, true);
    CenterText(720, 384,
               L"Lv." + std::to_wstring(world.stats.level) + L"   ·   처치 " +
                   std::to_wstring(world.kills) + L"   ·   복구 " +
                   std::to_wstring(world.RestoredCount()) + L" / 3",
               16, Muted);
    CenterText(720, 441, L"R   같은 지형에서 처음부터 다시 도전", 18, Gold);
    CenterText(720, 484, L"N   새로운 랜덤 지형으로 시작", 18, Teal);
    CenterText(720, 533, L"레벨과 아이템은 초기화됩니다.   ESC  종료", 13, Muted);
    CenterText(720, 600, L"MAP SEED  " + std::to_wstring(world.mapSeed), 12, Muted);
}
