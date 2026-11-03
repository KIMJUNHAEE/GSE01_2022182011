#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in uvec4 a_Joints0;
layout(location = 4) in vec4 a_Weights0;
layout(location = 5) in uvec4 a_Joints1;
layout(location = 6) in vec4 a_Weights1;

layout(std140) uniform SkinBlock
{
    mat4 u_Bones[128];
};

uniform mat4 u_Root;
uniform mat4 u_Node;
uniform bool u_Skinned;
uniform vec2 u_Foot;
uniform float u_Zoom;
uniform vec2 u_CanvasSize;
uniform vec2 u_CanvasOrigin;
out vec3 normal;
out vec4 vertexColor;

void main()
{
    mat4 deformation = u_Node;
    if (u_Skinned)
    {
        deformation = u_Bones[a_Joints0.x] * a_Weights0.x + u_Bones[a_Joints0.y] * a_Weights0.y +
                      u_Bones[a_Joints0.z] * a_Weights0.z + u_Bones[a_Joints0.w] * a_Weights0.w +
                      u_Bones[a_Joints1.x] * a_Weights1.x + u_Bones[a_Joints1.y] * a_Weights1.y +
                      u_Bones[a_Joints1.z] * a_Weights1.z + u_Bones[a_Joints1.w] * a_Weights1.w;
    }
    mat4 transform = u_Root * deformation;
    vec3 p = (transform * vec4(a_Position, 1.0)).xyz;
    vec2 pixel = u_Foot + vec2((p.x - p.z) * 42.0, (p.x + p.z) * 21.0 - p.y * 42.0) * u_Zoom;
    pixel -= u_CanvasOrigin;
    gl_Position = vec4(pixel.x * 2.0 / u_CanvasSize.x - 1.0, 1.0 - pixel.y * 2.0 / u_CanvasSize.y,
                       clamp(-(p.x + p.z + p.y) * 0.002, -0.99, 0.99), 1.0);
    mat3 basis = mat3(transform);
    normal = abs(determinant(basis)) > 0.0000001 ? transpose(inverse(basis)) * a_Normal : a_Normal;
    vertexColor = a_Color;
}
