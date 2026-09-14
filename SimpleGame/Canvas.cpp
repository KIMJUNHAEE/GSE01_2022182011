#include "stdafx.h"
#include "Canvas.h"
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace {
GLuint Shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048] = {}; glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader); throw std::runtime_error(log);
    }
    return shader;
}
}

Canvas::Canvas() {
    const char* vs = "#version 330\nlayout(location=0) in vec2 p; layout(location=1) in vec2 uv;"
        "layout(location=2) in vec4 c; out vec2 tex; out vec4 tint;"
        "void main(){gl_Position=vec4(p.x/720.0-1.0,1.0-p.y/450.0,0,1);tex=uv;tint=c;}";
    const char* fs = "#version 330\nin vec2 tex; in vec4 tint; out vec4 color; uniform sampler2D image;"
        "void main(){color=texture(image,tex)*tint;}";
    GLuint v = Shader(GL_VERTEX_SHADER, vs), f = Shader(GL_FRAGMENT_SHADER, fs);
    program = glCreateProgram(); glAttachShader(program, v); glAttachShader(program, f);
    glLinkProgram(program); glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) throw std::runtime_error("Canvas program could not be linked");
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < 3; ++i) glEnableVertexAttribArray(i);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(4 * sizeof(float)));
    glBindVertexArray(0);
    unsigned char pixel[] = { 255, 255, 255, 255 };
    glGenTextures(1, &white); glBindTexture(GL_TEXTURE_2D, white);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    vertices.reserve(65536);
}
Canvas::~Canvas() {
    for (const auto& item : textCache) glDeleteTextures(1, &item.second.texture);
    glDeleteTextures(1, &white); glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao); glDeleteProgram(program);
}
void Canvas::Select(GLuint texture) {
    if (texture != currentTexture || vertices.size() > 60000) Flush();
    currentTexture = texture;
}
void Canvas::Push(Vec2 p, Vec2 uv, Color color) {
    vertices.push_back({ p.x,p.y,uv.x,uv.y,color.r,color.g,color.b,color.a });
}
void Canvas::Flush() {
    if (vertices.empty()) return;
    glUseProgram(program); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, currentTexture);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STREAM_DRAW);
    glEnable(GL_BLEND); glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertices.size());
    glBindVertexArray(0); vertices.clear();
}
void Canvas::Triangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
    Select(white); Push(a, {}, color); Push(b, {}, color); Push(c, {}, color);
}
void Canvas::Quad(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color) {
    Triangle(a, b, c, color); Triangle(a, c, d, color);
}
void Canvas::Rect(float x, float y, float w, float h, Color color) {
    Quad({ x,y }, { x + w,y }, { x + w,y + h }, { x,y + h }, color);
}
void Canvas::Line(Vec2 a, Vec2 b, Color color, float thickness) {
    float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;
    Vec2 n = { -dy / len * thickness * .5f, dx / len * thickness * .5f };
    Quad(a + n, b + n, b - n, a - n, color);
}
void Canvas::Ellipse(Vec2 p, float rx, float ry, Color color, bool fill, float thickness) {
    const int segments = 48;
    for (int i = 0; i < segments; ++i) {
        float a = i * 6.2831853f / segments, b = (i + 1) * 6.2831853f / segments;
        Vec2 u = p + Vec2{ std::cos(a) * rx,std::sin(a) * ry };
        Vec2 v = p + Vec2{ std::cos(b) * rx,std::sin(b) * ry };
        if (fill) Triangle(p, u, v, color); else Line(u, v, color, thickness);
    }
}
void Canvas::Glow(Vec2 p, float rx, float ry, Color color) {
    Select(white);
    for (int i = 0; i < 48; ++i) {
        float a = i * 6.2831853f / 48, b = (i + 1) * 6.2831853f / 48;
        Push(p, {}, color);
        Push(p + Vec2{ std::cos(a) * rx,std::sin(a) * ry }, {}, color.Fade(0));
        Push(p + Vec2{ std::cos(b) * rx,std::sin(b) * ry }, {}, color.Fade(0));
    }
}
void Canvas::Texture(GLuint texture, float x, float y, float w, float h, Color color) {
    TextureRegion(texture,x,y,w,h,0,0,1,1,color);
}
void Canvas::TextureRegion(GLuint texture, float x, float y, float w, float h, float u0, float v0, float u1, float v1, Color color) {
    if (!texture) return;
    Select(texture);
    Push({ x,y }, { u0,v1 }, color); Push({ x + w,y }, { u1,v1 }, color); Push({ x + w,y + h }, { u1,v0 }, color);
    Push({ x,y }, { u0,v1 }, color); Push({ x + w,y + h }, { u1,v0 }, color); Push({ x,y + h }, { u0,v0 }, color);
}
const Canvas::TextImage& Canvas::GetText(const std::wstring& text, int size, bool bold) {
    const std::wstring key = std::to_wstring(size) + (bold ? L"b:" : L"r:") + text;
    auto it = textCache.find(key); if (it != textCache.end()) return it->second;
    // Bounded text cache: dynamic distance labels must never grow GPU memory indefinitely.
    if (textCache.size() >= 384) {
        Flush(); for (const auto& item : textCache) glDeleteTextures(1, &item.second.texture);
        textCache.clear();
    }
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT font = CreateFontW(-size, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH, L"Malgun Gothic");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SIZE extent = {}; GetTextExtentPoint32W(dc, text.c_str(), (int)text.size(), &extent);
    int w = (std::max)(1L, extent.cx + 4), h = (std::max)(1L, extent.cy + 4);
    BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap || !pixels) { SelectObject(dc, oldFont); DeleteObject(font); DeleteDC(dc); throw std::runtime_error("Unicode font rasterization failed"); }
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    memset(pixels, 0, w * h * 4); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255, 255, 255));
    TextOutW(dc, 2, 1, text.c_str(), (int)text.size()); GdiFlush();
    std::vector<unsigned char> rgba(w * h * 4);
    const auto* src = static_cast<unsigned char*>(pixels);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        int to = ((h - 1 - y) * w + x) * 4, from = (y * w + x) * 4;
        rgba[to] = rgba[to + 1] = rgba[to + 2] = 255;
        rgba[to + 3] = (std::max)(src[from], (std::max)(src[from + 1], src[from + 2]));
    }
    SelectObject(dc, oldBitmap); DeleteObject(bitmap); SelectObject(dc, oldFont); DeleteObject(font); DeleteDC(dc);
    TextImage result; result.width = w; result.height = h;
    glGenTextures(1, &result.texture); glBindTexture(GL_TEXTURE_2D, result.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return textCache.emplace(key, result).first->second;
}
float Canvas::Text(float x, float y, const std::wstring& text, int size, Color color, bool bold) {
    if (text.empty()) return 0;
    const auto& image = GetText(text, size, bold);
    Texture(image.texture, x - 2, y - 1, (float)image.width, (float)image.height, color);
    return (float)image.width - 4;
}
float Canvas::Measure(const std::wstring& text, int size, bool bold) { return (float)GetText(text, size, bold).width - 4; }
