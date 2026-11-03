#pragma once
#include <algorithm>
#include <array>
#include <cmath>

struct ModelVec3
{
    float x = 0, y = 0, z = 0;
};

struct ModelVec4
{
    float x = 0, y = 0, z = 0, w = 0;
};

inline ModelVec4 operator+(ModelVec4 a, ModelVec4 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

inline ModelVec4 operator*(ModelVec4 a, float s)
{
    return {a.x * s, a.y * s, a.z * s, a.w * s};
}

inline ModelVec4 NormalizeQuaternion(ModelVec4 q)
{
    float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return length > 1e-8f ? q * (1.f / length) : ModelVec4{0, 0, 0, 1};
}

inline ModelVec4 Slerp(ModelVec4 a, ModelVec4 b, float t)
{
    a = NormalizeQuaternion(a);
    b = NormalizeQuaternion(b);
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0)
    {
        b = b * -1;
        dot = -dot;
    }
    dot = std::clamp(dot, 0.f, 1.f);
    if (dot > .9995f)
    {
        return NormalizeQuaternion(a * (1 - t) + b * t);
    }
    float angle = std::acos(dot), divisor = std::sin(angle);
    return a * (std::sin((1 - t) * angle) / divisor) + b * (std::sin(t * angle) / divisor);
}

// Column-major, column vectors; exactly the glTF/OpenGL matrix convention.
struct ModelMatrix
{
    std::array<float, 16> values = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

inline ModelMatrix operator*(const ModelMatrix& a, const ModelMatrix& b)
{
    ModelMatrix result;
    result.values.fill(0);
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int k = 0; k < 4; ++k)
            {
                result.values[column * 4 + row] += a.values[k * 4 + row] * b.values[column * 4 + k];
            }
        }
    }
    return result;
}

inline ModelVec3 TransformPoint(const ModelMatrix& matrix, ModelVec3 point)
{
    const auto& m = matrix.values;
    return {m[0] * point.x + m[4] * point.y + m[8] * point.z + m[12],
            m[1] * point.x + m[5] * point.y + m[9] * point.z + m[13],
            m[2] * point.x + m[6] * point.y + m[10] * point.z + m[14]};
}

inline ModelMatrix ModelTRS(ModelVec3 translation, ModelVec4 rotation, ModelVec3 scale)
{
    const auto q = NormalizeQuaternion(rotation);
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    ModelMatrix result;
    result.values = {(1 - 2 * y * y - 2 * z * z) * scale.x,
                     (2 * x * y + 2 * z * w) * scale.x,
                     (2 * x * z - 2 * y * w) * scale.x,
                     0,
                     (2 * x * y - 2 * z * w) * scale.y,
                     (1 - 2 * x * x - 2 * z * z) * scale.y,
                     (2 * y * z + 2 * x * w) * scale.y,
                     0,
                     (2 * x * z + 2 * y * w) * scale.z,
                     (2 * y * z - 2 * x * w) * scale.z,
                     (1 - 2 * x * x - 2 * y * y) * scale.z,
                     0,
                     translation.x,
                     translation.y,
                     translation.z,
                     1};
    return result;
}
