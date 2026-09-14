#version 330

in vec3 a_Position;
in vec3 a_Normal;

uniform mat4 u_ViewProj;
uniform mat4 u_Model;
uniform mat3 u_NormalMat;

out vec3 v_WorldPos;
out vec3 v_Normal;

void main()
{
	vec4 world = u_Model * vec4(a_Position, 1.0);
	v_WorldPos = world.xyz;
	v_Normal = normalize(u_NormalMat * a_Normal);
	gl_Position = u_ViewProj * world;
}
