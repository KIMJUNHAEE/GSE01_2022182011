#pragma once
#include "WorldMath.h"
#include <algorithm>
#include <cstdint>

struct OutputViewport
{
    int x = 0, y = 0, width = 0, height = 0;

    static OutputViewport Fit(int width, int height, int logicalWidth = 1440,
                              int logicalHeight = 900)
    {
        if (width <= 0 || height <= 0 || logicalWidth <= 0 || logicalHeight <= 0)
        {
            return {};
        }
        const bool fitWidth = int64_t(width) * logicalHeight <= int64_t(height) * logicalWidth;
        const int w =
            fitWidth ? width : (std::max)(1, int(int64_t(height) * logicalWidth / logicalHeight));
        const int h =
            fitWidth ? (std::max)(1, int(int64_t(width) * logicalHeight / logicalWidth)) : height;
        return {(width - w) / 2, (height - h) / 2, w, h};
    }
};

struct WindowMetrics
{
    int width = 0, height = 0;
    int framebufferWidth = 0, framebufferHeight = 0;

    bool Drawable() const
    {
        return width > 0 && height > 0 && framebufferWidth > 0 && framebufferHeight > 0;
    }

    Vec2 ToCanvas(double x, double y) const
    {
        if (!Drawable())
        {
            return {-1, -1};
        }
        const auto viewport = OutputViewport::Fit(framebufferWidth, framebufferHeight);
        // GLFW cursor coordinates are window units, not necessarily framebuffer pixels.
        const double pixelX = x * framebufferWidth / width;
        const double pixelY = y * framebufferHeight / height;
        const int top = framebufferHeight - viewport.y - viewport.height;
        return {float((pixelX - viewport.x) * 1440 / viewport.width),
                float((pixelY - top) * 900 / viewport.height)};
    }
};
