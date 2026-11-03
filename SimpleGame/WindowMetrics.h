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
        // GLFW cursor coordinates are window units, not necessarily framebuffer pixels.
        const double pixelX = x * framebufferWidth / width;
        const double pixelY = y * framebufferHeight / height;
        const double scale = (std::min)(framebufferWidth / 1440.0, framebufferHeight / 900.0);
        return {float((pixelX - framebufferWidth * .5) / scale + 720),
                float((pixelY - framebufferHeight * .5) / scale + 450)};
    }
};

// The HUD retains its 1440x900 safe area; the world expands around it to fill any monitor.
struct CanvasView
{
    float width = 1440, height = 900;
    Vec2 origin;
    float pixelScale = 1;

    static CanvasView Fit(int width, int height)
    {
        if (width <= 0 || height <= 0)
        {
            return {};
        }
        const double scale = (std::min)(width / 1440.0, height / 900.0);
        CanvasView view;
        view.width = float(width / scale);
        view.height = float(height / scale);
        view.pixelScale = float(scale);
        view.origin = {(1440 - view.width) * .5f, (900 - view.height) * .5f};
        return view;
    }
};
