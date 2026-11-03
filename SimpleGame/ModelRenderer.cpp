#include "stdafx.h"
#include "ModelRenderer.h"
#include "ShaderLoader.h"
#include <cstddef>
#include <stdexcept>

namespace
{
    struct ModelDrawState
    {
        GLint program = 0, vao = 0, array = 0, uniformBuffer = 0, boneBinding = 0,
              depthFunction = 0;
        GLboolean depthMask = GL_TRUE;
        bool depth = false, cull = false;

        ModelDrawState()
        {
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array);
            glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &uniformBuffer);
            glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 2, &boneBinding);
            glGetIntegerv(GL_DEPTH_FUNC, &depthFunction);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
            depth = glIsEnabled(GL_DEPTH_TEST) != 0;
            cull = glIsEnabled(GL_CULL_FACE) != 0;
        }

        ~ModelDrawState()
        {
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, array);
            glBindBufferBase(GL_UNIFORM_BUFFER, 2, boneBinding);
            glBindBuffer(GL_UNIFORM_BUFFER, uniformBuffer);
            glUseProgram(program);
            glDepthMask(depthMask);
            glDepthFunc(depthFunction);
            if (!depth)
            {
                glDisable(GL_DEPTH_TEST);
            }
            if (cull)
            {
                glEnable(GL_CULL_FACE);
            }
        }
    };
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
                         float opacity)
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
    for (int pass = 0; pass < 2; ++pass)
    {
        glDepthMask(pass == 0 ? GL_TRUE : GL_FALSE);
        const void* lastPalette = nullptr;
        for (const auto& part : actor.parts)
        {
            const auto& asset = *part.asset;
            const auto& gpu = Upload(part.asset);
            for (size_t nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex)
            {
                const auto& node = asset.nodes[nodeIndex];
                if (node.mesh < 0 || !node.inScene)
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
                    const auto& palette = part.SkinMatrices(node.skin);
                    if (lastPalette != palette.data())
                    {
                        glBindBuffer(GL_UNIFORM_BUFFER, bones);
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
                    auto override = actor.materialOverrides.find(material.name);
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
