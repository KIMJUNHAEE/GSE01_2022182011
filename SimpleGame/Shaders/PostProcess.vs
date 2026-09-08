#version 330

// Shared vertex shader for full-screen post-process passes: the quad already
// covers the whole NDC range, so there's no camera/position transform here -
// just pass the position and UV straight through.

in vec3 a_Position;
in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main()
{
	gl_Position = vec4(a_Position.xy, 0.0, 1.0);
	v_TexCoord = a_TexCoord;
}
