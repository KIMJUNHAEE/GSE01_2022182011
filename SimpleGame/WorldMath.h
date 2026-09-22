#pragma once

struct Vec2
{
    float x = 0, y = 0;
};

inline Vec2 operator+(Vec2 a, Vec2 b)
{
    return {a.x + b.x, a.y + b.y};
}

inline Vec2 operator-(Vec2 a, Vec2 b)
{
    return {a.x - b.x, a.y - b.y};
}

inline Vec2 operator*(Vec2 a, float scale)
{
    return {a.x * scale, a.y * scale};
}
