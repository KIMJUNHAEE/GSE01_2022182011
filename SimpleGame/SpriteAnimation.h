#pragma once
#include "WorldMath.h"

// Pose 0 is idle; poses 1..32 form a loop. Phase follows collision-tested movement.
class SpriteAnimation
{
public:

    static constexpr int WalkFrames = 32;
    static constexpr float StrideLength = 144;
    void Advance(Vec2 screenDelta);
    int Frame() const;
    bool Flipped() const;
    bool Moving() const;

private:

    double phase = 0;
    bool moving = false, flipped = false;
};
