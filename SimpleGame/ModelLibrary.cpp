#include "stdafx.h"
#include "ModelLibrary.h"
#include "Dependencies/json.hpp"
#include <charconv>
#include <fstream>
#include <set>
#include <stdexcept>

namespace
{
    nlohmann::json Json(const std::filesystem::path& path)
    {
        std::ifstream input(path);
        if (!input)
        {
            throw std::runtime_error("Cannot open metadata: " + path.u8string());
        }
        return nlohmann::json::parse(input);
    }

    void RequireSchema(const nlohmann::json& data, const std::filesystem::path& path,
                       unsigned major)
    {
        const auto version = data.at("schema_version").get<std::string>();
        if (!ModelLibrary::SupportsSchemaVersion(version, major))
        {
            throw std::runtime_error("Unsupported LowPoly schema " + version + " in " +
                                     path.u8string() + "; expected " + std::to_string(major) +
                                     ".x metadata");
        }
    }
} // namespace

bool ModelLibrary::SupportsSchemaVersion(const std::string& version, unsigned major)
{
    size_t start = 0;
    for (int component = 0; component < 3; ++component)
    {
        const size_t dot = version.find('.', start);
        const size_t end = dot == std::string::npos ? version.size() : dot;
        unsigned value = 0;
        const auto parsed = std::from_chars(version.data() + start, version.data() + end, value);
        if (start == end || parsed.ec != std::errc{} || parsed.ptr != version.data() + end ||
            (component == 0 && value != major))
        {
            return false;
        }
        if (component == 2)
        {
            return dot == std::string::npos;
        }
        if (dot == std::string::npos)
        {
            return false;
        }
        start = dot + 1;
    }
    return false;
}

float ModelBinding::ScaleFor(const ModelAsset& asset, float width, float depth) const
{
    // Authored origins are not necessarily bounding-box centers (e.g. the ship/workshop).
    // Fit both sides without recentering the mesh or extending beyond the collision rectangle.
    const float x = 2 * (std::max)(std::abs(asset.minimum.x), std::abs(asset.maximum.x));
    const float z = 2 * (std::max)(std::abs(asset.minimum.z), std::abs(asset.maximum.z));
    if (fitFootprint && width > 0 && depth > 0 && x > .001f && z > .001f)
    {
        return scale * (std::min)(width / x, depth / z);
    }
    return scale;
}

ModelLibrary::ModelLibrary(std::filesystem::path root)
    : root(std::filesystem::weakly_canonical(root))
{
    Reload();
}

std::filesystem::path ModelLibrary::SafePath(const std::filesystem::path& relative) const
{
    if (relative.is_absolute())
    {
        throw std::runtime_error("Absolute asset paths are forbidden");
    }
    const auto path = std::filesystem::weakly_canonical(root / relative);
    auto rel = path.lexically_relative(root);
    if (rel.empty() || *rel.begin() == "..")
    {
        throw std::runtime_error("Asset escapes LowPoly root");
    }
    return path;
}

