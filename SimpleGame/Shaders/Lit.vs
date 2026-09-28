#version 330

// Vertex layout matches MeshVertex in Mesh.h. Anim bits match AnimFlag.
// Locations 4-9 carry Renderer::Instance. Single draws leave them at identity and neutral
// defaults and place the mesh with u_Model instead.

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec3  a_Normal;
layout(location = 2) in vec3  a_Color;
layout(location = 3) in float a_Anim;
layout(location = 4) in mat4  i_Model;
layout(location = 8) in vec4  i_TintEmissive;
layout(location = 9) in vec2  i_PhaseFlash;

uniform mat4  u_ViewProj;
uniform mat4  u_Model;
uniform float u_Time;

out vec3  v_WorldPos;
out vec3  v_Normal;
out vec3  v_Color;
out vec3  v_Local;
out float v_Glow;
out vec3  v_Tint;
out float v_Emissive;
out float v_Flash;

const int ANIM_BREATHE = 1;
const int ANIM_SWAY = 2;
const int ANIM_PULSE = 4;
const int ANIM_HOVER = 8;
const int ANIM_SPIN = 16;
const int ANIM_BOB = 32;

void main()
{
	int anim = int(a_Anim + 0.5);
	float phase = i_PhaseFlash.x;
	mat4 model = u_Model * i_Model;
	vec3 p = a_Position;
	vec3 n = a_Normal;

	if ((anim & ANIM_BREATHE) != 0)
	{
		p.y *= 1.0 + 0.06 * sin(u_Time * 0.7 + phase);
	}

	// Walk bob: the phase carries the walk cycle instead of an ambient offset.
	if ((anim & ANIM_BOB) != 0)
	{
		p.y += abs(sin(phase)) * 0.05;
	}

	if ((anim & ANIM_SPIN) != 0)
	{
		float a = u_Time * 1.6 + phase;
		float c = cos(a);
		float s = sin(a);
		p.xz = vec2(c * p.x + s * p.z, -s * p.x + c * p.z);
		n.xz = vec2(c * n.x + s * n.z, -s * n.x + c * n.z);
	}

	if ((anim & ANIM_HOVER) != 0)
	{
		p.y += 0.08 * sin(u_Time * 1.3 + phase);
	}

	vec4 world = model * vec4(p, 1.0);

	// Sway grows with height, and world position keeps neighbouring trees out of step.
	if ((anim & ANIM_SWAY) != 0)
	{
		float wave = sin(u_Time * 1.1 + world.x * 0.35 + world.z * 0.27 + phase);
		float lean = max(p.y, 0.0);
		world.x += wave * 0.05 * lean;
		world.z += wave * 0.03 * lean;
	}

	v_Glow = 0.0;
	if ((anim & ANIM_PULSE) != 0)
	{
		v_Glow = 0.12 + 0.10 * sin(u_Time * 0.9 + phase);
	}

	v_WorldPos = world.xyz;
	v_Normal = normalize(transpose(inverse(mat3(model))) * n);
	v_Color = a_Color;
	v_Tint = i_TintEmissive.rgb;
	v_Emissive = i_TintEmissive.a;
	v_Flash = i_PhaseFlash.y;
	v_Local = a_Position;
	gl_Position = u_ViewProj * world;
}
