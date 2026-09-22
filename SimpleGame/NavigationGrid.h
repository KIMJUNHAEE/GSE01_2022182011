#pragma once
#include "WorldMath.h"
#include <functional>
#include <vector>

// Reusable floor graph; no dependency on actors, combat, rendering, or LevelOne.
class NavigationGrid
{
public:

    using WalkTest = std::function<bool(Vec2, float)>;
    using PathTest = std::function<bool(Vec2, Vec2, float)>;
    NavigationGrid(Vec2 origin = {-10, -17}, int width = 61, int height = 45, float step = .5f);
    void Rebuild(const WalkTest& walk);
    int Nearest(Vec2 position, const PathTest& path) const;
    bool Connected(Vec2 start, const PathTest& path) const;
    void BuildFlow(Vec2 target, const PathTest& path);
    Vec2 NextTarget(Vec2 position, Vec2 target, const PathTest& path) const;
    Vec2 Position(int index) const;
    bool IsOpen(int index) const;
    int Count() const;

private:

    Vec2 origin;
    int width, height;
    float step;
    int source = -1;
    std::vector<unsigned char> open, edges;
    std::vector<int> flow;
    int Neighbor(int index, int direction) const;
};
