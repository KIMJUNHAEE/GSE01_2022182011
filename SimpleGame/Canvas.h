#pragma once
#include "Dependencies/glew.h"
#include <string>
#include <vector>
#include <map>

struct Vec2
{
    float x = 0, y = 0;
};

inline Vec2 operator+(Vec2 a, Vec2 b)
{
    return {a.x + b.x, a.y + b.y};
}

inline Vec2 operator-(Vec2 a, Vec2 b)
{
    return {a.x - b.x, a.y - b.y};
}

inline Vec2 operator*(Vec2 a, float s)
{
    return {a.x * s, a.y * s};
}

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

// Instanced, top-left-origin drawing in a resolution-independent 1440 x 900 canvas.
// Five immutable GPU meshes are cached per Canvas; draw attributes stream separately.
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

    void Flush();
    void Triangle(Vec2 a, Vec2 b, Vec2 c, Color color);
    void Quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color);
    void Rect(float x, float y, float w, float h, Color color);
    void Line(Vec2 a, Vec2 b, Color color, float thickness = 1);
    void Ellipse(Vec2 p, float rx, float ry, Color color, bool fill = true, float thickness = 1);
    void Glow(Vec2 p, float rx, float ry, Color color);
    void Texture(GLuint texture, float x, float y, float w, float h, Color color = Color());
    void TextureRegion(GLuint texture, float x, float y, float w, float h, float u0, float v0,
                       float u1, float v1, Color color = Color());
    float Text(float x, float y, const std::wstring& text, int size, Color color,
               bool bold = false);
    float Measure(const std::wstring& text, int size, bool bold = false);

private:

    enum class MeshKind
    {
        Triangle,
        Quad,
        Disc,
        Ring,
        Glow
    };

    struct MeshVertex
    {
        Vec2 p;
        Vec2 edge;
        float stroke = 0;
        float alpha = 1;
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
    };

    struct TextImage
    {
        GLuint texture = 0;
        int width = 0, height = 0;
    };

    std::map<std::wstring, TextImage> textCache;
    std::map<MeshKind, CachedMesh> meshes;
    std::vector<Instance> instances;
    static constexpr size_t InstanceCapacity = 4096;
    GLuint program = 0, instanceBuffer = 0, white = 0, currentTexture = 0;
    MeshKind currentMesh = MeshKind::Quad;
    const CachedMesh& GetMesh(MeshKind kind);
    void Submit(MeshKind kind, GLuint texture, const Instance& instance);
    void Release();
    const TextImage& GetText(const std::wstring& text, int size, bool bold);
};
