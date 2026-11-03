#pragma once
#include "ModelMath.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

constexpr size_t ModelJointLimit = 128;

struct ModelVertex
{
    ModelVec3 position, normal;
    ModelVec4 color = {1, 1, 1, 1};
    std::array<uint32_t, 8> joints = {};
    std::array<float, 8> weights = {};
};

struct ModelPrimitive
{
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;
    size_t material = 0;
    bool weighted = false;
};

struct ModelMesh
{
    std::vector<ModelPrimitive> primitives;
};

struct ModelMaterial
{
    std::string name;
    ModelVec4 baseColor = {1, 1, 1, 1};
    ModelVec3 emissive;
    float alphaCutoff = 0;
    bool blend = false, unlit = false;
};

struct ModelNode
{
    std::string name;
    int parent = -1, mesh = -1, skin = -1;
    ModelVec3 translation, scale = {1, 1, 1};
    ModelVec4 rotation = {0, 0, 0, 1};
    ModelMatrix matrix;
    bool hasMatrix = false, inScene = false;
};

struct ModelSkin
{
    std::vector<size_t> joints;
    std::vector<ModelMatrix> inverseBind;
};

enum class ModelChannelPath
{
    Translation,
    Rotation,
    Scale
};
enum class ModelInterpolation
{
    Step,
    Linear,
    CubicSpline
};

struct ModelChannel
{
    size_t node;
    ModelChannelPath path;
    ModelInterpolation interpolation;
    std::vector<float> times;
    std::vector<ModelVec4> values;
};

struct ModelClip
{
    std::string name;
    float duration = 0;
    std::vector<ModelChannel> channels;
};

// Immutable CPU data. Instances own poses; the renderer owns cached GPU buffers.
struct ModelAsset
{
    std::filesystem::path source;
    std::vector<ModelMesh> meshes;
    std::vector<ModelMaterial> materials;
    std::vector<ModelNode> nodes;
    std::vector<size_t> hierarchyOrder;
    std::vector<ModelSkin> skins;
    std::vector<ModelClip> clips;
    ModelVec3 minimum, maximum;
    size_t triangleCount = 0;

    static std::shared_ptr<const ModelAsset> LoadGlb(const std::filesystem::path& path);
};
