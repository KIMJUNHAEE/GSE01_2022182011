#pragma once
#include "Actor.h"
#include "ModelLibrary.h"

struct ModelPart
{
    std::shared_ptr<const ModelAsset> asset;
    std::shared_ptr<ModelInstance> pose;
    std::vector<std::vector<size_t>> jointNodes;
    std::vector<int> sharedSkins;
    mutable std::vector<std::vector<ModelMatrix>> palettes;
    const std::vector<ModelMatrix>& SkinMatrices(size_t skin) const;
};

// One world actor may assemble several cached mesh parts with a shared authored origin.
class ModelActor : public Actor
{
public:

    explicit ModelActor(const ModelAssembly& assembly, float scale = 1,
                        RenderLayer layer = RenderLayer::World);
    ModelActor(const ModelActor& other);
    std::unique_ptr<Actor> Clone() const override;
    void Draw(ActorRenderer& renderer) const override;
    void Update(float dt) override;
    bool Play(const std::string& name, bool loop = true, bool restart = false);
    bool PlayAction(const std::string& name, bool restart = false);
    void Seek(float seconds);
    void ResetMotionTracking();
    std::vector<ModelPart> parts;
    std::map<std::string, ModelVec4> materialOverrides;
    float scale = 1, yaw = 0, opacity = 1;
    bool automaticLocomotion = true;

private:

    Vec2 previous;
    bool positioned = false;
    bool locomotionMoving = false;
    float stoppedFor = 0, filteredSpeed = 0, playbackRate = 1;
    std::string action;
    std::vector<std::shared_ptr<ModelInstance>> poses;
    std::map<std::string, ModelMotion> motions;
    bool finishAction = false;
    void UpdateLocomotion(float dt, Vec2 displacement);
};
