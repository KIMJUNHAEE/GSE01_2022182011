#version 330 core

layout(location = 0) in vec2 a_Position;

layout(location = 3) in vec2 i_CornerA;
layout(location = 4) in vec2 i_CornerB;
layout(location = 5) in vec2 i_CornerC;
layout(location = 6) in vec2 i_CornerD;
layout(location = 7) in vec4 i_UVRect;
layout(location = 8) in vec4 i_Color;
layout(location = 9) in float i_Thickness;
layout(location = 10) in float i_Shape;
layout(location = 11) in vec2 i_Radii;

uniform vec2 u_CanvasSize;
uniform vec2 u_CanvasOrigin;

out vec2 tex;
out vec4 tint;
out vec2 localPixel;
flat out vec2 radii;
flat out float shape;
flat out float thickness;

void main()
{
    vec2 top = mix(i_CornerA, i_CornerB, a_Position.x);
    vec2 bottom = mix(i_CornerD, i_CornerC, a_Position.x);
    vec2 position = mix(top, bottom, a_Position.y);

    localPixel =
        (a_Position - 0.5) * vec2(length(i_CornerB - i_CornerA), length(i_CornerD - i_CornerA));
    radii = i_Radii;
    shape = i_Shape;
    thickness = i_Thickness;
    position -= u_CanvasOrigin;
    gl_Position = vec4(position.x * 2.0 / u_CanvasSize.x - 1.0,
                       1.0 - position.y * 2.0 / u_CanvasSize.y, 0.0, 1.0);
    tex = mix(i_UVRect.xy, i_UVRect.zw, a_Position);
    tint = i_Color;
}
