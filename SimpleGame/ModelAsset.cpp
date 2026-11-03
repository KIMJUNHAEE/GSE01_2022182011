#include "stdafx.h"
#include "ModelAsset.h"

// cgltf also exposes its implementation when IntelliSense parses this header.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

#include "Dependencies/cgltf.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <fstream>
#include <limits>
#include <queue>
#include <stdexcept>

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    std::string Name(const char* name)
    {
        return name ? name : "";
    }

    ModelVec4 Read(const cgltf_accessor* accessor, size_t index, size_t components)
    {
        float values[16] = {};
        Require(accessor && index < accessor->count &&
                    cgltf_num_components(accessor->type) == components &&
                    cgltf_accessor_read_float(accessor, index, values, 16),
                "Invalid GLB accessor");
        for (size_t i = 0; i < components; ++i)
        {
            Require(std::isfinite(values[i]), "Non-finite GLB value");
        }
        return {values[0], values[1], values[2], values[3]};
    }

    ModelVec3 XYZ(ModelVec4 value)
    {
        return {value.x, value.y, value.z};
    }

    const cgltf_accessor* Attribute(const cgltf_primitive& primitive, cgltf_attribute_type type,
                                    int index = 0)
    {
        for (size_t i = 0; i < primitive.attributes_count; ++i)
        {
            if (primitive.attributes[i].type == type && primitive.attributes[i].index == index)
            {
                return primitive.attributes[i].data;
            }
        }
        return nullptr;
    }

    ModelPrimitive ImportPrimitive(const cgltf_primitive& primitive, const cgltf_data& data)
    {
        Require(primitive.type == cgltf_primitive_type_triangles && !primitive.targets_count &&
                    !primitive.has_draco_mesh_compression,
                "Only uncompressed triangle meshes without morph targets are supported");
        const auto* positions = Attribute(primitive, cgltf_attribute_type_position);
        const auto* normals = Attribute(primitive, cgltf_attribute_type_normal);
        const auto* colors = Attribute(primitive, cgltf_attribute_type_color);
        Require(positions && normals && positions->count == normals->count &&
                    positions->count <= 2000000,
                "Missing normals or invalid vertex count");
        ModelPrimitive result;
        result.material =
            primitive.material ? primitive.material - data.materials : data.materials_count;
        result.vertices.resize(positions->count);
        for (size_t i = 0; i < result.vertices.size(); ++i)
        {
            auto& vertex = result.vertices[i];
            vertex.position = XYZ(Read(positions, i, 3));
            vertex.normal = XYZ(Read(normals, i, 3));
            if (colors)
            {
                size_t count = cgltf_num_components(colors->type);
                Require(count == 3 || count == 4, "Invalid vertex color");
                vertex.color = Read(colors, i, count);
                if (count == 3)
                {
                    vertex.color.w = 1;
                }
            }
            float total = 0;
            for (int set = 0; set < 2; ++set)
            {
                const auto* joints = Attribute(primitive, cgltf_attribute_type_joints, set);
                const auto* weights = Attribute(primitive, cgltf_attribute_type_weights, set);
                Require((joints == nullptr) == (weights == nullptr), "Unpaired joints and weights");
                if (!joints)
                {
                    continue;
                }
                Require(!joints->is_sparse && joints->type == cgltf_type_vec4 &&
                            joints->count == positions->count &&
                            weights->count == positions->count &&
                            (joints->component_type == cgltf_component_type_r_8u ||
                             joints->component_type == cgltf_component_type_r_16u),
                        "Invalid joint data");
                cgltf_uint ids[4] = {};
                Require(cgltf_accessor_read_uint(joints, i, ids, 4), "Cannot read joint indices");
                const auto value = Read(weights, i, 4);
                const float weight[] = {value.x, value.y, value.z, value.w};
                for (int j = 0; j < 4; ++j)
                {
                    Require(weight[j] >= 0, "Negative skin weight");
                    vertex.weights[set * 4 + j] = weight[j];
                    vertex.joints[set * 4 + j] = weight[j] > 0 ? ids[j] : 0;
                    total += weight[j];
                }
                result.weighted = true;
            }
            Require(!Attribute(primitive, cgltf_attribute_type_joints, 2),
                    "More than 8 skin influences");
            if (result.weighted)
            {
                Require(total > 1e-8f, "Vertex has zero total skin weight");
                for (auto& weight : vertex.weights)
                {
                    weight /= total;
                }
            }
        }
        const size_t count = primitive.indices ? primitive.indices->count : positions->count;
        Require(count % 3 == 0 && count <= 6000000, "Invalid triangle index count");
        Require(!primitive.indices || !primitive.indices->is_sparse,
                "Sparse triangle indices unsupported");
        result.indices.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            size_t index = primitive.indices ? cgltf_accessor_read_index(primitive.indices, i) : i;
            Require(index < result.vertices.size(), "GLB index outside vertex buffer");
            result.indices.push_back((uint32_t)index);
        }
        return result;
    }
} // namespace

