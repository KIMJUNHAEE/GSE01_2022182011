#include "stdafx.h"
#include "Canvas.h"
#include "RenderStats.h"
#include "ShaderLoader.h"
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <type_traits>

Canvas::Canvas()
{
    static_assert(std::is_standard_layout_v<MeshVertex> && std::is_standard_layout_v<Instance>);
    static_assert(sizeof(Vec2) == 2 * sizeof(float) && sizeof(Color) == 4 * sizeof(float));
    static_assert(offsetof(MeshVertex, alpha) == offsetof(MeshVertex, stroke) + sizeof(float));
    try
    {
        program = LoadShaderProgram("Shaders/Canvas.vs", "Shaders/Canvas.fs");
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program, "u_Image"), 0);
        glUniform2f(glGetUniformLocation(program, "u_CanvasSize"), Width, Height);
        glGenBuffers(1, &instanceBuffer);
        unsigned char pixel[] = {255, 255, 255, 255};
        glGenTextures(1, &white);
        if (!instanceBuffer || !white)
        {
            throw std::runtime_error("Canvas resource creation failed");
        }
        glBindTexture(GL_TEXTURE_2D, white);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        instances.reserve(InstanceCapacity);
    }
    catch (...)
    {
        Release();
        throw;
    }
}

Canvas::~Canvas()
{
    Release();
}

void Canvas::Release()
{
    for (const auto& item : meshes)
    {
        glDeleteVertexArrays(1, &item.second.vao);
        glDeleteBuffers(1, &item.second.vbo);
    }
    meshes.clear();
    for (const auto& item : textCache)
    {
        glDeleteTextures(1, &item.second.texture);
    }
    textCache.clear();
    glDeleteTextures(1, &white);
    glDeleteBuffers(1, &instanceBuffer);
    glDeleteProgram(program);
    program = instanceBuffer = white = 0;
}

const Canvas::CachedMesh& Canvas::GetMesh(MeshKind kind)
{
    auto found = meshes.find(kind);
    if (found != meshes.end())
    {
        return found->second;
    }

    // Unit-space topology is generated and uploaded only on the first request for each kind.
    std::vector<MeshVertex> vertices;
    if (kind == MeshKind::Triangle || kind == MeshKind::Quad)
    {
        vertices = {{{0, 0}}, {{1, 0}}, {{1, 1}}};
        if (kind == MeshKind::Quad)
        {
            vertices.insert(vertices.end(), {{{0, 0}}, {{1, 1}}, {{0, 1}}});
        }
    }
    else
    {
        constexpr int Segments = 48;
        vertices.reserve(Segments * (kind == MeshKind::Ring ? 6 : 3));
        for (int i = 0; i < Segments; ++i)
        {
            float a = i * 6.2831853f / Segments;
            float b = (i + 1) * 6.2831853f / Segments;
            Vec2 u = {.5f + std::cos(a) * .5f, .5f + std::sin(a) * .5f};
            Vec2 v = {.5f + std::cos(b) * .5f, .5f + std::sin(b) * .5f};
            if (kind == MeshKind::Ring)
            {
                Vec2 edge = v - u;
                vertices.push_back({u, edge, .5f, 1});
                vertices.push_back({v, edge, .5f, 1});
                vertices.push_back({v, edge, -.5f, 1});
                vertices.push_back({u, edge, .5f, 1});
                vertices.push_back({v, edge, -.5f, 1});
                vertices.push_back({u, edge, -.5f, 1});
            }
            else
            {
                float alpha = kind == MeshKind::Glow ? 0.f : 1.f;
                vertices.push_back({{.5f, .5f}, {}, 0, 1});
                vertices.push_back({u, {}, 0, alpha});
                vertices.push_back({v, {}, 0, alpha});
            }
        }
    }

    auto entry = meshes.emplace(kind, CachedMesh{}).first;
    CachedMesh& mesh = entry->second;
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    if (!mesh.vao || !mesh.vbo)
    {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
        meshes.erase(entry);
        throw std::runtime_error("Canvas mesh cache allocation failed");
    }
    mesh.count = (GLsizei)vertices.size();
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(MeshVertex), vertices.data(),
                 GL_STATIC_DRAW);
    const size_t vertexOffsets[] = {offsetof(MeshVertex, p), offsetof(MeshVertex, edge),
                                    offsetof(MeshVertex, stroke)};
    for (GLuint i = 0; i < 3; ++i)
    {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                              reinterpret_cast<const void*>(vertexOffsets[i]));
        glVertexAttribDivisor(i, 0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
    const size_t instanceOffsets[] = {offsetof(Instance, a),        offsetof(Instance, b),
                                      offsetof(Instance, c),        offsetof(Instance, d),
                                      offsetof(Instance, u0),       offsetof(Instance, color),
                                      offsetof(Instance, thickness)};
    const GLint components[] = {2, 2, 2, 2, 4, 4, 1};
    for (GLuint i = 0; i < 7; ++i)
    {
        GLuint attribute = 3 + i;
        glEnableVertexAttribArray(attribute);
        glVertexAttribPointer(attribute, components[i], GL_FLOAT, GL_FALSE, sizeof(Instance),
                              reinterpret_cast<const void*>(instanceOffsets[i]));
        glVertexAttribDivisor(attribute, 1);
    }
    glBindVertexArray(0);
    return mesh;
}

