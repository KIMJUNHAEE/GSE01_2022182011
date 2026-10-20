#include "stdafx.h"
#include "SpriteSheet.h"
#include "Renderer.h"
#include "Dependencies/stb_image.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <utility>

bool SpriteSheet::Load(Renderer& renderer, const char* path, int columns, int rows,
                       const SpriteFrameGeometry& geometry)
{
    if (texture || columns <= 0 || rows <= 0 || columns > 64 || rows > 64)
    {
        return false;
    }
    stbi_set_flip_vertically_on_load(false);
    int width = 0, height = 0, channels = 0;
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(
        stbi_load(path, &width, &height, &channels, STBI_rgb_alpha), stbi_image_free);
    stbi_set_flip_vertically_on_load(true);
    if (!pixels || width < columns || height < rows)
    {
        std::cerr << "[Sprite] Invalid atlas dimensions: " << path << '\n';
        return false;
    }
    std::vector<FrameRegion> regions;
    for (int frame = 0; frame < columns * rows; ++frame)
    {
        // Integer boundaries also handle generated atlases that are not exactly divisible.
        const int column = frame % columns, row = frame / columns;
        const int ox = column * width / columns, oy = row * height / rows;
        const int cellWidth = (column + 1) * width / columns - ox;
        const int cellHeight = (row + 1) * height / rows - oy;
        if (geometry.bodyHeight > 0)
        {
            if (geometry.anchorX < 0 || geometry.anchorX > cellWidth || geometry.anchorY < 0 ||
                geometry.anchorY > cellHeight || geometry.bodyHeight > cellHeight)
            {
                std::cerr << "[Sprite] Invalid authored frame geometry: " << path << '\n';
                return false;
            }
            // Authored animation uses one canvas/pivot for every frame. Preserve the motion;
            // never rescale a raised knee or swaying coat independently each frame.
            regions.push_back({(ox + .5f) / width, 1.f - (oy + cellHeight - .5f) / height,
                               (ox + cellWidth - .5f) / width, 1.f - (oy + .5f) / height,
                               (float)cellWidth, (float)cellHeight, geometry.bodyHeight,
                               geometry.anchorX, geometry.anchorY});
            continue;
        }
        auto alpha = [&](int x, int y)
        {
            return pixels.get()[((size_t)(oy + y) * width + ox + x) * 4 + 3];
        };
        int left = cellWidth, top = cellHeight, right = -1, bottom = -1;
        for (int y = 0; y < cellHeight; ++y)
        {
            for (int x = 0; x < cellWidth; ++x)
            {
                if (alpha(x, y) < 32)
                {
                    continue;
                }
                left = (std::min)(left, x);
                right = (std::max)(right, x);
                top = (std::min)(top, y);
                bottom = (std::max)(bottom, y);
            }
        }
        if (right < left || bottom <= top)
        {
            std::cerr << "[Sprite] Empty frame " << frame << ": " << path << '\n';
            return false;
        }
        // Upper-body centroid does not drift with a swinging boot or trailing coat.
        double weightedX = 0, weight = 0;
        const int headEnd = top + (bottom - top + 1) / 3;
        for (int y = top; y < headEnd; ++y)
        {
            for (int x = left; x <= right; ++x)
            {
                const int a = alpha(x, y);
                if (a >= 32)
                {
                    weightedX += (x + .5) * a;
                    weight += a;
                }
            }
        }
        const float center = weight > 0 ? (float)(weightedX / weight) : (left + right + 1) * .5f;
        const int x0 = (std::max)(0, left - 2), y0 = (std::max)(0, top - 2);
        const int x1 = (std::min)(cellWidth, right + 3);
        const int y1 = (std::min)(cellHeight, bottom + 3);
        regions.push_back({(ox + x0 + .5f) / width, 1.f - (oy + y1 - .5f) / height,
                           (ox + x1 - .5f) / width, 1.f - (oy + y0 + .5f) / height,
                           (float)(x1 - x0), (float)(y1 - y0), (float)(bottom - top + 1),
                           center - x0, (float)(bottom + 1 - y0)});
    }
    texture = renderer.LoadTexture(path);
    if (!texture)
    {
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    frames = std::move(regions);
    std::cout << "[Sprite] " << path << " : " << width << 'x' << height << ", " << frames.size()
              << " frames; normalized height and foot anchor\n";
    return true;
}

void SpriteSheet::Draw(Canvas& canvas, int frame, Vec2 foot, float bodyHeight, bool flipped,
                       Color color) const
{
    if (frame < 0 || (size_t)frame >= frames.size() || bodyHeight <= 0)
    {
        return;
    }
    const auto& f = frames[frame];
    const float scale = bodyHeight / f.bodyHeight;
    const float anchorX = flipped ? f.width - f.anchorX : f.anchorX;
    canvas.TextureRegion(texture, foot.x - anchorX * scale, foot.y - f.anchorY * scale,
                         f.width * scale, f.height * scale, flipped ? f.u1 : f.u0, f.v0,
                         flipped ? f.u0 : f.u1, f.v1, color);
}

size_t SpriteSheet::FrameCount() const
{
    return frames.size();
}
