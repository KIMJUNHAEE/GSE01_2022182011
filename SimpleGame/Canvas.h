#pragma once
#include "Dependencies/glew.h"
#include <string>
#include <vector>
#include <map>

struct Vec2 { float x = 0, y = 0; };
inline Vec2 operator+(Vec2 a, Vec2 b) { return { a.x + b.x, a.y + b.y }; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return { a.x - b.x, a.y - b.y }; }
inline Vec2 operator*(Vec2 a, float s) { return { a.x * s, a.y * s }; }
struct Color {
    float r, g, b, a;
    Color(unsigned hex = 0xffffff, float alpha = 1) :
        r(((hex >> 16) & 255) / 255.f), g(((hex >> 8) & 255) / 255.f),
        b((hex & 255) / 255.f), a(alpha) {}
    Color Fade(float alpha) const { Color c = *this; c.a *= alpha; return c; }
};

// Batched, top-left-origin drawing in a resolution-independent 1440 x 900 canvas.
// Geometry, textures and Windows-rasterized Unicode text share the same pipeline.
class Canvas {
public:
    static constexpr float Width = 1440, Height = 900;
    Canvas();
    ~Canvas();
    bool Ready() const { return program != 0; }
    void Flush();
    void Triangle(Vec2 a, Vec2 b, Vec2 c, Color color);
    void Quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color);
    void Rect(float x, float y, float w, float h, Color color);
    void Line(Vec2 a, Vec2 b, Color color, float thickness = 1);
    void Ellipse(Vec2 p, float rx, float ry, Color color, bool fill = true, float thickness = 1);
    void Glow(Vec2 p, float rx, float ry, Color color);
    void Texture(GLuint texture, float x, float y, float w, float h, Color color = Color());
    void TextureRegion(GLuint texture, float x, float y, float w, float h, float u0, float v0, float u1, float v1, Color color = Color());
    float Text(float x, float y, const std::wstring& text, int size, Color color, bool bold = false);
    float Measure(const std::wstring& text, int size, bool bold = false);
private:
    struct Vertex { float x, y, u, v, r, g, b, a; };
    struct TextImage { GLuint texture = 0; int width = 0, height = 0; };
    std::map<std::wstring, TextImage> textCache;
    std::vector<Vertex> vertices;
    GLuint program = 0, vbo = 0, vao = 0, white = 0, currentTexture = 0;
    void Select(GLuint texture);
    void Push(Vec2 p, Vec2 uv, Color color);
    const TextImage& GetText(const std::wstring& text, int size, bool bold);
};
