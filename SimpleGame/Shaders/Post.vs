#version 330

// Full-screen pass over the unit quad in [0,1]^2.

in vec3 a_Position;

out vec2 v_UV;

void main()
{
	v_UV = a_Position.xy;
	gl_Position = vec4(a_Position.xy * 2.0 - 1.0, 0.0, 1.0);
}
