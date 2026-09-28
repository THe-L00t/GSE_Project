#version 330

// Glyph quads in pixels with the origin at the top-left.

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec3 a_Color;

uniform vec2 u_Screen;

out vec2 v_TexCoord;
out vec3 v_Color;

void main()
{
	vec2 ndc = vec2(a_Position.x / u_Screen.x * 2.0 - 1.0, 1.0 - a_Position.y / u_Screen.y * 2.0);
	v_TexCoord = a_TexCoord;
	v_Color = a_Color;
	gl_Position = vec4(ndc, 0.0, 1.0);
}
