#include "stdafx.h"
#include "SpriteAnimation.h"
#include <cmath>

void SpriteAnimation::Advance(Vec2 screenDelta)
{
    const double distance = std::hypot(screenDelta.x, screenDelta.y);
    moving = std::isfinite(distance) && distance > .001;
    if (!moving)
    {
        return;
    }
    if (std::abs(screenDelta.x) > .001f)
    {
        flipped = screenDelta.x < 0;
    }
    phase = std::fmod(phase + distance / StrideLength, 1.0);
}

int SpriteAnimation::Frame() const
{
    return moving ? 1 + (int)(phase * WalkFrames) : 0;
}

bool SpriteAnimation::Flipped() const
{
    return flipped;
}

bool SpriteAnimation::Moving() const
{
    return moving;
}
