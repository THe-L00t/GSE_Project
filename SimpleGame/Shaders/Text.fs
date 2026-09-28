#version 330

in vec2 v_TexCoord;
in vec3 v_Color;

uniform sampler2D u_Atlas;

layout(location = 0) out vec4 FragColor;

void main()
{
	FragColor = vec4(v_Color, texture(u_Atlas, v_TexCoord).r);
}
