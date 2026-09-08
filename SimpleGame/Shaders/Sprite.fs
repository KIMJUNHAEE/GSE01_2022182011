#version 330

in vec2 v_TexCoord;
layout(location=0) out vec4 FragColor;

uniform sampler2D u_Texture;
uniform float u_Alpha;

void main()
{
	vec4 texColor = texture(u_Texture, v_TexCoord);
	FragColor = vec4(texColor.rgb, texColor.a * u_Alpha);
}
