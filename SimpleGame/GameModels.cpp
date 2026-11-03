#include "stdafx.h"
#include "Game.h"
#include <windows.h>
#include <fstream>
#include <iostream>

namespace
{
    std::wstring ErrorText(const std::string& message)
    {
        const int size = (int)(std::min)(message.size(), size_t{16384});
        const int length = MultiByteToWideChar(CP_UTF8, 0, message.data(), size, nullptr, 0);
        std::wstring text(length, L' ');
        if (length > 0)
        {
            MultiByteToWideChar(CP_UTF8, 0, message.data(), size, text.data(), length);
        }
        return text;
    }

    void ModelFailure(GameWorld& world, const std::string& message, bool notify = true)
    {
        std::cerr << "[LowPoly] " << message << '\n';
        const auto detail = ErrorText(message);
        OutputDebugStringW((L"[LowPoly] " + detail + L"\n").c_str());
        try
        {
            wchar_t directory[32768] = {};
            const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, 32768);
            const auto root = length > 0 && length < 32768 ? std::filesystem::path(directory)
                                                           : std::filesystem::temp_directory_path();
            const auto logs = root / "StarDrifter/Logs";
            std::filesystem::create_directories(logs);
            std::ofstream(logs / "LowPoly.log", std::ios::app) << "[LowPoly] " << message << '\n';
        }
        catch (const std::exception&)
        {
            // Logging must never replace the original loading failure.
        }
        if (notify)
        {
            auto summary = detail.substr(0, detail.find_first_of(L"\r\n"));
            if (summary.size() > 100)
            {
                summary = summary.substr(0, 97) + L"...";
            }
            world.Notify(L"LowPoly 실패: " + summary + L" · F7 재시도");
            world.toastTime = 10;
        }
    }

    std::filesystem::path ArtRoot()
    {
        // Development builds can reload the artist's source files without recopying Resources.
        auto directory = std::filesystem::current_path();
        for (int i = 0; i < 6; ++i)
        {
            auto candidate = directory / "SimpleGame/Resources/Art/LowPoly";
            if (std::filesystem::exists(candidate / "Data/asset_catalog.json"))
            {
                return candidate;
            }
            auto parent = directory.parent_path();
            if (parent == directory)
            {
                break;
            }
            directory = parent;
        }
        return "Resources/Art/LowPoly";
    }
} // namespace

void Game::ToggleModels()
{
    if (modelsEnabled)
    {
        modelsEnabled = false;
        modelsFailed = false;
        for (const auto& entry : modelAttachments)
        {
            if (auto* actor = world.scene.Find<ModelActor>(entry.second))
            {
                actor->SetActive(false);
            }
        }
        world.Notify(L"기존 그래픽 모드 · F8로 LowPoly 모델 표시");
        return;
    }
    try
    {
        if (!modelLibrary)
        {
            const auto root = ArtRoot();
            std::cout << "[LowPoly/root] " << root.u8string() << '\n';
            modelLibrary = std::make_unique<ModelLibrary>(root);
        }
        if (!modelRenderer)
        {
            modelRenderer = std::make_unique<ModelRenderer>();
        }
        modelsEnabled = true;
        modelsFailed = false;
        UpdateModelBindings();
        for (const auto& entry : modelAttachments)
        {
            if (auto* actor = world.scene.Find<ModelActor>(entry.second))
            {
                if (!failedModels.count(entry.first))
                {
                    actor->SetActive(true);
                    actor->ResetMotionTracking();
                }
            }
        }
        world.Notify(L"LowPoly 모델 표시 · F8 기존 그래픽 / F7 추가·변경된 에셋 다시 읽기");
        std::cout << "[LowPoly] " << modelLibrary->AssetIds().size() << " assets, "
                  << modelLibrary->CachedAssetCount() << " cached, " << modelAttachments.size()
                  << " actor bindings, " << failedModels.size() << " failed\n";
    }
    catch (const std::exception& error)
    {
        modelsEnabled = false;
        modelsFailed = true;
        ModelFailure(world, error.what());
    }
}

