#include "stdafx.h"
#include "ModelActor.h"
#include "ActorRenderer.h"
#include <stdexcept>

namespace
{
    bool SameMatrix(const ModelMatrix& a, const ModelMatrix& b)
    {
        for (size_t i = 0; i < 16; ++i)
        {
            if (std::abs(a.values[i] - b.values[i]) > .0001f)
            {
                return false;
            }
        }
        return true;
    }
} // namespace

const std::vector<ModelMatrix>& ModelPart::SkinMatrices(size_t index) const
{
    if (sharedSkins.at(index) >= 0)
    {
        return pose->SkinMatrices(sharedSkins[index]);
    }
    const auto& world = pose->WorldMatrices();
    const auto& skin = asset->skins.at(index);
    auto& palette = palettes.at(index);
    for (size_t j = 0; j < skin.joints.size(); ++j)
    {
        palette[j] = world[jointNodes[index][j]] * skin.inverseBind[j];
    }
    return palette;
}

ModelActor::ModelActor(const ModelAssembly& assembly, float scale, RenderLayer layer)
    : Actor("Model", {}, layer), materialOverrides(assembly.materialOverrides), scale(scale),
      motions(assembly.motions)
{
    if (assembly.parts.empty())
    {
        throw std::invalid_argument("Empty model assembly");
    }
    auto driver = std::make_shared<ModelInstance>(assembly.parts.front());
    poses.push_back(driver);
    std::map<std::string, size_t> jointNames;
    for (const auto& skin : driver->Asset().skins)
    {
        for (size_t node : skin.joints)
        {
            jointNames[driver->Asset().nodes[node].name] = node;
        }
    }
    for (const auto& asset : assembly.parts)
    {
        ModelPart part;
        part.asset = asset;
        part.pose = driver;
        if (asset != assembly.parts.front() &&
            (asset->skins.empty() || driver->Asset().skins.empty()))
        {
            part.pose = std::make_shared<ModelInstance>(asset);
            poses.push_back(part.pose);
        }
        std::vector<ModelMatrix> bind(asset->nodes.size());
        for (size_t node : asset->hierarchyOrder)
        {
            bind[node] = asset->nodes[node].parent < 0
                             ? asset->nodes[node].matrix
                             : bind[asset->nodes[node].parent] * asset->nodes[node].matrix;
        }
        for (const auto& skin : asset->skins)
        {
            std::vector<size_t> mapping;
            for (size_t joint : skin.joints)
            {
                size_t source = joint;
                if (&part.pose->Asset() != asset.get())
                {
                    const auto& name = asset->nodes[joint].name;
                    auto found = jointNames.find(name);
                    if (name.empty() || found == jointNames.end())
                    {
                        throw std::runtime_error("NPC part has incompatible joint names");
                    }
                    source = found->second;
                    if (!SameMatrix(bind[joint], part.pose->WorldMatrices()[source]))
                    {
                        throw std::runtime_error("NPC part has incompatible bind transforms");
                    }
                }
                mapping.push_back(source);
            }
            int shared = -1;
            for (size_t candidate = 0; candidate < part.pose->Asset().skins.size(); ++candidate)
            {
                const auto& other = part.pose->Asset().skins[candidate];
                if (other.joints != mapping)
                {
                    continue;
                }
                bool same = true;
                for (size_t j = 0; j < skin.inverseBind.size(); ++j)
                {
                    same = same && SameMatrix(skin.inverseBind[j], other.inverseBind[j]);
                }
                if (same)
                {
                    shared = (int)candidate;
                }
            }
            part.sharedSkins.push_back(shared);
            part.jointNodes.push_back(std::move(mapping));
            part.palettes.emplace_back(skin.joints.size());
        }
        parts.push_back(std::move(part));
    }
    Play("Idle");
}

ModelActor::ModelActor(const ModelActor& other)
    : Actor(other), parts(other.parts), materialOverrides(other.materialOverrides),
      scale(other.scale), yaw(other.yaw), opacity(other.opacity),
      automaticLocomotion(other.automaticLocomotion), previous(other.previous),
      positioned(other.positioned), locomotionMoving(other.locomotionMoving),
      stoppedFor(other.stoppedFor), filteredSpeed(other.filteredSpeed),
      playbackRate(other.playbackRate), action(other.action), motions(other.motions),
      finishAction(other.finishAction)
{
    std::map<const ModelInstance*, std::shared_ptr<ModelInstance>> replacements;
    for (const auto& pose : other.poses)
    {
        auto copy = std::make_shared<ModelInstance>(*pose);
        replacements[pose.get()] = copy;
        poses.push_back(std::move(copy));
    }
    for (auto& part : parts)
    {
        part.pose = replacements.at(part.pose.get());
    }
}

