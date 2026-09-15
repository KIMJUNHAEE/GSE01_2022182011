#version 330 core

in vec2 tex;
in vec4 tint;

uniform sampler2D u_Image;

out vec4 color;

void main()
{
    color = texture(u_Image, tex) * tint;
}