void Game::ReloadModels()
{
    const bool retry = modelsFailed;
    try
    {
        if (!modelLibrary)
        {
            modelLibrary = std::make_unique<ModelLibrary>(ArtRoot());
        }
        else
        {
            modelLibrary->Reload();
        }
        for (const auto& entry : modelAttachments)
        {
            auto* actor = world.scene.Find<ModelActor>(entry.second);
            if (modelSession == world.monitoring.sessionId && actor &&
                actor->ParentId() == entry.first)
            {
                actor->Destroy();
            }
        }
        world.scene.CollectDestroyed();
        modelAttachments.clear();
        failedModels.clear();
        modelBindingRevision = uint64_t(-1);
        if (modelRenderer)
        {
            modelRenderer->ClearCache();
        }
        if (modelsEnabled)
        {
            UpdateModelBindings();
        }
        else if (retry)
        {
            ToggleModels();
            if (!modelsEnabled)
            {
                return;
            }
        }
        modelsFailed = false;
        world.Notify(L"LowPoly 파일 목록·카탈로그·모델 캐시 갱신 완료");
    }
    catch (const std::exception& error)
    {
        modelsFailed = true;
        ModelFailure(world, std::string("reload: ") + error.what());
    }
}

bool Game::HasModel(ActorId parent) const
{
    if (!modelsEnabled)
    {
        return false;
    }
    auto found = modelAttachments.find(parent);
    const auto* actor =
        found == modelAttachments.end() ? nullptr : world.scene.Find<ModelActor>(found->second);
    return actor && actor->ParentId() == parent && actor->IsVisible();
}

void Game::AttachModel(ActorId parent, const ModelAssembly& assembly, float scale)
{
    const auto* owner = world.scene.Find(parent);
    auto& actor = world.scene.Add(ModelActor(assembly, scale, owner->Layer()), parent);
    actor.Seek((parent % 31) * .037f);
    modelAttachments[parent] = actor.Id();
}

void Game::UpdateModelBindings()
{
    if (!modelsEnabled || !modelLibrary)
    {
        return;
    }
    if (modelSession == world.monitoring.sessionId &&
        modelBindingRevision == world.scene.StructureRevision())
    {
        return;
    }
    if (modelSession != world.monitoring.sessionId)
    {
        modelAttachments.clear();
        failedModels.clear();
        modelSession = world.monitoring.sessionId;
    }
    for (auto it = modelAttachments.begin(); it != modelAttachments.end();)
    {
        if (!world.scene.Find(it->first) || !world.scene.Find<ModelActor>(it->second))
        {
            it = modelAttachments.erase(it);
        }
        else
        {
            ++it;
        }
    }
    auto bind = [&](const Actor& actor, const std::string& key, size_t variant, float width,
                    float depth, bool npc = false)
    {
        if (modelAttachments.count(actor.Id()) || failedModels.count(actor.Id()))
        {
            return;
        }
        try
        {
            const auto* binding = modelLibrary->FindBinding(key);
            if (!binding)
            {
                return;
            }
            const auto id = modelLibrary->Binding(key, variant);
            const auto assembly = npc ? modelLibrary->AssembleNpc(id) : modelLibrary->Single(id);
            const float scale = binding->ScaleFor(*assembly.parts.front(), width, depth);
            AttachModel(actor.Id(), assembly, scale);
        }
        catch (const std::exception& error)
        {
            failedModels.insert(actor.Id());
            ModelFailure(world, "actor " + std::to_string(actor.Id()) + ": " + error.what(), false);
        }
    };
    size_t residents = 0, couriers = 0;
    for (const auto& prop : world.scene.Actors<Prop>())
    {
        if (prop.kind == PropKind::Citizen)
        {
            auto resident = world.planet.residents.find(prop.Id());
            const bool story =
                resident != world.planet.residents.end() && resident->second.storyResident;
            const bool courier =
                resident != world.planet.residents.end() && resident->second.courier;
            const std::string key = story ? "NPC.Story" : courier ? "NPC.Courier" : "NPC.Resident";
            bind(prop, key, story ? 0 : courier ? couriers++ : residents++, 0, 0, true);
            continue;
        }
        const char* names[] = {"Habitat", "Archive", "Tree", "Crate",   "Lamp",
                               "Relay",   "Core",    "Ship", "Citizen", "Memory"};
        const auto key = prop.modelBinding.empty() ? "Prop." + std::string(names[(int)prop.kind])
                                                   : prop.modelBinding;
        const auto position = prop.Position();
        const auto variation = static_cast<uint64_t>(
            static_cast<int64_t>(std::round(position.x * 73 + position.y * 151)));
        bind(prop, key, variation ^ (variation >> 7), prop.Width(), prop.Depth());
    }
    for (const auto& character : world.scene.Actors<Character>())
    {
        if (character.captain)
        {
            bind(character, "NPC.Captain", 0, 0, 0, true);
        }
    }
    std::map<EnemyKind, size_t> enemyVariants;
    for (const auto& enemy : world.scene.Actors<Enemy>())
    {
        const char* kind = enemy.kind == EnemyKind::Boss     ? "Boss"
                           : enemy.kind == EnemyKind::Sentry ? "Sentry"
                                                             : "Drone";
        float size = enemy.kind == EnemyKind::Boss ? 2.6f : 1.1f;
        // Do not use ActorId modulo: enemy IDs advance by two because each owns a warning actor.
        bind(enemy, "Enemy." + std::string(kind), enemyVariants[enemy.kind]++, size, size);
    }
    for (const auto& effect : world.scene.Actors<SceneEffect>())
    {
        if (effect.kind == EffectKind::LandingPad)
        {
            bind(effect, "Effect.LandingPad", 0, 0, 0);
        }
    }
    modelBindingRevision = world.scene.StructureRevision();
}