std::unique_ptr<Actor> ModelActor::Clone() const
{
    return std::make_unique<ModelActor>(*this);
}

void ModelActor::Draw(ActorRenderer& renderer) const
{
    renderer.Draw(*this);
}

void ModelActor::Update(float dt)
{
    if (!std::isfinite(dt) || dt < 0)
    {
        return;
    }
    Vec2 position = Position();
    if (automaticLocomotion && dt > 0)
    {
        UpdateLocomotion(dt, positioned ? position - previous : Vec2{});
    }
    previous = position;
    positioned = true;
    for (auto& pose : poses)
    {
        pose->Update(dt);
    }
    if (finishAction && !poses.empty() && poses.front()->ClipIndex() >= 0)
    {
        const auto& pose = *poses.front();
        if (pose.Time() >= pose.Asset().clips[pose.ClipIndex()].duration)
        {
            auto motion = motions.find(action);
            if (motion != motions.end())
            {
                yaw += motion->second.headingDegrees * .01745329252f;
            }
            finishAction = false;
            automaticLocomotion = true;
            Play("Idle");
        }
    }
}

void ModelActor::UpdateLocomotion(float dt, Vec2 displacement)
{
    const float distance =
        std::sqrt(displacement.x * displacement.x + displacement.y * displacement.y);
    const bool displaced = distance > .001f;
    const float response = 1 - std::exp(-dt * 12);
    if (displaced)
    {
        stoppedFor = 0;
        locomotionMoving = true;
        const float speed = scale > 0 ? distance / (dt * scale) : 0;
        filteredSpeed += (speed - filteredSpeed) * response;
        const float targetYaw = std::atan2(displacement.x, displacement.y);
        const float turn = std::atan2(std::sin(targetYaw - yaw), std::cos(targetYaw - yaw));
        yaw += turn * (1 - std::exp(-dt * 14));
    }
    else
    {
        stoppedFor += dt;
        if (stoppedFor >= .14f)
        {
            locomotionMoving = false;
            filteredSpeed += (0 - filteredSpeed) * response;
        }
    }
    std::string next = locomotionMoving ? "Walk" : "Idle";
    const auto run = motions.find("Run");
    if (locomotionMoving && run != motions.end() && run->second.suggestedSpeed > 0)
    {
        const float threshold = run->second.suggestedSpeed * (action == "Run" ? .65f : .85f);
        if (filteredSpeed > threshold)
        {
            next = "Run";
        }
    }
    if (next != action)
    {
        const bool preservePhase = (action == "Walk" || action == "Run") && next != "Idle";
        for (auto& pose : poses)
        {
            pose->TransitionTo(next, .16f, preservePhase);
        }
        action = next;
    }
    const auto motion = motions.find(next);
    const float targetRate =
        locomotionMoving && motion != motions.end() && motion->second.suggestedSpeed > 0
            ? std::clamp(filteredSpeed / motion->second.suggestedSpeed, .65f, 1.8f)
            : 1.f;
    playbackRate += (targetRate - playbackRate) * response;
    for (auto& pose : poses)
    {
        pose->speed = playbackRate;
    }
}

bool ModelActor::Play(const std::string& name, bool loop, bool restart)
{
    bool played = false;
    for (auto& pose : poses)
    {
        played = pose->Play(name, loop, restart) || played;
    }
    action = name;
    return played;
}

bool ModelActor::PlayAction(const std::string& name, bool restart)
{
    auto motion = motions.find(name);
    const bool loop = motion != motions.end() ? motion->second.loop : false;
    if (!Play(name, loop, restart))
    {
        return false;
    }
    automaticLocomotion = false;
    finishAction = !loop;
    for (auto& pose : poses)
    {
        pose->speed = 1;
    }
    return true;
}

void ModelActor::Seek(float seconds)
{
    for (auto& pose : poses)
    {
        pose->Seek(seconds);
    }
}

void ModelActor::ResetMotionTracking()
{
    // Re-enabling graphics must not treat all movement while hidden as one frame's displacement.
    previous = Position();
    positioned = false;
    locomotionMoving = false;
    stoppedFor = filteredSpeed = 0;
    playbackRate = 1;
}
