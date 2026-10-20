#pragma once
#include "Canvas.h"
#include <vector>

class Renderer;

struct SpriteFrameGeometry
{
    float bodyHeight = 0;
    float anchorX = 0, anchorY = 0;
};

// The renderer owns the cached texture. Alpha bounds/anchors are measured once at load.
class SpriteSheet
{
public:

    bool Load(Renderer& renderer, const char* path, int columns, int rows,
              const SpriteFrameGeometry& geometry = {});
    void Draw(Canvas& canvas, int frame, Vec2 foot, float bodyHeight, bool flipped,
              Color color = Color()) const;
    size_t FrameCount() const;

private:

    struct FrameRegion
    {
        float u0, v0, u1, v1;
        float width, height, bodyHeight, anchorX, anchorY;
    };

    GLuint texture = 0;
    std::vector<FrameRegion> frames;
};