void ModelLibrary::Reload()
{
    // Publish only a complete registry. A partially written artist JSON leaves the old registry usable.
    const auto catalog = Json(root / "Data/asset_catalog.json");
    const auto rigNpcFile = root / "Rigged/Data/npc_rig_bindings.json";
    const bool rigNpc = std::filesystem::exists(rigNpcFile);
    const auto npcData = Json(rigNpc ? rigNpcFile : root / "Data/npc_presets.json");
    const auto paletteData = Json(root / "Data/palettes.json");
    RequireSchema(catalog, root / "Data/asset_catalog.json", 1);
    RequireSchema(npcData, rigNpc ? rigNpcFile : root / "Data/npc_presets.json", rigNpc ? 2 : 1);
    RequireSchema(paletteData, root / "Data/palettes.json", 1);
    if (npcData.at("none_index") != -1 || catalog.at("units") != "meters" ||
        catalog.at("glb_axes").at("up") != "+Y" || catalog.at("glb_axes").at("front") != "+Z")
    {
        throw std::runtime_error("Unsupported LowPoly catalog units, axes or schema");
    }
    decltype(files) newFiles;
    decltype(slots) newSlots;
    decltype(presets) newPresets;
    decltype(palettes) newPalettes;
    decltype(materialRoles) newRoles;
    decltype(bindings) newBindings;
    decltype(assetRigs) newAssetRigs;
    decltype(rigMotions) newRigMotions;
    const auto rigCatalogFile = root / "Rigged/Data/rigged_asset_catalog.json";
    if (std::filesystem::exists(rigCatalogFile))
    {
        const auto rigCatalog = Json(rigCatalogFile);
        // Minor art updates (2.1 adds digitigrade hind legs) retain the 2.x metadata contract.
        RequireSchema(rigCatalog, rigCatalogFile, 2);
        for (const auto& row : rigCatalog.at("npc_parts"))
        {
            newAssetRigs[row.at("asset_id").get<std::string>()] =
                row.at("rig_id").get<std::string>();
        }
        for (const auto& row : rigCatalog.at("mobs"))
        {
            newAssetRigs[row.at("mesh_name").get<std::string>()] =
                row.at("rig_name").get<std::string>();
        }
    }
    const auto clipFile = root / "Rigged/Data/animation_clips.json";
    if (std::filesystem::exists(clipFile))
    {
        const auto clips = Json(clipFile);
        RequireSchema(clips, clipFile, 2);
        for (const auto& row : clips.at("clips"))
        {
            ModelMotion motion;
            motion.loop = row.at("loop").get<bool>();
            motion.suggestedSpeed = row.value("suggested_speed_mps", 0.f);
            motion.headingDegrees = row.value("heading_delta_degrees", 0.f);
            if (!std::isfinite(motion.suggestedSpeed) || motion.suggestedSpeed < 0 ||
                !std::isfinite(motion.headingDegrees))
            {
                throw std::runtime_error("Invalid clip motion metadata");
            }
            newRigMotions[row.at("rig_id").get<std::string>()]
                         [row.at("clip_id").get<std::string>()] = motion;
        }
    }
    const auto bindingFile = root.parent_path().parent_path() / "LowPolyBindings.json";
    if (std::filesystem::exists(bindingFile))
    {
        const auto data = Json(bindingFile);
        const int version = data.at("schema_version").get<int>();
        if (version != 1 && version != 2)
        {
            throw std::runtime_error("Unsupported binding schema");
        }
        for (const auto& row : data.at("bindings").items())
        {
            ModelBinding binding;
            if (row.value().is_string())
            {
                binding.assets.push_back(row.value().get<std::string>());
                binding.fitFootprint = true;
            }
            else
            {
                binding.assets = row.value().at("assets").get<std::vector<std::string>>();
                binding.scale = row.value().value("scale", 1.f);
                binding.fitFootprint = row.value().value("fit_footprint", false);
            }
            if (binding.assets.empty() || !std::isfinite(binding.scale) || binding.scale <= 0 ||
                (binding.fitFootprint && binding.scale > 1))
            {
                throw std::runtime_error("Invalid model binding: " + row.key());
            }
            newBindings.emplace(row.key(), std::move(binding));
        }
    }
    for (const auto& row : catalog.at("assets"))
    {
        const auto id = row.at("asset_id").get<std::string>();
        if (!newFiles
                 .emplace(id, SafePath(std::filesystem::u8path(row.at("file").get<std::string>())))
                 .second)
        {
            throw std::runtime_error("Duplicate asset ID: " + id);
        }
    }
    for (const auto& slot : catalog.at("slots").items())
    {
        for (const auto& row : slot.value())
        {
            int index = row.at("index").get<int>();
            auto id = row.at("asset_id").get<std::string>();
            if (index < 0 || !newFiles.count(id) || !newSlots[slot.key()].emplace(index, id).second)
            {
                throw std::runtime_error("Invalid slot-local part index");
            }
        }
    }
    for (const char* slot : {"body", "hair", "top", "bottom", "footwear", "accessory", "headgear"})
    {
        if (!newSlots.count(slot))
        {
            throw std::runtime_error("Missing NPC assembly slot");
        }
    }
    // New independent GLBs need no hard-coded C++ registration. Rigged versions override matching IDs.
    for (const auto& folder :
         {std::filesystem::path("Meshes"), std::filesystem::path("Rigged/Meshes")})
    {
        if (!std::filesystem::exists(root / folder))
        {
            continue;
        }
        std::vector<std::filesystem::path> paths;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root / folder))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".glb")
            {
                paths.push_back(entry.path());
            }
        }
        std::sort(paths.begin(), paths.end());
        std::set<std::string> seen;
        for (const auto& path : paths)
        {
            std::string id = path.stem().u8string();
            if (!seen.insert(id).second)
            {
                throw std::runtime_error("Duplicate GLB basename: " + id);
            }
            newFiles[id] = SafePath(path.lexically_relative(root));
        }
    }
    for (const auto& row : npcData.at("npc_presets"))
    {
        Preset preset;
        preset.parts = row.at("part_indices").get<decltype(preset.parts)>();
        preset.palettes = row.at("palette_indices").get<decltype(preset.palettes)>();
        if (!newPresets.emplace(row.at("npc_id").get<std::string>(), std::move(preset)).second)
        {
            throw std::runtime_error("Duplicate NPC preset ID");
        }
    }
    newRoles = paletteData.at("override_material_roles").get<decltype(newRoles)>();
    for (const auto& palette : paletteData.at("palettes").items())
    {
        for (const auto& row : palette.value())
        {
            auto rgb = row.at("base_color_linear").get<std::vector<float>>();
            if (rgb.size() != 3 || !std::all_of(rgb.begin(), rgb.end(),
                                                [](float x)
                                                {
                                                    return std::isfinite(x) && x >= 0 && x <= 1;
                                                }))
            {
                throw std::runtime_error("Invalid linear palette color");
            }
            if (!newPalettes[palette.key()]
                     .emplace(row.at("index").get<int>(), ModelVec4{rgb[0], rgb[1], rgb[2], 1})
                     .second)
            {
                throw std::runtime_error("Duplicate palette index");
            }
        }
    }
    for (const auto& binding : newBindings)
    {
        for (const auto& id : binding.second.assets)
        {
            if (!newFiles.count(id) && !newPresets.count(id))
            {
                throw std::runtime_error("Unknown asset in binding " + binding.first + ": " + id);
            }
            if ((binding.first.rfind("NPC.", 0) == 0) != (newPresets.count(id) != 0))
            {
                throw std::runtime_error("Binding mixes NPC presets and single meshes: " +
                                         binding.first);
            }
        }
    }
    files.swap(newFiles);
    slots.swap(newSlots);
    presets.swap(newPresets);
    palettes.swap(newPalettes);
    materialRoles.swap(newRoles);
    bindings.swap(newBindings);
    cache.clear();
    failures.clear();
    assetRigs.swap(newAssetRigs);
    rigMotions.swap(newRigMotions);
}