void Canvas::Submit(MeshKind kind, GLuint texture, const Instance& instance)
{
    // Do not sort transparent geometry: only consecutive compatible draws may be batched.
    if (kind != currentMesh || texture != currentTexture || instances.size() >= InstanceCapacity)
    {
        RenderStats::FlushReason reason = RenderStats::FlushReason::Other;
        if (kind != currentMesh)
        {
            reason = RenderStats::FlushReason::MeshKindChanged;
        }
        else if (texture != currentTexture)
        {
            reason = RenderStats::FlushReason::TextureChanged;
        }
        else
        {
            reason = RenderStats::FlushReason::CapacityReached;
        }
        Flush(reason);
    }
    currentMesh = kind;
    currentTexture = texture;
    instances.push_back(instance);
}

void Canvas::Flush(RenderStats::FlushReason reason)
{
    if (instances.empty())
    {
        return;
    }
    const CachedMesh& mesh = GetMesh(currentMesh);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, currentTexture);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
    // Only per-draw attributes stream each frame; cached topology is never re-uploaded.
    // Orphaning prevents overwriting instance data that a previous draw is still using.
    glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(Instance), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, instances.size() * sizeof(Instance), instances.data());
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArraysInstanced(GL_TRIANGLES, 0, mesh.count, (GLsizei)instances.size());

    const char* kindName = "Unknown";
    switch (currentMesh)
    {
    case MeshKind::Triangle:
        kindName = "Triangle";
        break;
    case MeshKind::Quad:
        kindName = "Quad";
        break;
    case MeshKind::Disc:
        kindName = "Disc";
        break;
    case MeshKind::Ring:
        kindName = "Ring";
        break;
    case MeshKind::Glow:
        kindName = "Glow";
        break;
    }
    std::string category = std::string(kindName) + "/" +
                           (currentTexture == white ? "white" : std::to_string(currentTexture));
    RenderStats::DrawCall(category, (int)instances.size(), reason);

    glBindVertexArray(0);
    instances.clear();
}

void Canvas::Triangle(Vec2 a, Vec2 b, Vec2 c, Color color)
{
    Submit(MeshKind::Triangle, white, {a, b, c, c, 0, 0, 1, 1, color, 0});
}

void Canvas::Quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color)
{
    Submit(MeshKind::Quad, white, {a, b, c, d, 0, 0, 1, 1, color, 0});
}