void Game::Draw(const ModelActor& actor)
{
    if (!modelsEnabled || !modelRenderer)
    {
        return;
    }
    try
    {
        const auto* prop = world.scene.Find<Prop>(actor.ParentId());
        const auto* enemy = world.scene.Find<Enemy>(actor.ParentId());
        const auto* character = world.scene.Find<Character>(actor.ParentId());
        float opacity = 1;
        if (prop && (prop->kind == PropKind::Habitat || prop->kind == PropKind::Archive) &&
            GameWorld::Project(prop->Position()).y > GameWorld::Project(world.PlayerPosition()).y)
        {
            const auto& asset = *actor.parts.front().asset;
            const float height = (asset.maximum.y - asset.minimum.y) * actor.scale * 42;
            const Vec2 foot = P(actor.Position()), player = P(world.PlayerPosition(), 35);
            if (std::abs(player.x - foot.x) < (prop->Width() + prop->Depth()) * 23 * zoom &&
                player.y > foot.y - (height + 50) * zoom && player.y < foot.y + 20 * zoom)
            {
                opacity = .35f;
            }
        }
        if (character || enemy || (prop && prop->kind == PropKind::Citizen))
        {
            canvas.Ellipse(P(actor.Position()), 18 * zoom, 6 * zoom, Color(0x031419, .4f));
        }
        modelRenderer->Draw(actor, P(actor.Position()), zoom, canvas, opacity);
        // Indicators must be submitted after the mesh, otherwise the model covers them.
        if (prop && prop->kind == PropKind::Citizen)
        {
            DrawCitizenIndicators(*prop);
        }
        if (enemy && enemy->kind != EnemyKind::Boss)
        {
            const auto& asset = *actor.parts.front().asset;
            const float height = (asset.maximum.y - asset.minimum.y) * actor.scale * 42;
            const Vec2 hp = P(actor.Position(), height + 12);
            canvas.Rect(hp.x - 22 * zoom, hp.y, 44 * zoom, 4 * zoom, Color(0x08151c));
            canvas.Rect(hp.x - 22 * zoom, hp.y,
                        44 * zoom * (std::max)(0.f, enemy->health / enemy->maxHealth), 4 * zoom,
                        Color(0xf17d7c));
        }
    }
    catch (const std::exception& error)
    {
        ModelFailure(world, std::string("draw: ") + error.what(), false);
        if (auto* failed = world.scene.Find<ModelActor>(actor.Id()))
        {
            failed->SetActive(false);
        }
        failedModels.insert(actor.ParentId());
    }
}
