#include "stdafx.h"
#include "Game.h"
#include "Dependencies/freeglut.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <queue>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace
{
    void Require(bool ok, const char* message)
    {
        if (!ok)
        {
            throw std::runtime_error(message);
        }
    }

    class RecordingActorRenderer : public ActorRenderer
    {
    public:

        std::vector<ActorId> order;

        void Draw(const Prop& actor) override
        {
            order.push_back(actor.Id());
        }

        void Draw(const Character& actor) override
        {
            order.push_back(actor.Id());
        }

        void Draw(const Terrain& actor) override
        {
            order.push_back(actor.Id());
        }

        void Draw(const SceneEffect& actor) override
        {
            order.push_back(actor.Id());
        }

        void Draw(const Enemy& actor) override
        {
            order.push_back(actor.Id());
        }

        void Draw(const Projectile& actor) override
        {
            order.push_back(actor.Id());
        }

        void Draw(const Loot& actor) override
        {
            order.push_back(actor.Id());
        }

        void Draw(const CombatText& actor) override
        {
            order.push_back(actor.Id());
        }
    };

    void VerifySceneGraph()
    {
        SceneGraph scene;
        auto& parent = scene.Add(Actor("Parent", {10, 20}));
        auto& child = scene.Add(Prop(PropKind::Crate, {1, 2}, 1, 1, 20), parent.Id());
        ActorId childId = child.Id(), parentId = parent.Id();
        auto same = [](Vec2 a, Vec2 b)
        {
            return GameWorld::Distance(a, b) < .001f;
        };
        Require(same(child.Position(), {11, 22}), "Parent translation was not inherited");
        uint64_t revision = scene.NavigationRevision();
        parent.SetPosition({12, 23});
        Require(same(child.Position(), {13, 25}) && scene.NavigationRevision() > revision,
                "Moving a collider group did not update position/navigation");
        child.SetPosition({20, 30});
        Require(same(child.LocalPosition(), {8, 7}), "World position was stored as local position");
        auto& other = scene.Add(Actor("Other", {-2, 1}));
        Require(scene.Reparent(childId, other.Id()) && same(child.Position(), {20, 30}),
                "Reparenting failed to preserve world position");
        Require(!scene.Reparent(other.Id(), childId), "Scene graph accepted a cycle");
        Require(!scene.Reparent(childId, childId) && !scene.Reparent(childId, 999999),
                "Scene graph accepted a self or missing parent");
        Vec2 local = child.LocalPosition();
        Require(scene.Reparent(childId, parentId, false) && same(child.LocalPosition(), local),
                "Local-preserving reparent changed the local transform");

        auto& text = scene.Add(CombatText({}, L"lifetime", 0xffffff, 1), childId);
        ActorId textId = text.Id();
        parent.SetVisible(false);
        Require(!child.IsVisible() && child.IsActive(), "Visibility incorrectly disabled logic");
        parent.SetVisible(true);
        parent.SetActive(false);
        scene.Update(.25f);
        Require(!child.IsActive() && !child.IsVisible() && text.life == 1 &&
                    scene.Actors<Prop>().empty() && scene.Actors<Prop>(true).size() == 1,
                "Inactive ancestors did not suppress traversal/update");
        parent.SetActive(true);
        scene.Update(.25f);
        Require(text.life == .75f, "Actor virtual Update was not dispatched");

        SceneGraph copy = scene;
        copy.Find(parentId)->SetPosition({0, 0});
        Require(!same(copy.Find(childId)->Position(), child.Position()) &&
                    copy.Find<CombatText>(textId)->life == .75f,
                "Scene graph copy shares nodes or loses derived state");
        SceneGraph moved = std::move(copy);
        Require(moved.Find(childId)->ParentId() == parentId, "Scene move broke parent links");
        auto retained = scene.Actors<Prop>();
        scene.Destroy(parentId);
        Require(!scene.Find(childId) && !scene.Find(textId), "Destroy did not mark descendants");
        scene.CollectDestroyed();
        Require(scene.Actors<Prop>(true).empty() && retained[0].IsPendingDestroy(),
                "Deferred destruction invalidated a live snapshot");
        scene.Update(.1f);

        SceneGraph renderScene;
        ActorId front = renderScene.Add(Prop(PropKind::Crate, {3, 3}, 1, 1, 10)).Id();
        ActorId back = renderScene.Add(Prop(PropKind::Crate, {1, 1}, 1, 1, 10)).Id();
        ActorId shot = renderScene.Add(Projectile({-20, -20})).Id();
        ActorId overlay = renderScene.Add(CombatText({}, L"text")).Id();
        RecordingActorRenderer renderer;
        renderScene.Draw(renderer);
        Require(renderer.order == std::vector<ActorId>{back, front, shot, overlay},
                "Scene renderer did not respect layer/depth order");
        renderScene.Find(back)->SetVisible(false);
        renderer.order.clear();
        renderScene.Draw(renderer);
        Require(renderer.order == std::vector<ActorId>{front, shot, overlay},
                "Hidden actor was drawn");
        renderScene.Update(2);
        Require(!renderScene.Find(overlay), "Expired actor was not collected");
    }

    void VerifyActorWorld()
    {
        GameWorld world(42);
        world.Start();
        world.bossDefeated = true;
        world.scene.Clear<Enemy>();
        Vec2 player = world.PlayerPosition();
        Require(world.Walkable(player), "Actor collision test starts on blocked floor");
        auto& prop = world.Spawn(Prop(PropKind::Crate, player, .5f, .5f, 10));
        ActorId propId = prop.Id();
        Require(!world.Walkable(player), "Spawned collider did not invalidate the collision cache");
        prop.SetSolid(false);
        Require(world.Walkable(player), "Disabling solidity retained a cached collider");
        prop.SetSolid(true);
        Require(!world.Walkable(player), "Enabling solidity failed to restore the collider");
        world.scene.Find(world.roots.environment)->SetPosition({50, 50});
        Require(world.Walkable(player), "Parent movement did not move descendant colliders");
        world.scene.Find(world.roots.environment)->SetPosition({});
        world.scene.Destroy(propId);
        Require(world.Walkable(player), "Destroyed actor still blocked movement");
        world.scene.Find(world.roots.terrain)->SetActive(false);
        Require(!world.Walkable(player), "Inactive terrain still supplied a walkable floor");
        world.scene.Find(world.roots.terrain)->SetActive(true);

        world.scene.Find(world.roots.projectiles)->SetPosition({4, 5});
        auto& bullet = world.Spawn(Projectile(player, {1, 0}));
        ActorId bulletId = bullet.Id();
        Require(GameWorld::Distance(bullet.Position(), player) < .001f,
                "World Spawn double-applied the parent transform");
        world.scene.Find(world.roots.projectiles)->SetActive(false);
        world.scene.Find(world.roots.characters)->SetActive(false);
        world.firing = true;
        world.Move({1, 0}, .1f, false);
        world.DamagePlayer(20);
        world.Update(.1f);
        Require(GameWorld::Distance(world.PlayerPosition(), player) < .001f &&
                    GameWorld::Distance(world.scene.Find(bulletId)->Position(), player) < .001f &&
                    world.shotsFired == 0 && world.stats.health == 100,
                "Inactive graph branches still moved, fired, or took damage");
        world.scene.Find(world.roots.characters)->SetActive(true);
        GameWorld clone = world;
        clone.SetPlayerPosition(player + Vec2{1, 0});
        Require(GameWorld::Distance(world.PlayerPosition(), player) < .001f,
                "GameWorld copy still shares character nodes");
    }

    void Capture(Game& game, int width, int height, const std::filesystem::path& file)
    {
        game.Render();
        glFinish();
        Require(glGetError() == GL_NO_ERROR, "OpenGL error while rendering verification frame");
        std::vector<unsigned char> pixels(width * height * 4);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
        Require(glGetError() == GL_NO_ERROR, "OpenGL framebuffer readback failed");
        int lit = 0;
        for (size_t i = 0; i < pixels.size(); i += 4)
        {
            if (pixels[i] + pixels[i + 1] + pixels[i + 2] > 70)
            {
                ++lit;
            }
        }
        Require(lit > width * height / 20, "Rendered framebuffer is blank or nearly black");
        BITMAPFILEHEADER header = {};
        header.bfType = 0x4d42;
        header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        header.bfSize = header.bfOffBits + (DWORD)pixels.size();
        BITMAPINFOHEADER info = {};
        info.biSize = sizeof(info);
        info.biWidth = width;
        info.biHeight = height;
        info.biPlanes = 1;
        info.biBitCount = 32;
        info.biCompression = BI_RGB;
        std::ofstream out(file, std::ios::binary);
        out.write((char*)&header, sizeof(header));
        out.write((char*)&info, sizeof(info));
        out.write((char*)pixels.data(), pixels.size());
        Require(out.good(), "Could not write verification screenshot");
    }

    void VerifyMeshCache(Renderer& renderer)
    {
        Canvas canvas;
        Require(canvas.CachedMeshCount() == 0, "Canvas meshes should be created lazily");
        renderer.BeginSceneCapture();
        glDisable(GL_DEPTH_TEST);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        canvas.Rect(20, 20, 40, 40, Color(0xff0000, .5f));
        canvas.Ellipse({40, 40}, 12, 12, Color(0x0000ff, .5f));
        canvas.Flush();
        unsigned char pixel[4] = {};
        glReadPixels(40, (int)Canvas::Height - 41, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        Require(std::abs((int)pixel[0] - 64) <= 3 && pixel[1] <= 3 &&
                    std::abs((int)pixel[2] - 128) <= 3,
                "Mesh batching changed transparent draw order");

        for (int pass = 0; pass < 3; ++pass)
        {
            float scale = 1 + pass * .3f;
            canvas.Triangle({200, 20}, {220, 20}, {210, 40}, Color(0xffffff));
            canvas.Rect(200, 60, 20 * scale, 20, Color(0xffcc88));
            canvas.Line({200, 100}, {230, 110}, Color(0xffffff), 3);
            canvas.Ellipse({280, 40}, 20 * scale, 7 * scale, Color(0x88ccff));
            canvas.Ellipse({280, 90}, 20 * scale, 7 * scale, Color(0x88ccff), false, 2);
            canvas.Glow({340, 50}, 22 * scale, 30, Color(0x88ffaa, .5f));
            canvas.Flush();
            Require(canvas.CachedMeshCount() == 5,
                    "Repeated shapes created size/color-dependent mesh cache entries");
        }
        // More than one instance-buffer batch must not create any new topology.
        for (int i = 0; i < 5000; ++i)
        {
            canvas.Rect(400 + (i % 100) * 2.f, 20 + (i / 100) * 2.f, 1, 1, Color(0xffffff));
        }
        canvas.Flush();
        Require(canvas.CachedMeshCount() == 5, "Instance batch overflow recreated mesh data");
        Require(glGetError() == GL_NO_ERROR,
                "Cached mesh / instance rendering produced a GL error");
        renderer.EndSceneCaptureAndComposite();
        Require(glGetError() == GL_NO_ERROR,
                "Canvas instance attributes leaked into post-processing");
    }

    void FinishDialogue(GameWorld& w)
    {
        for (int n = 0; n < 20 && !w.dialogue.empty(); ++n)
        {
            w.Advance();
        }
    }

    // Routes use the same collision-tested movement function as keyboard input.
    void WalkTo(GameWorld& w, Vec2 target, float reach = 1.3f)
    {
        const int nx = 129, ny = 97;
        const float x0 = -11, y0 = -18, step = .25f;
        auto point = [&](int n)
        {
            return Vec2{x0 + (n % nx) * step, y0 + (n / nx) * step};
        };
        auto index = [&](Vec2 p)
        {
            return (int)std::round((p.y - y0) / step) * nx + (int)std::round((p.x - x0) / step);
        };
        std::vector<int> parent(nx * ny, -1);
        std::queue<int> q;
        int start = index(w.PlayerPosition()), end = -1;
        Require(start >= 0 && start < nx * ny, "Invalid route start");
        q.push(start);
        parent[start] = start;
        while (!q.empty())
        {
            int cur = q.front();
            q.pop();
            Vec2 p = point(cur);
            if (GameWorld::Distance(p, target) < reach)
            {
                end = cur;
                break;
            }
            for (int d : {1, -1, nx, -nx})
            {
                int n = cur + d;
                if (n < 0 || n >= nx * ny || parent[n] != -1)
                {
                    continue;
                }
                if (std::abs((n % nx) - (cur % nx)) + std::abs(n / nx - cur / nx) != 1)
                {
                    continue;
                }
                if (!w.ClearPath(point(cur), point(n), .27f))
                {
                    continue;
                }
                parent[n] = cur;
                q.push(n);
            }
        }
        Require(end >= 0, "A quest or memory target is unreachable");
        std::vector<int> path;
        for (int n = end; n != start; n = parent[n])
        {
            path.push_back(n);
        }
        std::reverse(path.begin(), path.end());
        for (int n : path)
        {
            Vec2 goal = point(n);
            int guard = 0;
            while (GameWorld::Distance(w.PlayerPosition(), goal) > .025f && guard++ < 100)
            {
                Vec2 d = GameWorld::Project(goal) - GameWorld::Project(w.PlayerPosition());
                float remaining = GameWorld::Distance(d, {});
                float dt = (std::min)(1 / 120.f, remaining / w.stats.MoveSpeed());
                w.Move(d, dt, false);
                w.Update(dt);
                Require(w.Walkable(w.PlayerPosition()), "Player penetrated a collider");
            }
            if (guard >= 100)
            {
                throw std::runtime_error("Movement stalled near target " +
                                         std::to_string(target.x) + "," + std::to_string(target.y) +
                                         " at " + std::to_string(w.PlayerPosition().x) + "," +
                                         std::to_string(w.PlayerPosition().y));
            }
        }
        w.Move({}, 0, false);
        w.Update(.016f);
        for (int i = 0;
             i < 180 && GameWorld::Distance(w.PlayerPosition(), w.CaptainPosition()) > 1.4f; ++i)
        {
            w.Update(1 / 60.f);
        }
        Require(GameWorld::Distance(w.PlayerPosition(), w.CaptainPosition()) < 2.f,
                "Companion failed to follow the collision-tested route");
    }
} // namespace

int RunPrototypeVerification(Game& game, Renderer& renderer, int width, int height,
                             const std::filesystem::path& output)
{
    std::filesystem::create_directories(output);
    std::ofstream report(output / "verification.txt");
    try
    {
        // Opt-in only: this suite is run by the user with --verify, never at normal startup.
        VerifySceneGraph();
        VerifyActorWorld();
        report << "PASS actor hierarchy, reparenting, activity, lifetime, copy, render order, "
                  "collision\n";
        VerifyMeshCache(renderer);
        report
            << "PASS lazy GPU mesh cache, reuse across transforms, alpha order, instance batches\n";
        for (uint32_t seed = 1; seed <= 64; ++seed)
        {
            GameWorld generated(seed);
            Require(generated.ValidateNavigation(), "Generated navigation is disconnected");
            Require(generated.scene.Actors<Enemy>().size() >= 12, "A farming camp failed to spawn");
            for (const auto& enemy : generated.scene.Actors<Enemy>())
            {
                Require(generated.Walkable(enemy.Position()), "Enemy spawned inside a collider");
                Require(GameWorld::Distance(GameWorld::Project(enemy.Position()),
                                            GameWorld::Project(generated.PlayerPosition())) > 240,
                        "Enemy spawned in the landing safety zone");
            }
        }
        GameWorld sameA(42), sameB(42), different(43);
        Require(sameA.districts.size() == sameB.districts.size(), "Seed is not reproducible");
        bool changed = false;
        for (size_t i = 0; i < sameA.districts.size(); ++i)
        {
            const auto& a = sameA.districts[i];
            const auto& b = sameB.districts[i];
            const auto& c = different.districts[i];
            Require(a.x0 == b.x0 && a.x1 == b.x1 && a.y0 == b.y0 && a.y1 == b.y1,
                    "Same seed generated different rooms");
            changed = changed || a.x0 != c.x0 || a.y0 != c.y0 || a.x1 != c.x1 || a.y1 != c.y1;
        }
        Require(changed, "Distinct seeds did not vary the layout");
        report << "PASS 64 seeds: connected navigation, safe spawn, farming camps; deterministic "
                  "seed\n";

        GameWorld growth(42);
        float damage = growth.stats.Damage(), interval = growth.stats.ShotInterval();
        float health = growth.stats.MaxHealth();
        growth.GainExperience(59);
        Require(growth.stats.level == 1 && growth.stats.experience == 59,
                "XP below threshold failed");
        growth.GainExperience(6);
        Require(growth.stats.level == 2 && growth.stats.experience == 5, "XP overflow was lost");
        Require(growth.stats.Damage() > damage && growth.stats.ShotInterval() < interval &&
                    growth.stats.MaxHealth() > health,
                "Level growth does not affect combat stats");
        growth.GainExperience(2000000000);
        growth.GainExperience(2000000000);
        Require(growth.stats.level == 80 && growth.stats.experience == 0 &&
                    growth.stats.NextLevelExperience() == 0 && growth.stats.ShotInterval() >= .11f,
                "Level 80 cap or minimum cooldown failed");
        report << "PASS XP thresholds, overflow, combat growth, level 80 cap\n";

        GameWorld combat(42);
        combat.Start();
        combat.scene.Clear<Enemy>();
        combat.bossDefeated = true; // Isolate projectile and pickup checks from reinforcements.
        combat.Spawn(Projectile{combat.PlayerPosition(), {1, 0}, 400, 40, 12, false, false});
        combat.Update(.05f);
        Require(combat.scene.Actors<Projectile>().size() == 1 &&
                    combat.scene.Actors<Projectile>()[0].remaining <= 20.1f,
                "Projectile range is not consumed by distance");
        combat.Update(.05f);
        Require(combat.scene.Actors<Projectile>().empty(), "Projectile exceeded maximum range");
        combat.firing = true;
        combat.Update(.01f);
        int shots = combat.shotsFired;
        combat.Update(.01f);
        Require(shots == 1 && combat.shotsFired == shots, "Shot cooldown was bypassed");
        combat.firing = false;
        combat.scene.Clear<Projectile>();
        combat.DamagePlayer(20);
        float damaged = combat.stats.health;
        combat.DamagePlayer(20);
        Require(combat.stats.health == damaged, "Damage immunity was bypassed");
        combat.AddLoot(LootKind::Health, combat.PlayerPosition());
        combat.AddLoot(LootKind::Weapon, combat.PlayerPosition());
        combat.AddLoot(LootKind::Soul, combat.PlayerPosition(), 60);
        combat.AddLoot(LootKind::Magnet, combat.PlayerPosition());
        combat.Update(.016f);
        Require(combat.stats.health > damaged && combat.stats.weaponTier == 1 &&
                    combat.stats.level == 2 && combat.magnetTime > 0 && combat.itemsCollected == 4,
                "One or more pickup effects failed");
        combat.AddLoot(LootKind::Weapon, combat.PlayerPosition(), 30);
        combat.Update(.016f);
        Require(combat.stats.weaponTier == 20, "Weapon enhancement cap failed");
        Vec2 farLoot = combat.PlayerPosition() + Vec2{1, -1};
        combat.AddLoot(LootKind::Soul, farLoot, 5);
        float previousDistance = GameWorld::Distance(GameWorld::Project(farLoot),
                                                     GameWorld::Project(combat.PlayerPosition()));
        combat.Update(.05f);
        Require(
            !combat.scene.Actors<Loot>().empty() && combat.scene.Actors<Loot>()[0].attracted &&
                GameWorld::Distance(GameWorld::Project(combat.scene.Actors<Loot>()[0].Position()),
                                    GameWorld::Project(combat.PlayerPosition())) < previousDistance,
            "Magnet did not attract a distant item");
        combat.view = View::Map;
        float pausedTime = combat.time, pausedMagnet = combat.magnetTime;
        combat.Update(1);
        Require(combat.time == pausedTime && combat.magnetTime == pausedMagnet,
                "Map did not pause combat timers");
        report
            << "PASS finite range, firing cooldown, damage immunity, four drops, magnet, pause\n";

        // A direct collision regression: solid scenery stops shots before they reach an enemy.
        GameWorld wall(42);
        wall.Start();
        wall.scene.Clear<Enemy>();
        wall.SetPlayerPosition({-5, 1});
        wall.Spawn(Prop{PropKind::Crate, {-4.5f, .5f}, .2f, .2f, 20});
        wall.Spawn(Projectile{wall.PlayerPosition(), {1, 0}, 660, 350, 12, false, false});
        wall.Update(.05f);
        wall.Update(.05f);
        Require(wall.scene.Actors<Projectile>().empty(),
                "Projectile tunneled through solid scenery");

        game.world = GameWorld(42);
        auto& w = game.world;
        game.SnapCamera();
        game.ClearInput();
        Capture(game, width, height, output / "01-title.bmp");
        game.MouseButton(200, 610, true);
        game.MouseButton(200, 610, false);
        game.Tick(.016f);
        Require(w.view == View::Explore && w.shotsFired == 0, "Title click fired into gameplay");
        game.MouseButton(900, 450, true);
        game.MouseButton(900, 450, false);
        game.Tick(.016f);
        Require(w.shotsFired == 1, "A short click between ticks was lost");
        game.MouseButton(900, 450, true);
        game.Key('m', true);
        game.Tick(.016f);
        game.Key('m', true);
        for (int i = 0; i < 40; ++i)
        {
            game.Tick(.016f);
        }
        Require(w.shotsFired == 1, "Held fire leaked through map closing");
        game.MouseButton(-10, 450, true);
        game.Tick(.016f);
        Require(w.shotsFired == 1, "Letterbox input fired into gameplay");
        Capture(game, width, height, output / "02-farming-hud.bmp");
        w.view = View::Map;
        Capture(game, width, height, output / "03-random-map.bmp");
        w.view = View::Explore;
        report << "PASS UI click consumption, short click, release, map fire reset, letterbox "
                  "rejection\n";

        // Navigation/story checks intentionally isolate combat; boss damage is tested separately below.
        w.scene.Clear<Enemy>();
        w.scene.Clear<Projectile>();
        w.bossDefeated = true;
        for (int i = 0; i < 3; ++i)
        {
            WalkTo(w, w.MemoryPosition(i));
            w.Interact();
            FinishDialogue(w);
            Require(w.found[i], "Memory is not interactable on the random map");
            WalkTo(w, w.RelayPosition(i));
            w.Interact();
            FinishDialogue(w);
            Require(w.restored[i], "Relay is not interactable on the random map");
        }
        Require(w.quest == 2, "Three relays did not unlock the boss objective");
        w.bossDefeated = false;
        WalkTo(w, w.CorePosition(), 1.2f);
        w.Interact();
        Require(w.bossActive && !w.choicePending, "Boss did not gate the final story choice");
        size_t count = w.scene.Actors<Enemy>().size();
        w.BeginBoss();
        Require(w.scene.Actors<Enemy>().size() == count, "Boss was spawned twice");
        w.SetPlayerPosition(w.bossHome + GameWorld::Unproject({-110, 0}));
        Require(w.Walkable(w.PlayerPosition()), "Boss test firing position is obstructed");
        w.invulnerability = 100;
        w.stats.weaponTier = 20;
        w.aim = {1, 0};
        game.SnapCamera();
        Capture(game, width, height, output / "04-boss.bmp");
        for (int i = 0; i < 1500 && !w.bossDefeated; ++i)
        {
            w.firing = true;
            w.Update(.016f);
        }
        w.firing = false;
        Require(w.bossDefeated && !w.bossActive && w.kills > 0,
                "Actual projectiles did not defeat boss");
        Require(!w.scene.Actors<Loot>().empty(), "Boss did not drop rewards");
        w.BeginBoss();
        Require(w.scene.Actors<Enemy>().empty(), "Defeated boss respawned");
        w.SetPlayerPosition(w.CorePosition() + Vec2{1, 0});
        w.trail.clear();
        w.SetCaptainPosition(w.PlayerPosition() + Vec2{.5f, .5f});
        w.Interact();
        FinishDialogue(w);
        Require(w.choicePending && w.view == View::Dialogue, "Post-boss choice did not open");
        Capture(game, width, height, output / "05-choice.bmp");
        GameWorld alternate = w;
        alternate.Choose(1);
        FinishDialogue(alternate);
        Require(alternate.quest == 3 && alternate.choice == 1, "Gradual opening branch failed");
        w.Choose(0);
        FinishDialogue(w);
        WalkTo(w, w.ShipPosition(), 2.4f);
        w.Interact();
        Require(w.view == View::Ending, "Return to ship did not finish level one");
        Capture(game, width, height, output / "06-ending.bmp");
        report << "PASS random-map routes, companion, memories, relays, boss damage/reward, both "
                  "choices\n";

        game.world = GameWorld(42);
        w.Start();
        w.invulnerability = 0;
        w.DamagePlayer(9999);
        Require(w.view == View::Defeat && !w.firing, "Player death did not stop exploration");
        Capture(game, width, height, output / "07-defeat.bmp");
        uint32_t seed = w.mapSeed;
        game.Key('r', true);
        Require(w.view == View::Explore && w.mapSeed == seed && w.stats.level == 1 &&
                    w.stats.weaponTier == 0 && w.kills == 0 && !w.bossActive && !w.bossDefeated &&
                    w.scene.Actors<Loot>().empty() && w.scene.Actors<Projectile>().empty(),
                "Retry leaked state or changed the requested seed");
        glutReshapeWindow(1000, 740);
        glutMainLoopEvent();
        renderer.SetOutputSize(1000, 740);
        Capture(game, 1000, 740, output / "08-resize.bmp");
        report << "PASS defeat, same-seed clean retry, non-16:10 rendering\nALL CHECKS PASSED\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        report << "FAIL: " << e.what() << "\n";
        report.flush();
        throw;
    }
}
