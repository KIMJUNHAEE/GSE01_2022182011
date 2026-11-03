#pragma once
#include "Canvas.h"
#include "ModelActor.h"
#include <cstdint>

class ModelRenderer
{
public:

    ModelRenderer();
    ~ModelRenderer();
    ModelRenderer(const ModelRenderer&) = delete;
    ModelRenderer& operator=(const ModelRenderer&) = delete;
    void Draw(const ModelActor& actor, Vec2 screenFoot, float zoom, Canvas& canvas,
              float opacity = 1, bool batchParts = true);
    void ClearCache();
    size_t CachedAssetCount() const;
    void SetView(const CanvasView& view);

private:

    struct Mesh
    {
        GLuint vao = 0, vertices = 0, indices = 0;
        GLsizei count = 0;
    };

    struct GpuAsset
    {
        std::shared_ptr<const ModelAsset> owner;
        std::vector<std::vector<Mesh>> meshes;
    };

    GLuint program = 0, bones = 0;
    GLint rootUniform = -1, nodeUniform = -1, skinUniform = -1, footUniform = -1;
    GLint zoomUniform = -1, colorUniform = -1, emissiveUniform = -1, cutoffUniform = -1;
    GLint unlitUniform = -1;
    GLint viewSizeUniform = -1, viewOriginUniform = -1;
    CanvasView view;
    std::map<const ModelAsset*, GpuAsset> cache;

    struct Assembly
    {
        std::shared_ptr<const ModelAsset> merged;
        std::vector<std::shared_ptr<const ModelAsset>> owners;
    };

    std::map<std::vector<uintptr_t>, Assembly> assemblies;
    std::shared_ptr<const ModelAsset> MergeAssembly(const ModelActor& actor);
    const GpuAsset& Upload(const std::shared_ptr<const ModelAsset>& asset);
};
