#version 330 core

in vec2 tex;
in vec4 tint;
in vec2 localPixel;
flat in vec2 radii;
flat in float shape;
flat in float thickness;

uniform sampler2D u_Image;

out vec4 color;

void main()
{
    color = texture(u_Image, tex) * tint;
    if (shape > 0.5)
    {
        vec2 q = localPixel / max(radii, vec2(0.0001));
        float radius = length(q);
        // Distance along the ellipse's local normal keeps ring thickness in canvas pixels.
        float gradient = length(q / max(radii, vec2(0.0001))) / max(radius, 0.0001);
        gradient = max(gradient, 1.0 / max(max(radii.x, radii.y), 0.0001));
        float distance = (radius - 1.0) / max(gradient, 0.0001);
        float aa = max(length(fwidth(localPixel)), 0.0001);
        float coverage;
        if (shape > 2.5)
        {
            coverage = max(0.0, 1.0 - radius);
        }
        else if (shape > 1.5)
        {
            coverage = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, abs(distance) - thickness * 0.5);
        }
        else
        {
            coverage = 1.0 - smoothstep(-aa * 0.5, aa * 0.5, distance);
        }
        color.a *= coverage;
    }
}
