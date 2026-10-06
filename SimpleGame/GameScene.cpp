#include "stdafx.h"
#include "Game.h"
#include <algorithm>
#include <cmath>

namespace
{
    const Color Teal(0x70d7c7), Gold(0xe8bd7b);
}

void Game::Draw(const Prop& actor)
{
    DrawProp(actor);
}

void Game::Draw(const Enemy& actor)
{
    DrawEnemy(actor);
}

void Game::Draw(const Character& actor)
{
    Person(P(actor.Position()), actor.captain ? 1 : 0, zoom,
           actor.captain ? world.captainWalking : world.walking, actor.captain ? 1.2f : 0.f);
}

void Game::Draw(const SceneEffect& effect)
{
    switch (effect.kind)
    {
        case EffectKind::Weapon:
            DrawWeapon(effect);
            break;
        case EffectKind::PlayerRing:
            canvas.Ellipse(P(effect.Position()), 20 * zoom, 9 * zoom, Teal.Fade(.7f), false,
                           1.1f * zoom);
            break;
        case EffectKind::EnemyWarning:
            if (const auto* enemy = world.scene.Find<Enemy>(effect.ParentId()))
            {
                DrawEnemyWarning(*enemy);
            }
            break;
        case EffectKind::PropGround:
            if (const auto* prop = world.scene.Find<Prop>(effect.ParentId()))
            {
                const auto& o = *prop;
                if (o.kind == PropKind::Lamp || o.kind == PropKind::Relay)
                {
                    Color c = o.kind == PropKind::Relay && !world.restored[o.variant] ? Gold : Teal;
                    canvas.QueueGlow(P(effect.Position()), 70 * zoom, 35 * zoom, c.Fade(.14f));
                }
                if (o.kind == PropKind::Core)
                {
                    canvas.QueueGlow(P(effect.Position()), 190 * zoom, 95 * zoom, Teal.Fade(.18f));
                }
                if (o.kind != PropKind::Memory && o.kind != PropKind::Citizen)
                {
                    Vec2 s = P(effect.Position());
                    canvas.Ellipse(s + Vec2{o.h * .24f * zoom, 7 * zoom},
                                   (o.Width() * 26 + o.h * .22f) * zoom,
                                   (o.Depth() * 13 + 10) * zoom, Color(0x061216, .27f));
                }
            }
            break;
        case EffectKind::LandingPad:
        {
            // Landing pad rings and inset guide lights provide a readable arrival landmark.
            Vec2 pad = P(effect.Position());
            canvas.Ellipse(pad, 152 * zoom, 76 * zoom, Gold.Fade(.4f), false, 1.4f * zoom);
            canvas.Ellipse(pad, 145 * zoom, 72.5f * zoom, Color(0xadc2c6, .18f), false, zoom);
            for (int i = 0; i < 12; ++i)
            {
                float angle = i * 6.283185f / 12;
                Vec2 v = pad + Vec2{std::cos(angle) * 152 * zoom, std::sin(angle) * 76 * zoom};
                canvas.Ellipse(v, 2 * zoom, 2 * zoom, Gold);
            }

            break;
        }
        case EffectKind::AmbientDust:
        {
            // Floating ambient dust is deterministic and does not change the playable map.
            for (int i = 0; i < 64; ++i)
            {
                float x = std::fmod(i * 197.3f + world.time * (3 + i % 4), 1500.f) - 30;
                float y = std::fmod(i * 137.7f - world.time * (6 + i % 3) + 10000, 960.f) - 30;
                float alpha = .15f + .22f * (.5f + .5f * std::sin(world.time + i));
                canvas.Ellipse(Vec2{x, y} + GameWorld::Project(effect.Position()) * zoom,
                               i % 3 == 0 ? 1.8f : 1.f, i % 3 == 0 ? 1.8f : 1.f,
                               Color(i % 4 == 0 ? 0xe5bc80 : 0x90c6bf, alpha));
            }

            break;
        }
        case EffectKind::Scan:
        {
            if (world.scanWave >= 0)
            {
                float radius = world.scanWave * 230 * zoom;
                canvas.Ellipse(P(effect.Position()), radius, radius * .5f,
                               Teal.Fade(std::clamp(1 - world.scanWave / 2.3f, 0.f, 1.f) * .8f),
                               false, 2 * zoom);
                for (int i = 0; i < 3; ++i)
                {
                    if (!world.found[i])
                    {
                        canvas.QueueGlow(P(world.MemoryPosition(i), 20), 30, 40, Gold.Fade(.3f));
                    }
                }
            }

            break;
        }
        case EffectKind::Magnet:
        {
            if (world.magnetTime > 0)
            {
                float radius = 35 + std::fmod(world.time * 90, 90.f);
                canvas.Ellipse(P(effect.Position()), radius * zoom, radius * .5f * zoom,
                               Teal.Fade(.4f * (1 - (radius - 35) / 90)), false, 2);
            }

            break;
        }
    }
}
