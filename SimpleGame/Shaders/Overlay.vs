#version 330

// Screen-space quad. a_Position is a unit quad in [0,1]^2, mapped into
// u_Rect = (x, y, width, height) in normalized device coordinates.

in vec3 a_Position;

uniform vec4 u_Rect;

out vec2 v_UV;

void main()
{
	vec2 p = u_Rect.xy + a_Position.xy * u_Rect.zw;
	v_UV = a_Position.xy;
	gl_Position = vec4(p, 0.0, 1.0);
}
