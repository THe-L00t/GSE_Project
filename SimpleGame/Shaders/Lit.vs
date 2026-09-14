#version 330

// Vertex layout matches MeshVertex in Mesh.h. Anim bits match AnimFlag.

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec3  a_Normal;
layout(location = 2) in vec3  a_Color;
layout(location = 3) in float a_Anim;

uniform mat4  u_ViewProj;
uniform mat4  u_Model;
uniform mat3  u_NormalMat;
uniform float u_Time;
uniform float u_Phase;

out vec3  v_WorldPos;
out vec3  v_Normal;
out vec3  v_Color;
out vec3  v_Local;
out float v_Glow;

const int ANIM_BREATHE = 1;
const int ANIM_SWAY = 2;
const int ANIM_PULSE = 4;
const int ANIM_HOVER = 8;
const int ANIM_SPIN = 16;
const int ANIM_BOB = 32;

void main()
{
	int anim = int(a_Anim + 0.5);
	vec3 p = a_Position;
	vec3 n = a_Normal;

	if ((anim & ANIM_BREATHE) != 0)
	{
		p.y *= 1.0 + 0.06 * sin(u_Time * 0.7 + u_Phase);
	}

	// Walk bob: the phase carries the walk cycle instead of an ambient offset.
	if ((anim & ANIM_BOB) != 0)
	{
		p.y += abs(sin(u_Phase)) * 0.05;
	}

	if ((anim & ANIM_SPIN) != 0)
	{
		float a = u_Time * 1.6 + u_Phase;
		float c = cos(a);
		float s = sin(a);
		p.xz = vec2(c * p.x + s * p.z, -s * p.x + c * p.z);
		n.xz = vec2(c * n.x + s * n.z, -s * n.x + c * n.z);
	}

	if ((anim & ANIM_HOVER) != 0)
	{
		p.y += 0.08 * sin(u_Time * 1.3 + u_Phase);
	}

	vec4 world = u_Model * vec4(p, 1.0);

	// Sway grows with height, and world position keeps neighbouring trees out of step.
	if ((anim & ANIM_SWAY) != 0)
	{
		float wave = sin(u_Time * 1.1 + world.x * 0.35 + world.z * 0.27 + u_Phase);
		float lean = max(p.y, 0.0);
		world.x += wave * 0.05 * lean;
		world.z += wave * 0.03 * lean;
	}

	v_Glow = 0.0;
	if ((anim & ANIM_PULSE) != 0)
	{
		v_Glow = 0.12 + 0.10 * sin(u_Time * 0.9 + u_Phase);
	}

	v_WorldPos = world.xyz;
	v_Normal = normalize(u_NormalMat * n);
	v_Color = a_Color;
	v_Local = a_Position;
	gl_Position = u_ViewProj * world;
}
