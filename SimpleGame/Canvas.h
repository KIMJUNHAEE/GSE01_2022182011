#pragma once
#include "Dependencies/glew.h"
#include "RenderStats.h"
#include "WorldMath.h"
#include "WindowMetrics.h"
#include <string>
#include <vector>
#include <map>

struct Color
{
    float r, g, b, a;

    Color(unsigned hex = 0xffffff, float alpha = 1)
        : r(((hex >> 16) & 255) / 255.f), g(((hex >> 8) & 255) / 255.f), b((hex & 255) / 255.f),
          a(alpha)
    {
    }

    Color Fade(float alpha) const
    {
        Color c = *this;
        c.a *= alpha;
        return c;
    }
};

// One cached quad renders every shape, in submission order, without shape-change flushes.
// The 1440x900 HUD safe area is centered inside a monitor-dependent world view.
class Canvas
{
public:

    static constexpr float Width = 1440, Height = 900;
    Canvas();
    ~Canvas();
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    bool Ready() const
    {
        return program != 0;
    }

    size_t CachedMeshCount() const
    {
        return meshes.size();
    }

    void Flush(RenderStats::FlushReason reason = RenderStats::FlushReason::EndOfFrame);
    void SetView(const CanvasView& view);
    void Fill(Color color);
    void Triangle(Vec2 a, Vec2 b, Vec2 c, Color color);
    void Quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color);
    void Rect(float x, float y, float w, float h, Color color);
    void Line(Vec2 a, Vec2 b, Color color, float thickness = 1);
    void Ellipse(Vec2 p, float rx, float ry, Color color, bool fill = true, float thickness = 1);
    void Glow(Vec2 p, float rx, float ry, Color color);

    // Deferred soft lights keep their established overlay order above opaque world geometry.
    // Flush once after the world. A queued light may cover an otherwise occluding object.
    void QueueGlow(Vec2 p, float rx, float ry, Color color);
    void FlushQueuedGlows();
    void Texture(GLuint texture, float x, float y, float w, float h, Color color = Color());
    void TextureRegion(GLuint texture, float x, float y, float w, float h, float u0, float v0,
                       float u1, float v1, Color color = Color());
    float Text(float x, float y, const std::wstring& text, int size, Color color,
               bool bold = false);
    float Measure(const std::wstring& text, int size, bool bold = false);

private:

    struct MeshVertex
    {
        Vec2 p;
    };

    struct CachedMesh
    {
        GLuint vao = 0, vbo = 0;
        GLsizei count = 0;
    };

    struct Instance
    {
        Vec2 a, b, c, d;
        float u0, v0, u1, v1;
        Color color;
        float thickness;
        float shape = 0; // 0 quad/triangle/texture, 1 disc, 2 ring, 3 radial glow
        Vec2 radii;
    };

    struct TextImage
    {
        GLuint texture = 0;
        int width = 0, height = 0;
        uint64_t lastUse = 0;
    };

    struct QueuedGlow
    {
        Vec2 p;
        float rx, ry;
        Color color;
    };

    std::map<std::wstring, TextImage> textCache;
    std::vector<CachedMesh> meshes;
    std::vector<Instance> instances;
    std::vector<QueuedGlow> queuedGlows;
    static constexpr size_t InstanceCapacity = 4096;
    GLuint program = 0, instanceBuffer = 0, white = 0, currentTexture = 0;
    GLint sizeUniform = -1, originUniform = -1;
    CanvasView view;
    float fontScale = 1;
    uint64_t textUse = 0;
    const CachedMesh& GetMesh();
    void Submit(GLuint texture, const Instance& instance);
    void Release();
    const TextImage& GetText(const std::wstring& text, int size, bool bold);
};
