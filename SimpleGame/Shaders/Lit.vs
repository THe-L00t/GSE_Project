#version 330

// Vertex layout matches MeshVertex in Mesh.h for shape recipes and AssetVertex in Asset.h for
// Blender-made models. Anim bits match AnimFlag.
// Locations 4-10 carry Renderer::Instance. Single draws leave them at identity and neutral
// defaults and place the mesh with u_Model instead. Recipes leave 11-14 at constants.

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec3  a_Normal;
layout(location = 2) in vec4  a_Color;       // a: where moss settles first (Blender models)
layout(location = 3) in float a_Anim;
layout(location = 4) in mat4  i_Model;
layout(location = 8) in vec4  i_TintEmissive;
layout(location = 9) in vec4  i_PhaseFlash;   // phase, flash, dissolve, cutaway
layout(location = 10) in vec4 i_Extra;        // first bone in u_Bones (-1 none), moss growth
layout(location = 11) in vec2 a_UV;
layout(location = 12) in vec4 a_Tangent;      // w: bitangent sign
layout(location = 13) in vec4 a_Joints;
layout(location = 14) in vec4 a_Weights;

uniform mat4  u_ViewProj;
uniform mat4  u_Model;
uniform float u_Time;
uniform samplerBuffer u_Bones;               // three rows of each bone's skinning matrix

out vec3  v_WorldPos;
out vec3  v_Normal;
out vec3  v_Color;
out vec3  v_Local;
out float v_Glow;
out vec3  v_Tint;
out float v_Emissive;
out float v_Flash;
out float v_Dissolve;
out float v_Cutaway;
out vec2  v_UV;
out vec4  v_Tangent;
out float v_MossMask;
out float v_MossGrowth;
out float v_Foliage;

const int ANIM_BREATHE = 1;
const int ANIM_SWAY = 2;
const int ANIM_PULSE = 4;
const int ANIM_HOVER = 8;
const int ANIM_SPIN = 16;
const int ANIM_BOB = 32;
const int ANIM_FOLIAGE = 64;

mat4 BoneMatrix(int index)
{
	vec4 r0 = texelFetch(u_Bones, index * 3);
	vec4 r1 = texelFetch(u_Bones, index * 3 + 1);
	vec4 r2 = texelFetch(u_Bones, index * 3 + 2);
	return mat4(vec4(r0.x, r1.x, r2.x, 0.0),
				vec4(r0.y, r1.y, r2.y, 0.0),
				vec4(r0.z, r1.z, r2.z, 0.0),
				vec4(r0.w, r1.w, r2.w, 1.0));
}

void main()
{
	int anim = int(a_Anim + 0.5);
	float phase = i_PhaseFlash.x;
	mat4 model = u_Model * i_Model;
	vec3 p = a_Position;
	vec3 n = a_Normal;
	vec3 t = a_Tangent.xyz;

	// Rigged models take their pose first; the small shader motions below ride on top of it.
	float weightSum = a_Weights.x + a_Weights.y + a_Weights.z + a_Weights.w;
	if (i_Extra.x >= 0.0 && weightSum > 0.0)
	{
		int base = int(i_Extra.x + 0.5);
		mat4 skin = BoneMatrix(base + int(a_Joints.x + 0.5)) * a_Weights.x
				  + BoneMatrix(base + int(a_Joints.y + 0.5)) * a_Weights.y
				  + BoneMatrix(base + int(a_Joints.z + 0.5)) * a_Weights.z
				  + BoneMatrix(base + int(a_Joints.w + 0.5)) * a_Weights.w;
		skin /= weightSum;
		p = (skin * vec4(p, 1.0)).xyz;
		n = mat3(skin) * n;
		t = mat3(skin) * t;
	}
	// Posed, so a creature collapsing as it is released still comes apart from the ground up.
	vec3 local = p;

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
		t.xz = vec2(c * t.x + s * t.z, -s * t.x + c * t.z);
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
	v_Tangent = vec4(normalize(mat3(model) * t + vec3(1e-6)), a_Tangent.w < 0.0 ? -1.0 : 1.0);
	v_Color = a_Color.rgb;
	v_MossMask = a_Color.a;
	v_MossGrowth = i_Extra.y;
	v_Foliage = (anim & ANIM_FOLIAGE) != 0 ? 1.0 : 0.0;
	v_UV = a_UV;
	v_Tint = i_TintEmissive.rgb;
	v_Emissive = i_TintEmissive.a;
	v_Flash = i_PhaseFlash.y;
	v_Dissolve = i_PhaseFlash.z;
	v_Cutaway = i_PhaseFlash.w;
	v_Local = local;
	gl_Position = u_ViewProj * world;
}
