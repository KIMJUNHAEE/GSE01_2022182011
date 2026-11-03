#include "stdafx.h"
#include "ModelRenderer.h"
#include "ShaderLoader.h"
#include <cstddef>
#include <stdexcept>
#include <limits>
#include <cstring>

namespace
{
    struct ModelDrawState
    {
        ~ModelDrawState()
        {
            // Explicit contract with Canvas: no per-actor glGet* driver round trips.
            // Both renderers bind their own program, VAO, buffers and textures before drawing.
            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
        }
    };

    bool SameShading(const ModelMaterial& a, const ModelMaterial& b)
    {
        return a.emissive.x == b.emissive.x && a.emissive.y == b.emissive.y &&
               a.emissive.z == b.emissive.z && a.alphaCutoff == b.alphaCutoff &&
               a.blend == b.blend && a.unlit == b.unlit;
    }
} // namespace

ModelRenderer::ModelRenderer()
{
    static_assert(sizeof(ModelMatrix) == 16 * sizeof(float));
    program = LoadShaderProgram("Shaders/Model.vs", "Shaders/Model.fs");
    GLint blockSize = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &blockSize);
    if (blockSize < (GLint)(ModelJointLimit * sizeof(ModelMatrix)))
    {
        glDeleteProgram(program);
        throw std::runtime_error("GPU uniform block is too small for model skins");
    }
    rootUniform = glGetUniformLocation(program, "u_Root");
    nodeUniform = glGetUniformLocation(program, "u_Node");
    skinUniform = glGetUniformLocation(program, "u_Skinned");
    footUniform = glGetUniformLocation(program, "u_Foot");
    zoomUniform = glGetUniformLocation(program, "u_Zoom");
    colorUniform = glGetUniformLocation(program, "u_Color");
    emissiveUniform = glGetUniformLocation(program, "u_Emissive");
    cutoffUniform = glGetUniformLocation(program, "u_AlphaCutoff");
    unlitUniform = glGetUniformLocation(program, "u_Unlit");
    viewSizeUniform = glGetUniformLocation(program, "u_CanvasSize");
    viewOriginUniform = glGetUniformLocation(program, "u_CanvasOrigin");
    glUniformBlockBinding(program, glGetUniformBlockIndex(program, "SkinBlock"), 2);
    glGenBuffers(1, &bones);
    if (!bones)
    {
        glDeleteProgram(program);
        throw std::runtime_error("Cannot allocate skin uniform buffer");
    }
    glBindBuffer(GL_UNIFORM_BUFFER, bones);
    glBufferData(GL_UNIFORM_BUFFER, ModelJointLimit * sizeof(ModelMatrix), nullptr,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

ModelRenderer::~ModelRenderer()
{
    ClearCache();
    glDeleteBuffers(1, &bones);
    glDeleteProgram(program);
}

void ModelRenderer::ClearCache()
{
    for (auto& entry : cache)
    {
        for (auto& primitives : entry.second.meshes)
        {
            for (auto& mesh : primitives)
            {
                glDeleteVertexArrays(1, &mesh.vao);
                glDeleteBuffers(1, &mesh.vertices);
                glDeleteBuffers(1, &mesh.indices);
            }
        }
    }
    cache.clear();
    assemblies.clear();
}

void ModelRenderer::SetView(const CanvasView& value)
{
    view = value;
}

std::shared_ptr<const ModelAsset> ModelRenderer::MergeAssembly(const ModelActor& actor)
{
    if (actor.parts.size() < 2)
    {
        return {};
    }
    std::vector<uintptr_t> key;
    key.reserve(actor.parts.size());
    for (const auto& part : actor.parts)
    {
        key.push_back(reinterpret_cast<uintptr_t>(part.asset.get()));
    }
    key.push_back(0);
    // Include exact palette values, not just the preset's mesh list or a collision-prone hash.
    for (const auto& entry : actor.materialOverrides)
    {
        key.push_back(entry.first.size());
        for (unsigned char character : entry.first)
        {
            key.push_back(character);
        }
        for (float component : {entry.second.x, entry.second.y, entry.second.z, entry.second.w})
        {
            uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(component));
            std::memcpy(&bits, &component, sizeof(bits));
            key.push_back(bits);
        }
    }
    auto found = assemblies.find(key);
    if (found != assemblies.end())
    {
        return found->second.merged;
    }
    Assembly entry;
    for (const auto& part : actor.parts)
    {
        entry.owners.push_back(part.asset);
    }
    auto merged = std::make_shared<ModelAsset>();
    merged->meshes.emplace_back();
    int sharedSkin = -1;
    bool compatible = true;
    for (const auto& part : actor.parts)
    {
        if (part.pose != actor.parts.front().pose)
        {
            compatible = false;
            break;
        }
        for (const auto& node : part.asset->nodes)
        {
            if (node.mesh < 0 || !node.inScene)
            {
                continue;
            }
            if (node.skin < 0 || part.sharedSkins.at(node.skin) < 0)
            {
                compatible = false;
                break;
            }
            const int palette = part.sharedSkins[node.skin];
            if (sharedSkin < 0)
            {
                sharedSkin = palette;
            }
            if (palette != sharedSkin)
            {
                compatible = false;
                break;
            }
            for (const auto& primitive : part.asset->meshes[node.mesh].primitives)
            {
                const auto& material = part.asset->materials[primitive.material];
                // Transparent primitives retain their authored order through the original path.
                if (material.blend || material.baseColor.w < .999f ||
                    std::any_of(
                        primitive.vertices.begin(), primitive.vertices.end(),
                        [](const auto& vertex)
                        {
                            return vertex.color.w < .999f;
                        }))
                {
                    compatible = false;
                    break;
                }
                size_t bucket = 0;
                for (; bucket < merged->materials.size(); ++bucket)
                {
                    if (SameShading(material, merged->materials[bucket]))
                    {
                        break;
                    }
                }
                if (bucket == merged->materials.size())
                {
                    auto bakedMaterial = material;
                    bakedMaterial.baseColor = {1, 1, 1, 1};
                    bakedMaterial.name = "NPC_Batch_" + std::to_string(bucket);
                    merged->materials.push_back(bakedMaterial);
                    merged->meshes.front().primitives.emplace_back();
                    merged->meshes.front().primitives.back().material = bucket;
                }
                auto& target = merged->meshes.front().primitives[bucket];
                if (target.vertices.size() + primitive.vertices.size() >
                    (std::numeric_limits<uint32_t>::max)())
                {
                    throw std::runtime_error("Merged NPC mesh exceeds index range");
                }
                const auto offset = uint32_t(target.vertices.size());
                target.vertices.insert(target.vertices.end(), primitive.vertices.begin(),
                                       primitive.vertices.end());
                const auto override = actor.materialOverrides.find(material.name);
                const auto color = override == actor.materialOverrides.end() ? material.baseColor
                                                                             : override->second;
                // Flat material colors become vertex colors, so compatible materials share a draw.
                // The immutable source assets and each actor's animated pose remain separate.
                for (size_t i = offset; i < target.vertices.size(); ++i)
                {
                    auto& value = target.vertices[i].color;
                    value = {value.x * color.x, value.y * color.y, value.z * color.z,
                             value.w * color.w};
                }
                for (uint32_t index : primitive.indices)
                {
                    target.indices.push_back(index + offset);
                }
            }
            if (!compatible)
            {
                break;
            }
        }
        if (!compatible)
        {
            break;
        }
    }
    if (compatible && sharedSkin >= 0 && !merged->materials.empty())
    {
        ModelNode node;
        node.mesh = 0;
        node.skin = sharedSkin;
        node.inScene = true;
        merged->nodes.push_back(node);
        entry.merged = merged;
    }
    auto result = entry.merged;
    assemblies.emplace(std::move(key), std::move(entry));
    return result;
}

