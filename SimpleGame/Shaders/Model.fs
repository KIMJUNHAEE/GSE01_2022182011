#version 330 core
in vec3 normal;
in vec4 vertexColor;
uniform vec4 u_Color;
uniform vec3 u_Emissive;
uniform float u_AlphaCutoff;
uniform bool u_Unlit;
out vec4 fragColor;

void main()
{
    vec4 base = u_Color * vertexColor;
    if (base.a < u_AlphaCutoff || base.a <= 0.001)
    {
        discard;
    }
    float light =
        u_Unlit ? 1.0
                : 0.38 + 0.62 * max(dot(normalize(normal), normalize(vec3(-0.5, 1.0, 0.6))), 0.0);
    vec3 linearColor = base.rgb * light + u_Emissive;
    fragColor = vec4(pow(max(linearColor, vec3(0.0)), vec3(1.0 / 2.2)), base.a);
}