std::shared_ptr<const ModelAsset> ModelLibrary::Load(const std::string& id)
{
    auto file = files.find(id);
    if (file == files.end())
    {
        throw std::runtime_error("Unknown model asset: " + id);
    }
    auto found = cache.find(file->second);
    if (found != cache.end())
    {
        return found->second;
    }
    auto failure = failures.find(file->second);
    if (failure != failures.end())
    {
        throw std::runtime_error(failure->second);
    }
    std::shared_ptr<const ModelAsset> asset;
    try
    {
        asset = ModelAsset::LoadGlb(file->second);
    }
    catch (const std::exception& error)
    {
        failures[file->second] = error.what();
        throw;
    }
    cache.emplace(file->second, asset);
    return asset;
}

ModelAssembly ModelLibrary::AssembleNpc(const std::string& id)
{
    const auto& preset = presets.at(id);
    if (preset.parts.at("headgear") >= 0 && preset.parts.at("hair") >= 0)
    {
        throw std::runtime_error("NPC headgear requires hair=-1");
    }
    ModelAssembly result;
    for (const char* slot : {"body", "hair", "top", "bottom", "footwear", "accessory", "headgear"})
    {
        const int index = preset.parts.at(slot);
        if (index == -1)
        {
            if (std::string(slot) != "hair" && std::string(slot) != "accessory" &&
                std::string(slot) != "headgear")
            {
                throw std::runtime_error("Required NPC slot is empty");
            }
            continue;
        }
        // All parts retain their authored origin, scale and transforms. Never center parts separately.
        const auto partId = slots.at(slot).at(index);
        result.parts.push_back(Load(partId));
        if (std::string(slot) == "body" && assetRigs.count(partId) &&
            rigMotions.count(assetRigs.at(partId)))
        {
            result.motions = rigMotions.at(assetRigs.at(partId));
        }
    }
    for (const auto& role : materialRoles)
    {
        result.materialOverrides[role.second] =
            palettes.at(role.first).at(preset.palettes.at(role.first));
    }
    return result;
}

ModelAssembly ModelLibrary::Single(const std::string& id)
{
    ModelAssembly assembly;
    assembly.parts.push_back(Load(id));
    if (assetRigs.count(id) && rigMotions.count(assetRigs.at(id)))
    {
        assembly.motions = rigMotions.at(assetRigs.at(id));
    }
    return assembly;
}

std::vector<std::string> ModelLibrary::AssetIds() const
{
    std::vector<std::string> result;
    for (const auto& entry : files)
    {
        result.push_back(entry.first);
    }
    return result;
}

std::vector<std::string> ModelLibrary::PresetIds() const
{
    std::vector<std::string> result;
    for (const auto& entry : presets)
    {
        result.push_back(entry.first);
    }
    return result;
}

size_t ModelLibrary::CachedAssetCount() const
{
    return cache.size();
}

std::string ModelLibrary::Binding(const std::string& key, size_t variant) const
{
    const auto* binding = FindBinding(key);
    return binding ? binding->assets[variant % binding->assets.size()] : "";
}

const ModelBinding* ModelLibrary::FindBinding(const std::string& key) const
{
    const auto found = bindings.find(key);
    return found == bindings.end() ? nullptr : &found->second;
}