size_t ModelRenderer::CachedAssetCount() const
{
    return cache.size();
}

const ModelRenderer::GpuAsset& ModelRenderer::Upload(
    const std::shared_ptr<const ModelAsset>& source)
{
    const auto& asset = *source;
    auto found = cache.find(&asset);
    if (found != cache.end())
    {
        return found->second;
    }
    auto& gpu = cache[&asset];
    gpu.owner = source;
    try
    {
        gpu.meshes.resize(asset.meshes.size());
        for (size_t i = 0; i < asset.meshes.size(); ++i)
        {
            for (const auto& primitive : asset.meshes[i].primitives)
            {
                gpu.meshes[i].emplace_back();
                auto& mesh = gpu.meshes[i].back();
                glGenVertexArrays(1, &mesh.vao);
                glGenBuffers(1, &mesh.vertices);
                glGenBuffers(1, &mesh.indices);
                if (!mesh.vao || !mesh.vertices || !mesh.indices)
                {
                    throw std::runtime_error("Model GPU allocation failed");
                }
                glBindVertexArray(mesh.vao);
                glBindBuffer(GL_ARRAY_BUFFER, mesh.vertices);
                glBufferData(GL_ARRAY_BUFFER, primitive.vertices.size() * sizeof(ModelVertex),
                             primitive.vertices.data(), GL_STATIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, primitive.indices.size() * sizeof(uint32_t),
                             primitive.indices.data(), GL_STATIC_DRAW);
                const size_t offsets[] = {offsetof(ModelVertex, position),
                                          offsetof(ModelVertex, normal),
                                          offsetof(ModelVertex, color),
                                          offsetof(ModelVertex, joints),
                                          offsetof(ModelVertex, weights),
                                          offsetof(ModelVertex, joints) + 4 * sizeof(uint32_t),
                                          offsetof(ModelVertex, weights) + 4 * sizeof(float)};
                const GLint sizes[] = {3, 3, 4, 4, 4, 4, 4};
                for (GLuint attribute = 0; attribute < 7; ++attribute)
                {
                    glEnableVertexAttribArray(attribute);
                    if (attribute == 3 || attribute == 5)
                    {
                        glVertexAttribIPointer(attribute, sizes[attribute], GL_UNSIGNED_INT,
                                               sizeof(ModelVertex),
                                               reinterpret_cast<const void*>(offsets[attribute]));
                    }
                    else
                    {
                        glVertexAttribPointer(attribute, sizes[attribute], GL_FLOAT, GL_FALSE,
                                              sizeof(ModelVertex),
                                              reinterpret_cast<const void*>(offsets[attribute]));
                    }
                }
                mesh.count = (GLsizei)primitive.indices.size();
            }
        }
    }
    catch (...)
    {
        ClearCache();
        throw;
    }
    glBindVertexArray(0);
    return gpu;
}

