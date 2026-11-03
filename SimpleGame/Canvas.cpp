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
    try
    {
        program = LoadShaderProgram("Shaders/Canvas.vs", "Shaders/Canvas.fs");
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program, "u_Image"), 0);
        sizeUniform = glGetUniformLocation(program, "u_CanvasSize");
        originUniform = glGetUniformLocation(program, "u_CanvasOrigin");
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
        glDeleteVertexArrays(1, &item.vao);
        glDeleteBuffers(1, &item.vbo);
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

const Canvas::CachedMesh& Canvas::GetMesh()
{
    if (!meshes.empty())
    {
        return meshes.front();
    }
    const std::vector<MeshVertex> vertices = {{{0, 0}}, {{1, 0}}, {{1, 1}},
                                              {{0, 0}}, {{1, 1}}, {{0, 1}}};
    meshes.emplace_back();
    CachedMesh& mesh = meshes.back();
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    if (!mesh.vao || !mesh.vbo)
    {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
        meshes.clear();
        throw std::runtime_error("Canvas mesh cache allocation failed");
    }
    mesh.count = (GLsizei)vertices.size();
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(MeshVertex), vertices.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), nullptr);
    glVertexAttribDivisor(0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
    const size_t instanceOffsets[] = {
        offsetof(Instance, a),         offsetof(Instance, b),     offsetof(Instance, c),
        offsetof(Instance, d),         offsetof(Instance, u0),    offsetof(Instance, color),
        offsetof(Instance, thickness), offsetof(Instance, shape), offsetof(Instance, radii)};
    const GLint components[] = {2, 2, 2, 2, 4, 4, 1, 1, 2};
    for (GLuint i = 0; i < 9; ++i)
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

void Canvas::SetView(const CanvasView& value)
{
    Flush();
    view = value;
    fontScale = std::clamp(std::round(view.pixelScale * 4) / 4, .5f, 4.f);
}

void Canvas::Fill(Color color)
{
    Rect(view.origin.x, view.origin.y, view.width, view.height, color);
}

void Canvas::Submit(GLuint texture, const Instance& instance)
{
    // Do not sort transparent geometry: only consecutive compatible draws may be batched.
    if (texture != currentTexture || instances.size() >= InstanceCapacity)
    {
        Flush(texture != currentTexture ? RenderStats::FlushReason::TextureChanged
                                        : RenderStats::FlushReason::CapacityReached);
    }
    currentTexture = texture;
    instances.push_back(instance);
}

void Canvas::Flush(RenderStats::FlushReason reason)
{
    if (instances.empty())
    {
        return;
    }
    const CachedMesh& mesh = GetMesh();
    glUseProgram(program);
    glUniform2f(sizeUniform, view.width, view.height);
    glUniform2f(originUniform, view.origin.x, view.origin.y);
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

    std::string category = std::string("Canvas/") +
                           (currentTexture == white ? "white" : std::to_string(currentTexture));
    RenderStats::DrawCall(category, (int)instances.size(), reason);

    glBindVertexArray(0);
    instances.clear();
}

void Canvas::Triangle(Vec2 a, Vec2 b, Vec2 c, Color color)
{
    Submit(white, {a, b, c, c, 0, 0, 1, 1, color, 0});
}

void Canvas::Quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color)
{
    Submit(white, {a, b, c, d, 0, 0, 1, 1, color, 0});
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
    const float padding = fill ? 1 : thickness * .5f + 1;
    const Vec2 extent{rx + padding, ry + padding};
    Submit(white, {p - extent,
                   p + Vec2{extent.x, -extent.y},
                   p + extent,
                   p + Vec2{-extent.x, extent.y},
                   0,
                   0,
                   1,
                   1,
                   color,
                   thickness,
                   fill ? 1.f : 2.f,
                   {rx, ry}});
}

void Canvas::Glow(Vec2 p, float rx, float ry, Color color)
{
    if (rx <= 0 || ry <= 0)
    {
        return;
    }
    Submit(white, {p + Vec2{-rx, -ry},
                   p + Vec2{rx, -ry},
                   p + Vec2{rx, ry},
                   p + Vec2{-rx, ry},
                   0,
                   0,
                   1,
                   1,
                   color,
                   0,
                   3,
                   {rx, ry}});
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
    Submit(texture, {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}, u0, v1, u1, v0, color, 0});
}

const Canvas::TextImage& Canvas::GetText(const std::wstring& text, int size, bool bold)
{
    size = (std::max)(1, int(std::round(size * fontScale)));
    const std::wstring key = std::to_wstring(size) + (bold ? L"b:" : L"r:") + text;
    auto it = textCache.find(key);
    if (it != textCache.end())
    {
        it->second.lastUse = ++textUse;
        return it->second;
    }
    // Evict only the least recently used label. Changing distance/HP must not discard the HUD.
    if (textCache.size() >= 384)
    {
        auto oldest = std::min_element(textCache.begin(), textCache.end(),
                                       [](const auto& a, const auto& b)
                                       {
                                           return a.second.lastUse < b.second.lastUse;
                                       });
        if (oldest->second.texture == currentTexture)
        {
            Flush(RenderStats::FlushReason::Other);
        }
        glDeleteTextures(1, &oldest->second.texture);
        textCache.erase(oldest);
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
    result.lastUse = ++textUse;
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
    Texture(image.texture, x - 2 / fontScale, y - 1 / fontScale, image.width / fontScale,
            image.height / fontScale, color);
    return (image.width - 4) / fontScale;
}

float Canvas::Measure(const std::wstring& text, int size, bool bold)
{
    return (GetText(text, size, bold).width - 4) / fontScale;
}
