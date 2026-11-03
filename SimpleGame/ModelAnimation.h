#pragma once
#include "ModelAsset.h"

class ModelInstance
{
public:

    explicit ModelInstance(std::shared_ptr<const ModelAsset> asset);
    bool Play(const std::string& name, bool loop = true, bool restart = false);
    bool TransitionTo(const std::string& name, float duration = .16f, bool preservePhase = false);
    void Update(float dt);
    void Seek(float seconds);
    const ModelAsset& Asset() const;
    const std::shared_ptr<const ModelAsset>& SharedAsset() const;
    const std::vector<ModelMatrix>& WorldMatrices() const;
    const std::vector<ModelMatrix>& SkinMatrices(size_t skin) const;
    int ClipIndex() const;
    float Time() const;
    float speed = 1;

private:

    std::shared_ptr<const ModelAsset> asset;
    mutable std::vector<ModelMatrix> world;
    mutable std::vector<ModelVec3> translations, scales;
    mutable std::vector<ModelVec4> rotations;
    mutable std::vector<std::vector<ModelMatrix>> skinMatrices;
    std::vector<ModelVec3> blendTranslations, blendScales;
    std::vector<ModelVec4> blendRotations;
    mutable bool dirty = true;
    int clip = -1;
    float time = 0;
    bool looping = true;
    float blendDuration = 0, blendElapsed = 0;
    int FindClip(const std::string& name) const;
    void Evaluate() const;
};
