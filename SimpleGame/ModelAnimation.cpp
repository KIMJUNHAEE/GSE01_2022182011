#include "stdafx.h"
#include "ModelAnimation.h"
#include <cctype>
#include <stdexcept>

namespace
{
    std::string Lower(std::string value)
    {
        for (char& c : value)
        {
            c = (char)std::tolower((unsigned char)c);
        }
        return value;
    }

    ModelVec4 Sample(const ModelChannel& channel, float time)
    {
        const auto& keys = channel.times;
        size_t a = 0, b = 0;
        if (time >= keys.back())
        {
            a = b = keys.size() - 1;
        }
        else if (time > keys.front())
        {
            b = std::upper_bound(keys.begin(), keys.end(), time) - keys.begin();
            a = b - 1;
        }
        const bool cubic = channel.interpolation == ModelInterpolation::CubicSpline;
        ModelVec4 start = channel.values[a * (cubic ? 3 : 1) + (cubic ? 1 : 0)];
        if (a == b || channel.interpolation == ModelInterpolation::Step)
        {
            return start;
        }
        float duration = keys[b] - keys[a], t = (time - keys[a]) / duration;
        ModelVec4 end = channel.values[b * (cubic ? 3 : 1) + (cubic ? 1 : 0)];
        if (cubic)
        {
            float t2 = t * t, t3 = t2 * t;
            ModelVec4 result = start * (2 * t3 - 3 * t2 + 1) + end * (-2 * t3 + 3 * t2) +
                               channel.values[a * 3 + 2] * (duration * (t3 - 2 * t2 + t)) +
                               channel.values[b * 3] * (duration * (t3 - t2));
            return channel.path == ModelChannelPath::Rotation ? NormalizeQuaternion(result)
                                                              : result;
        }
        return channel.path == ModelChannelPath::Rotation ? Slerp(start, end, t)
                                                          : start * (1 - t) + end * t;
    }
} // namespace

ModelInstance::ModelInstance(std::shared_ptr<const ModelAsset> asset) : asset(std::move(asset))
{
    if (!this->asset)
    {
        throw std::invalid_argument("ModelInstance requires an asset");
    }
    world.resize(this->asset->nodes.size());
    translations.resize(world.size());
    rotations.resize(world.size());
    scales.resize(world.size());
    for (const auto& skin : this->asset->skins)
    {
        skinMatrices.emplace_back(skin.joints.size());
    }
}

int ModelInstance::FindClip(const std::string& name) const
{
    const std::string target = Lower(name);
    for (size_t i = 0; i < asset->clips.size(); ++i)
    {
        if (Lower(asset->clips[i].name) == target)
        {
            return (int)i;
        }
    }
    return -1;
}

bool ModelInstance::Play(const std::string& name, bool loop, bool restart)
{
    const int selected = FindClip(name);
    if (selected < 0)
    {
        return false;
    }
    if (selected != clip || restart)
    {
        clip = selected;
        time = 0;
        blendDuration = blendElapsed = 0;
    }
    looping = loop;
    dirty = true;
    return true;
}

bool ModelInstance::TransitionTo(const std::string& name, float duration, bool preservePhase)
{
    const int selected = FindClip(name);
    if (selected < 0 || !std::isfinite(duration) || duration < 0)
    {
        return false;
    }
    if (selected == clip)
    {
        looping = true;
        return true;
    }
    if (clip < 0 || duration == 0)
    {
        return Play(name);
    }
    // Snapshot the currently displayed pose, including any blend already in progress.
    Evaluate();
    blendTranslations = translations;
    blendRotations = rotations;
    blendScales = scales;
    const float oldDuration = asset->clips[clip].duration;
    const float phase = preservePhase && looping && oldDuration > 0 ? time / oldDuration : 0;
    clip = selected;
    looping = true;
    time = phase * asset->clips[clip].duration;
    blendDuration = duration;
    blendElapsed = 0;
    dirty = true;
    return true;
}

void ModelInstance::Update(float dt)
{
    if (!std::isfinite(dt) || !std::isfinite(speed) || dt < 0)
    {
        return;
    }
    if (clip < 0 || dt == 0)
    {
        return;
    }
    // Blend duration is real time, independent of the clip playback rate.
    blendElapsed = (std::min)(blendDuration, blendElapsed + dt);
    Seek(time + dt * speed);
}

void ModelInstance::Seek(float seconds)
{
    if (!std::isfinite(seconds))
    {
        return;
    }
    time = (std::max)(0.f, seconds);
    if (clip >= 0)
    {
        float duration = asset->clips[clip].duration;
        time = looping && duration > 0 ? std::fmod(time, duration) : (std::min)(time, duration);
    }
    dirty = true;
}

void ModelInstance::Evaluate() const
{
    if (!dirty)
    {
        return;
    }
    for (size_t i = 0; i < asset->nodes.size(); ++i)
    {
        const auto& node = asset->nodes[i];
        translations[i] = node.translation;
        rotations[i] = node.rotation;
        scales[i] = node.scale;
    }
    if (clip >= 0)
    {
        for (const auto& channel : asset->clips[clip].channels)
        {
            const auto value = Sample(channel, time);
            if (channel.path == ModelChannelPath::Rotation)
            {
                rotations[channel.node] = value;
            }
            else if (channel.path == ModelChannelPath::Translation)
            {
                translations[channel.node] = {value.x, value.y, value.z};
            }
            else
            {
                scales[channel.node] = {value.x, value.y, value.z};
            }
        }
    }
    if (blendDuration > 0 && blendElapsed < blendDuration)
    {
        float alpha = blendElapsed / blendDuration;
        alpha = alpha * alpha * (3 - 2 * alpha);
        for (size_t i = 0; i < asset->nodes.size(); ++i)
        {
            translations[i] = {blendTranslations[i].x * (1 - alpha) + translations[i].x * alpha,
                               blendTranslations[i].y * (1 - alpha) + translations[i].y * alpha,
                               blendTranslations[i].z * (1 - alpha) + translations[i].z * alpha};
            scales[i] = {blendScales[i].x * (1 - alpha) + scales[i].x * alpha,
                         blendScales[i].y * (1 - alpha) + scales[i].y * alpha,
                         blendScales[i].z * (1 - alpha) + scales[i].z * alpha};
            rotations[i] = Slerp(blendRotations[i], rotations[i], alpha);
        }
    }
    for (size_t i : asset->hierarchyOrder)
    {
        const auto& node = asset->nodes[i];
        ModelMatrix local =
            node.hasMatrix ? node.matrix : ModelTRS(translations[i], rotations[i], scales[i]);
        world[i] = node.parent < 0 ? local : world[node.parent] * local;
    }
    for (size_t index = 0; index < asset->skins.size(); ++index)
    {
        const auto& skin = asset->skins[index];
        for (size_t i = 0; i < skin.joints.size(); ++i)
        {
            skinMatrices[index][i] = world[skin.joints[i]] * skin.inverseBind[i];
        }
    }
    dirty = false;
}

const ModelAsset& ModelInstance::Asset() const
{
    return *asset;
}

const std::shared_ptr<const ModelAsset>& ModelInstance::SharedAsset() const
{
    return asset;
}

const std::vector<ModelMatrix>& ModelInstance::WorldMatrices() const
{
    Evaluate();
    return world;
}

const std::vector<ModelMatrix>& ModelInstance::SkinMatrices(size_t index) const
{
    Evaluate();
    return skinMatrices.at(index);
}

int ModelInstance::ClipIndex() const
{
    return clip;
}

float ModelInstance::Time() const
{
    return time;
}