void ModelRenderer::Draw(const ModelActor& actor, Vec2 foot, float zoom, Canvas& canvas,
                         float opacity, bool batchParts)
{
    canvas.Flush(RenderStats::FlushReason::Other);
    const ModelDrawState previousState;
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    // Preserve the 2.5D scene graph's painter order; depth is local to this assembled actor.
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, bones);
    const ModelMatrix root =
        ModelTRS({}, {0, std::sin(actor.yaw * .5f), 0, std::cos(actor.yaw * .5f)},
                 {actor.scale, actor.scale, actor.scale});
    glUniformMatrix4fv(rootUniform, 1, GL_FALSE, root.values.data());
    glUniform2f(footUniform, foot.x, foot.y);
    glUniform1f(zoomUniform, zoom);
    glUniform2f(viewSizeUniform, view.width, view.height);
    glUniform2f(viewOriginUniform, view.origin.x, view.origin.y);
    const bool opaque = actor.opacity * opacity >= .999f &&
                        std::none_of(actor.materialOverrides.begin(), actor.materialOverrides.end(),
                                     [](const auto& entry)
                                     {
                                         return entry.second.w < .999f;
                                     });
    const auto merged = batchParts && opaque ? MergeAssembly(actor) : nullptr;
    const void* lastPalette = nullptr;
    for (int pass = 0; pass < 2; ++pass)
    {
        glDepthMask(pass == 0 ? GL_TRUE : GL_FALSE);
        const size_t partCount = merged ? 1 : actor.parts.size();
        for (size_t partIndex = 0; partIndex < partCount; ++partIndex)
        {
            const auto& part = actor.parts[partIndex];
            const auto& source = merged ? merged : part.asset;
            const auto& asset = *source;
            const auto& gpu = Upload(source);
            for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex)
            {
                const auto& node = asset.nodes[nodeIndex];
                if (node.mesh < 0 || !node.inScene)
                {
                    continue;
                }
                const auto& primitives = asset.meshes[node.mesh].primitives;
                if (std::none_of(primitives.begin(), primitives.end(),
                                 [&](const auto& primitive)
                                 {
                                     return asset.materials[primitive.material].blend ==
                                            (pass == 1);
                                 }))
                {
                    continue;
                }
                const ModelMatrix identity;
                const ModelMatrix& transform =
                    node.skin >= 0 ? identity : part.pose->WorldMatrices()[nodeIndex];
                glUniformMatrix4fv(nodeUniform, 1, GL_FALSE, transform.values.data());
                glUniform1i(skinUniform, node.skin >= 0 ? 1 : 0);
                if (node.skin >= 0)
                {
                    const auto& palette =
                        merged ? part.pose->SkinMatrices(node.skin) : part.SkinMatrices(node.skin);
                    if (lastPalette != palette.data())
                    {
                        glBindBuffer(GL_UNIFORM_BUFFER, bones);
                        // A previous actor may still be using this buffer on the GPU.
                        glBufferData(GL_UNIFORM_BUFFER, ModelJointLimit * sizeof(ModelMatrix),
                                     nullptr, GL_STREAM_DRAW);
                        glBufferSubData(GL_UNIFORM_BUFFER, 0, palette.size() * sizeof(ModelMatrix),
                                        palette.data());
                        lastPalette = palette.data();
                    }
                }
                for (size_t p = 0; p < asset.meshes[node.mesh].primitives.size(); ++p)
                {
                    const auto& primitive = asset.meshes[node.mesh].primitives[p];
                    const auto& material = asset.materials[primitive.material];
                    if (material.blend != (pass == 1))
                    {
                        continue;
                    }
                    auto override = merged ? actor.materialOverrides.end()
                                           : actor.materialOverrides.find(material.name);
                    auto color = override == actor.materialOverrides.end() ? material.baseColor
                                                                           : override->second;
                    glUniform4f(colorUniform, color.x, color.y, color.z,
                                color.w * actor.opacity * opacity);
                    glUniform3f(emissiveUniform, material.emissive.x, material.emissive.y,
                                material.emissive.z);
                    glUniform1f(cutoffUniform, material.alphaCutoff);
                    glUniform1i(unlitUniform, material.unlit ? 1 : 0);
                    const auto& mesh = gpu.meshes[node.mesh][p];
                    glBindVertexArray(mesh.vao);
                    glDrawElements(GL_TRIANGLES, mesh.count, GL_UNSIGNED_INT, nullptr);
                    RenderStats::DrawCall("Model/" + material.name, 1,
                                          RenderStats::FlushReason::Other);
                }
            }
        }
    }
}