std::shared_ptr<const ModelAsset> ModelAsset::LoadGlb(const std::filesystem::path& path)
{
    try
    {
        // filesystem::path preserves Korean Windows paths; cgltf never opens an ANSI filename.
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        Require((bool)stream, "Cannot open GLB file");
        auto size = stream.tellg();
        Require(size >= 20 && size <= 256 * 1024 * 1024, "Invalid GLB file size");
        std::vector<char> bytes((size_t)size);
        stream.seekg(0);
        Require((bool)stream.read(bytes.data(), (std::streamsize)bytes.size()),
                "Truncated GLB file");
        cgltf_options options = {};
        cgltf_data* raw = nullptr;
        Require(cgltf_parse(&options, bytes.data(), bytes.size(), &raw) == cgltf_result_success,
                "GLB parsing failed");
        std::unique_ptr<cgltf_data, decltype(&cgltf_free)> owner(raw, cgltf_free);
        const auto& data = *raw;
        Require(data.file_type == cgltf_file_type_glb && data.nodes_count <= 8192 &&
                    data.meshes_count > 0,
                "Expected a GLB 2.0 mesh asset");
        for (size_t i = 0; i < data.buffers_count; ++i)
        {
            Require(!data.buffers[i].uri,
                    "External/data URI buffers unsupported; export a self-contained GLB");
        }
        for (size_t i = 0; i < data.buffer_views_count; ++i)
        {
            Require(!data.buffer_views[i].has_meshopt_compression,
                    "Meshopt compression unsupported");
        }
        Require(cgltf_load_buffers(&options, raw, nullptr) == cgltf_result_success &&
                    cgltf_validate(raw) == cgltf_result_success,
                "Invalid GLB buffer/accessor layout");
        Require(data.textures_count == 0,
                "Textured GLB unsupported by the flat-color LowPoly renderer");
        for (size_t i = 0; i < data.extensions_required_count; ++i)
        {
            std::string extension = data.extensions_required[i];
            Require(extension == "KHR_materials_unlit" ||
                        extension == "KHR_materials_emissive_strength",
                    "Unsupported required glTF extension");
        }
        auto asset = std::make_shared<ModelAsset>();
        asset->source = std::filesystem::weakly_canonical(path);
        for (size_t i = 0; i < data.materials_count; ++i)
        {
            const auto& source = data.materials[i];
            ModelMaterial material;
            material.name = Name(source.name);
            const float* color = source.pbr_metallic_roughness.base_color_factor;
            material.baseColor = {color[0], color[1], color[2], color[3]};
            float strength =
                source.has_emissive_strength ? source.emissive_strength.emissive_strength : 1.f;
            material.emissive = {source.emissive_factor[0] * strength,
                                 source.emissive_factor[1] * strength,
                                 source.emissive_factor[2] * strength};
            material.blend = source.alpha_mode == cgltf_alpha_mode_blend;
            material.alphaCutoff =
                source.alpha_mode == cgltf_alpha_mode_mask ? source.alpha_cutoff : 0;
            material.unlit = source.unlit != 0;
            asset->materials.push_back(material);
        }
        asset->materials.push_back(ModelMaterial{});
        for (size_t i = 0; i < data.meshes_count; ++i)
        {
            ModelMesh mesh;
            for (size_t j = 0; j < data.meshes[i].primitives_count; ++j)
            {
                mesh.primitives.push_back(ImportPrimitive(data.meshes[i].primitives[j], data));
                asset->triangleCount += mesh.primitives.back().indices.size() / 3;
            }
            asset->meshes.push_back(std::move(mesh));
        }
        std::vector<std::vector<size_t>> children(data.nodes_count);
        std::queue<size_t> pending;
        for (size_t i = 0; i < data.nodes_count; ++i)
        {
            const auto& source = data.nodes[i];
            ModelNode node;
            node.name = Name(source.name);
            node.parent = source.parent ? (int)(source.parent - data.nodes) : -1;
            node.mesh = source.mesh ? (int)(source.mesh - data.meshes) : -1;
            node.skin = source.skin ? (int)(source.skin - data.skins) : -1;
            node.translation = {source.translation[0], source.translation[1],
                                source.translation[2]};
            node.rotation = {source.rotation[0], source.rotation[1], source.rotation[2],
                             source.rotation[3]};
            node.scale = {source.scale[0], source.scale[1], source.scale[2]};
            node.hasMatrix = source.has_matrix != 0;
            cgltf_node_transform_local(&source, node.matrix.values.data());
            for (float value : node.matrix.values)
            {
                Require(std::isfinite(value), "Invalid node transform");
            }
            if (node.parent < 0)
            {
                pending.push(i);
            }
            else
            {
                children[node.parent].push_back(i);
            }
            asset->nodes.push_back(node);
        }
        while (!pending.empty())
        {
            size_t node = pending.front();
            pending.pop();
            asset->hierarchyOrder.push_back(node);
            for (size_t child : children[node])
            {
                pending.push(child);
            }
        }
        Require(asset->hierarchyOrder.size() == asset->nodes.size(), "Cyclic node hierarchy");
        const cgltf_scene* scene = data.scene          ? data.scene
                                   : data.scenes_count ? &data.scenes[0]
                                                       : nullptr;
        if (scene)
        {
            for (size_t i = 0; i < scene->nodes_count; ++i)
            {
                asset->nodes[scene->nodes[i] - data.nodes].inScene = true;
            }
        }
        for (size_t i : asset->hierarchyOrder)
        {
            auto& node = asset->nodes[i];
            node.inScene =
                node.inScene || !scene || (node.parent >= 0 && asset->nodes[node.parent].inScene);
        }
        for (size_t i = 0; i < data.skins_count; ++i)
        {
            const auto& source = data.skins[i];
            Require(source.joints_count > 0 && source.joints_count <= ModelJointLimit,
                    "Skin exceeds 128-joint limit");
            ModelSkin skin;
            for (size_t j = 0; j < source.joints_count; ++j)
            {
                skin.joints.push_back(source.joints[j] - data.nodes);
                ModelMatrix inverse;
                if (source.inverse_bind_matrices)
                {
                    Require(source.inverse_bind_matrices->type == cgltf_type_mat4 &&
                                source.inverse_bind_matrices->count == source.joints_count &&
                                cgltf_accessor_read_float(source.inverse_bind_matrices, j,
                                                          inverse.values.data(), 16),
                            "Invalid inverse bind matrix");
                    for (float value : inverse.values)
                    {
                        Require(std::isfinite(value), "Invalid inverse bind value");
                    }
                }
                skin.inverseBind.push_back(inverse);
            }
            asset->skins.push_back(std::move(skin));
        }
        for (const auto& node : asset->nodes)
        {
            if (node.skin < 0 || node.mesh < 0)
            {
                continue;
            }
            for (const auto& primitive : asset->meshes[node.mesh].primitives)
            {
                Require(primitive.weighted, "Skinned primitive has no weights");
                for (const auto& vertex : primitive.vertices)
                {
                    for (size_t j = 0; j < 8; ++j)
                    {
                        Require(vertex.joints[j] < asset->skins[node.skin].joints.size(),
                                "Joint index outside skin");
                    }
                }
            }
        }
        for (size_t i = 0; i < data.animations_count; ++i)
        {
            ModelClip clip;
            clip.name = data.animations[i].name ? data.animations[i].name
                                                : "Animation_" + std::to_string(i);
            for (size_t j = 0; j < data.animations[i].channels_count; ++j)
            {
                const auto& source = data.animations[i].channels[j];
                Require(source.target_node &&
                            (source.target_path == cgltf_animation_path_type_translation ||
                             source.target_path == cgltf_animation_path_type_rotation ||
                             source.target_path == cgltf_animation_path_type_scale),
                        "Morph animation is unsupported");
                ModelChannel channel;
                channel.node = source.target_node - data.nodes;
                Require(!asset->nodes[channel.node].hasMatrix,
                        "Animation targets a matrix-only node");
                channel.path = source.target_path == cgltf_animation_path_type_rotation
                                   ? ModelChannelPath::Rotation
                               : source.target_path == cgltf_animation_path_type_translation
                                   ? ModelChannelPath::Translation
                                   : ModelChannelPath::Scale;
                channel.interpolation =
                    source.sampler->interpolation == cgltf_interpolation_type_step
                        ? ModelInterpolation::Step
                    : source.sampler->interpolation == cgltf_interpolation_type_cubic_spline
                        ? ModelInterpolation::CubicSpline
                        : ModelInterpolation::Linear;
                const auto& sampler = *source.sampler;
                Require(
                    sampler.input->count > 0 && sampler.input->count <= 1000000 &&
                        sampler.output->count ==
                            sampler.input->count *
                                (channel.interpolation == ModelInterpolation::CubicSpline ? 3 : 1),
                    "Invalid animation sampler length");
                for (size_t key = 0; key < sampler.input->count; ++key)
                {
                    float time = Read(sampler.input, key, 1).x;
                    Require(time >= 0 && (key == 0 || time > channel.times.back()),
                            "Unsorted animation keys");
                    channel.times.push_back(time);
                }
                for (size_t key = 0; key < sampler.output->count; ++key)
                {
                    channel.values.push_back(Read(
                        sampler.output, key, channel.path == ModelChannelPath::Rotation ? 4 : 3));
                }
                clip.duration = (std::max)(clip.duration, channel.times.back());
                clip.channels.push_back(std::move(channel));
            }
            asset->clips.push_back(std::move(clip));
        }
        std::vector<ModelMatrix> world(asset->nodes.size());
        const float infinity = std::numeric_limits<float>::infinity();
        asset->minimum = {infinity, infinity, infinity};
        asset->maximum = {-infinity, -infinity, -infinity};
        for (size_t i : asset->hierarchyOrder)
        {
            const auto& node = asset->nodes[i];
            world[i] = node.parent < 0 ? node.matrix : world[node.parent] * node.matrix;
            if (node.mesh < 0 || !node.inScene)
            {
                continue;
            }
            for (const auto& primitive : asset->meshes[node.mesh].primitives)
            {
                for (const auto& vertex : primitive.vertices)
                {
                    ModelVec3 p = TransformPoint(world[i], vertex.position);
                    asset->minimum = {(std::min)(asset->minimum.x, p.x),
                                      (std::min)(asset->minimum.y, p.y),
                                      (std::min)(asset->minimum.z, p.z)};
                    asset->maximum = {(std::max)(asset->maximum.x, p.x),
                                      (std::max)(asset->maximum.y, p.y),
                                      (std::max)(asset->maximum.z, p.z)};
                }
            }
        }
        Require(std::isfinite(asset->minimum.x), "No mesh in default GLB scene");
        return asset;
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error(path.u8string() + ": " + error.what());
    }
}
