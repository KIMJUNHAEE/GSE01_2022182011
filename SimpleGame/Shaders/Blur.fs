#version 330

// One direction of a separable 5-tap linear-sampled Gaussian blur. Called
// twice per pass (horizontal then vertical via u_Direction) by the Renderer.

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform vec2 u_TexelSize; // 1/width, 1/height of the SOURCE texture being sampled
uniform vec2 u_Direction; // (1,0) for horizontal, (0,1) for vertical

void main()
{
	vec2 off1 = u_Direction * u_TexelSize * 1.3846153846;
	vec2 off2 = u_Direction * u_TexelSize * 3.2307692308;

	vec3 result = texture(u_Texture, v_TexCoord).rgb * 0.2270270270;
	result += texture(u_Texture, v_TexCoord + off1).rgb * 0.3162162162;
	result += texture(u_Texture, v_TexCoord - off1).rgb * 0.3162162162;
	result += texture(u_Texture, v_TexCoord + off2).rgb * 0.0702702703;
	result += texture(u_Texture, v_TexCoord - off2).rgb * 0.0702702703;

	FragColor = vec4(result, 1.0);
}
