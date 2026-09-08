#version 330

in vec3 a_Position;
in vec2 a_TexCoord;

uniform vec4 u_Trans; // xy = position offset, zw = width/height scale

out vec2 v_TexCoord;

void main()
{
	vec4 newPosition;
	newPosition.x = a_Position.x * u_Trans.z + u_Trans.x;
	newPosition.y = a_Position.y * u_Trans.w + u_Trans.y;
	newPosition.z = 0;
	newPosition.w = 1;
	gl_Position = newPosition;

	v_TexCoord = a_TexCoord;
}
