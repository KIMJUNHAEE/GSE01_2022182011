#pragma once
#include "ModelAnimation.h"
#include <map>

struct ModelMotion
{
    bool loop = true;
    float suggestedSpeed = 0, headingDegrees = 0;
};

struct ModelAssembly
{
    std::vector<std::shared_ptr<const ModelAsset>> parts;
    std::map<std::string, ModelVec4> materialOverrides;
    std::map<std::string, ModelMotion> motions;
};

struct ModelBinding
{
    std::vector<std::string> assets;
    float scale = 1;
    bool fitFootprint = false;
    float ScaleFor(const ModelAsset& asset, float width, float depth) const;
};

class ModelLibrary
{
public:

    explicit ModelLibrary(std::filesystem::path root);
    static bool SupportsSchemaVersion(const std::string& version, unsigned major);
    void Reload();
    std::shared_ptr<const ModelAsset> Load(const std::string& assetId);
    ModelAssembly AssembleNpc(const std::string& presetId);
    ModelAssembly Single(const std::string& assetId);
    std::vector<std::string> AssetIds() const;
    std::vector<std::string> PresetIds() const;
    size_t CachedAssetCount() const;
    std::string Binding(const std::string& key, size_t variant = 0) const;
    const ModelBinding* FindBinding(const std::string& key) const;

private:

    struct Preset
    {
        std::map<std::string, int> parts, palettes;
    };

    std::filesystem::path root;
    std::map<std::string, std::filesystem::path> files;
    std::map<std::filesystem::path, std::shared_ptr<const ModelAsset>> cache;
    std::map<std::string, std::map<int, std::string>> slots;
    std::map<std::string, Preset> presets;
    std::map<std::string, std::map<int, ModelVec4>> palettes;
    std::map<std::string, std::string> materialRoles;
    std::map<std::string, ModelBinding> bindings;
    std::map<std::filesystem::path, std::string> failures;
    std::map<std::string, std::string> assetRigs;
    std::map<std::string, std::map<std::string, ModelMotion>> rigMotions;
    std::filesystem::path SafePath(const std::filesystem::path& relative) const;
};
