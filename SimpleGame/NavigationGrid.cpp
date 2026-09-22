#include "stdafx.h"
#include "NavigationGrid.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <stdexcept>

namespace
{
    constexpr int DX[] = {1, -1, 0, 0};
    constexpr int DY[] = {0, 0, 1, -1};

    float Distance(Vec2 a, Vec2 b)
    {
        Vec2 d = a - b;
        return std::sqrt(d.x * d.x + d.y * d.y);
    }
} // namespace

NavigationGrid::NavigationGrid(Vec2 origin, int width, int height, float step)
    : origin(origin), width(width), height(height), step(step)
{
    if (width <= 0 || height <= 0 || step <= 0 || width > 4096 || height > 4096)
    {
        throw std::invalid_argument("Invalid navigation grid dimensions");
    }
    open.resize(width * height);
    edges.resize(width * height);
    flow.resize(width * height, -1);
}

Vec2 NavigationGrid::Position(int index) const
{
    return origin + Vec2{(index % width) * step, (index / width) * step};
}

bool NavigationGrid::IsOpen(int index) const
{
    return index >= 0 && index < Count() && open[index] != 0;
}

int NavigationGrid::Count() const
{
    return width * height;
}

int NavigationGrid::Neighbor(int index, int direction) const
{
    int x = index % width + DX[direction], y = index / width + DY[direction];
    return x < 0 || y < 0 || x >= width || y >= height ? -1 : y * width + x;
}

void NavigationGrid::Rebuild(const WalkTest& walk)
{
    std::fill(edges.begin(), edges.end(), 0);
    std::fill(flow.begin(), flow.end(), -1);
    source = -1;
    for (int i = 0; i < Count(); ++i)
    {
        open[i] = walk(Position(i), .28f);
    }
    for (int i = 0; i < Count(); ++i)
    {
        if (!IsOpen(i))
        {
            continue;
        }
        for (int d = 0; d < 4; ++d)
        {
            int next = Neighbor(i, d);
            if (!IsOpen(next))
            {
                continue;
            }
            bool clear = true;
            int steps = (std::max)(1, (int)std::ceil(step / .12f));
            for (int n = 1; n < steps && clear; ++n)
            {
                clear =
                    walk(Position(i) + (Position(next) - Position(i)) * ((float)n / steps), .28f);
            }
            if (clear)
            {
                edges[i] |= (1 << d);
            }
        }
    }
}

int NavigationGrid::Nearest(Vec2 position, const PathTest& path) const
{
    int cx = (int)std::round((position.x - origin.x) / step);
    int cy = (int)std::round((position.y - origin.y) / step);
    int result = -1;
    float best = 100000;
    for (int y = (std::max)(0, cy - 2); y <= (std::min)(height - 1, cy + 2); ++y)
    {
        for (int x = (std::max)(0, cx - 2); x <= (std::min)(width - 1, cx + 2); ++x)
        {
            int i = y * width + x;
            float distance = Distance(position, Position(i));
            if (IsOpen(i) && distance < best && path(position, Position(i), .22f))
            {
                best = distance;
                result = i;
            }
        }
    }
    return result;
}

bool NavigationGrid::Connected(Vec2 start, const PathTest& path) const
{
    int first = Nearest(start, path);
    if (first < 0)
    {
        return false;
    }
    std::vector<bool> visited(Count(), false);
    std::queue<int> pending;
    pending.push(first);
    visited[first] = true;
    while (!pending.empty())
    {
        int current = pending.front();
        pending.pop();
        for (int d = 0; d < 4; ++d)
        {
            int next = Neighbor(current, d);
            if ((edges[current] & (1 << d)) && !visited[next])
            {
                visited[next] = true;
                pending.push(next);
            }
        }
    }
    for (int i = 0; i < Count(); ++i)
    {
        if (IsOpen(i) && !visited[i])
        {
            return false;
        }
    }
    return true;
}

void NavigationGrid::BuildFlow(Vec2 target, const PathTest& path)
{
    int first = Nearest(target, path);
    if (first == source)
    {
        return;
    }
    source = first;
    std::fill(flow.begin(), flow.end(), -1);
    if (first < 0)
    {
        return;
    }
    std::queue<int> pending;
    pending.push(first);
    flow[first] = 0;
    while (!pending.empty())
    {
        int current = pending.front();
        pending.pop();
        for (int d = 0; d < 4; ++d)
        {
            int next = Neighbor(current, d);
            if ((edges[current] & (1 << d)) && flow[next] < 0)
            {
                flow[next] = flow[current] + 1;
                pending.push(next);
            }
        }
    }
}

Vec2 NavigationGrid::NextTarget(Vec2 position, Vec2 target, const PathTest& path) const
{
    if (path(position, target, .22f))
    {
        return target;
    }
    int current = Nearest(position, path);
    if (current < 0 || flow[current] < 0)
    {
        return position;
    }
    int best = current;
    for (int d = 0; d < 4; ++d)
    {
        int next = Neighbor(current, d);
        if ((edges[current] & (1 << d)) && flow[next] >= 0 && flow[next] < flow[best] &&
            path(position, Position(next), .22f))
        {
            best = next;
        }
    }
    return Position(best);
}
