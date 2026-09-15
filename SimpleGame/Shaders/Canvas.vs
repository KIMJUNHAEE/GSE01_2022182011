#version 330 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_Edge;
layout(location = 2) in vec2 a_StrokeAlpha;

layout(location = 3) in vec2 i_CornerA;
layout(location = 4) in vec2 i_CornerB;
layout(location = 5) in vec2 i_CornerC;
layout(location = 6) in vec2 i_CornerD;
layout(location = 7) in vec4 i_UVRect;
layout(location = 8) in vec4 i_Color;
layout(location = 9) in float i_Thickness;

uniform vec2 u_CanvasSize;

out vec2 tex;
out vec4 tint;

void main()
{
    vec2 top = mix(i_CornerA, i_CornerB, a_Position.x);
    vec2 bottom = mix(i_CornerD, i_CornerC, a_Position.x);
    vec2 position = mix(top, bottom, a_Position.y);

    // Stroke normals are computed after scaling so thin ellipses retain pixel-width outlines.
    vec2 edge = (i_CornerB - i_CornerA) * a_Edge.x + (i_CornerD - i_CornerA) * a_Edge.y;
    float edgeLength = length(edge);
    if (edgeLength > 0.0001)
    {
        position += vec2(-edge.y, edge.x) / edgeLength * a_StrokeAlpha.x * i_Thickness;
    }

    gl_Position = vec4(position.x * 2.0 / u_CanvasSize.x - 1.0,
                       1.0 - position.y * 2.0 / u_CanvasSize.y, 0.0, 1.0);
    tex = mix(i_UVRect.xy, i_UVRect.zw, a_Position);
    tint = vec4(i_Color.rgb, i_Color.a * a_StrokeAlpha.y);
}