void Canvas::Rect(float x, float y, float w, float h, Color color)
{
    Quad({x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, color);
}

void Canvas::Line(Vec2 a, Vec2 b, Color color, float thickness)
{
    float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
    if (len < .001f || thickness <= 0)
    {
        return;
    }
    Vec2 n = {-dy / len * thickness * .5f, dx / len * thickness * .5f};
    Quad(a + n, b + n, b - n, a - n, color);
}

void Canvas::Ellipse(Vec2 p, float rx, float ry, Color color, bool fill, float thickness)
{
    if (rx <= 0 || ry <= 0 || (!fill && thickness <= 0))
    {
        return;
    }
    Submit(fill ? MeshKind::Disc : MeshKind::Ring, white,
           {p + Vec2{-rx, -ry}, p + Vec2{rx, -ry}, p + Vec2{rx, ry}, p + Vec2{-rx, ry}, 0, 0, 1, 1,
            color, thickness});
}

void Canvas::Glow(Vec2 p, float rx, float ry, Color color)
{
    if (rx <= 0 || ry <= 0)
    {
        return;
    }
    Submit(MeshKind::Glow, white,
           {p + Vec2{-rx, -ry}, p + Vec2{rx, -ry}, p + Vec2{rx, ry}, p + Vec2{-rx, ry}, 0, 0, 1, 1,
            color, 0});
}

void Canvas::QueueGlow(Vec2 p, float rx, float ry, Color color)
{
    if (rx <= 0 || ry <= 0)
    {
        return;
    }
    queuedGlows.push_back({p, rx, ry, color});
}

void Canvas::FlushQueuedGlows()
{
    for (const QueuedGlow& g : queuedGlows)
    {
        Glow(g.p, g.rx, g.ry, g.color);
    }
    queuedGlows.clear();
}

void Canvas::Texture(GLuint texture, float x, float y, float w, float h, Color color)
{
    TextureRegion(texture, x, y, w, h, 0, 0, 1, 1, color);
}

void Canvas::TextureRegion(GLuint texture, float x, float y, float w, float h, float u0, float v0,
                           float u1, float v1, Color color)
{
    if (!texture)
    {
        return;
    }
    Submit(MeshKind::Quad, texture,
           {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, u0, v1, u1, v0, color, 0});
}

const Canvas::TextImage& Canvas::GetText(const std::wstring& text, int size, bool bold)
{
    const std::wstring key = std::to_wstring(size) + (bold ? L"b:" : L"r:") + text;
    auto it = textCache.find(key);
    if (it != textCache.end())
    {
        return it->second;
    }
    // Bounded text cache: dynamic distance labels must never grow GPU memory indefinitely.
    if (textCache.size() >= 384)
    {
        Flush(RenderStats::FlushReason::Other);
        for (const auto& item : textCache)
        {
            glDeleteTextures(1, &item.second.texture);
        }
        textCache.clear();
    }
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT font = CreateFontW(-size, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE extent = {};
    GetTextExtentPoint32W(dc, text.c_str(), (int)text.size(), &extent);
    int w = (std::max)(1L, extent.cx + 4), h = (std::max)(1L, extent.cy + 4);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap || !pixels)
    {
        SelectObject(dc, oldFont);
        DeleteObject(font);
        DeleteDC(dc);
        throw std::runtime_error("Unicode font rasterization failed");
    }
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    memset(pixels, 0, w * h * 4);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    TextOutW(dc, 2, 1, text.c_str(), (int)text.size());
    GdiFlush();
    std::vector<unsigned char> rgba(w * h * 4);
    const auto* src = static_cast<unsigned char*>(pixels);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int to = ((h - 1 - y) * w + x) * 4, from = (y * w + x) * 4;
            rgba[to] = rgba[to + 1] = rgba[to + 2] = 255;
            rgba[to + 3] = (std::max)(src[from], (std::max)(src[from + 1], src[from + 2]));
        }
    }
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    SelectObject(dc, oldFont);
    DeleteObject(font);
    DeleteDC(dc);
    TextImage result;
    result.width = w;
    result.height = h;
    glGenTextures(1, &result.texture);
    glBindTexture(GL_TEXTURE_2D, result.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return textCache.emplace(key, result).first->second;
}

float Canvas::Text(float x, float y, const std::wstring& text, int size, Color color, bool bold)
{
    if (text.empty())
    {
        return 0;
    }
    const auto& image = GetText(text, size, bold);
    Texture(image.texture, x - 2, y - 1, (float)image.width, (float)image.height, color);
    return (float)image.width - 4;
}

float Canvas::Measure(const std::wstring& text, int size, bool bold)
{
    return (float)GetText(text, size, bold).width - 4;
}
