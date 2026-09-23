#version 330

// Mode 0: model. Mode 1: procedural ground. Mode 2: reservoir water. Mode 3: blob shadow.

#include "Env.glsl"

in vec3  v_WorldPos;
in vec3  v_Normal;
in vec3  v_Color;
in vec3  v_Local;
in float v_Glow;

uniform vec3  u_Tint;
uniform float u_Emissive;
uniform float u_Flash;
uniform int   u_Mode;
uniform vec3  u_FogOrigin;      // camera target; fog thickens away from it
uniform float u_Time;
uniform vec3  u_CamPos;

uniform float u_Stage;          // ground naturalisation, 0 bare .. 3 overgrown
uniform vec4  u_NeighborStage;  // -x, +x, -z, +z
uniform vec2  u_ChunkCenter;
uniform float u_ChunkSize;      // zero turns neighbour blending off
uniform vec3  u_Damp;           // x, z, strength

const int kMaxLights = 8;
uniform int   u_LightCount;
uniform vec4  u_LightPos[kMaxLights];     // xyz position, w radius
uniform vec3  u_LightColor[kMaxLights];

uniform sampler2DShadow u_ShadowMap;
uniform mat4  u_LightViewProj;
uniform float u_ShadowOn;

layout(location = 0) out vec4 FragColor;

float hash21(vec2 p)
{
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float valueNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = hash21(i);
	float b = hash21(i + vec2(1.0, 0.0));
	float c = hash21(i + vec2(0.0, 1.0));
	float d = hash21(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p)
{
	float v = 0.0;
	float amp = 0.5;
	for (int i = 0; i < 4; ++i)
	{
		v += amp * valueNoise(p);
		p *= 2.03;
		amp *= 0.5;
	}
	return v;
}

// The dirt road winds gently along +Z. Echoed by RoadCenter() in ChunkMap.h.
float roadCenter(float z)
{
	return sin(z * 0.05) * 3.0;
}

// Both sides of a chunk border meet at the average of the two stages, so there is no seam.
float BlendedStage(vec2 p)
{
	if (u_ChunkSize <= 0.0) return u_Stage;

	vec2 local = (p - u_ChunkCenter) / u_ChunkSize;
	float wx = smoothstep(0.2, 0.5, abs(local.x)) * 0.5;
	float wz = smoothstep(0.2, 0.5, abs(local.y)) * 0.5;
	float sx = local.x < 0.0 ? u_NeighborStage.x : u_NeighborStage.y;
	float sz = local.y < 0.0 ? u_NeighborStage.z : u_NeighborStage.w;

	float stage = mix(u_Stage, sx, wx);
	return mix(stage, sz, wz);
}

// Slow cloud shadows drifting over everything; they keep large flat areas alive.
float CloudShade(vec2 p)
{
	float c = fbm(p * 0.035 + vec2(u_Time * 0.012, u_Time * 0.007));
	return mix(0.62, 1.0, smoothstep(0.38, 0.62, c));
}

// Sun visibility from the shadow map: 3x3 hardware-filtered taps, faded out at the map's border.
float SunShadow(vec3 pos, vec3 n)
{
	if (u_ShadowOn < 0.5) return 1.0;

	vec4 lp = u_LightViewProj * vec4(pos + n * 0.05, 1.0);
	vec3 sc = lp.xyz / lp.w * 0.5 + 0.5;
	if (sc.x <= 0.0 || sc.x >= 1.0 || sc.y <= 0.0 || sc.y >= 1.0 || sc.z >= 1.0) return 1.0;

	vec2 texel = 1.0 / vec2(textureSize(u_ShadowMap, 0));
	float lit = 0.0;
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
			lit += texture(u_ShadowMap, vec3(sc.xy + vec2(float(x), float(y)) * texel * 1.5, sc.z - 0.0005));
	}
	lit /= 9.0;

	float border = min(min(sc.x, 1.0 - sc.x), min(sc.y, 1.0 - sc.y));
	return mix(1.0, lit, smoothstep(0.0, 0.06, border));
}

vec3 PointLights(vec3 pos, vec3 n)
{
	vec3 sum = vec3(0.0);
	for (int i = 0; i < kMaxLights; ++i)
	{
		if (i >= u_LightCount) break;
		vec3 toLight = u_LightPos[i].xyz - pos;
		float d = length(toLight);
		float range = u_LightPos[i].w;
		float falloff = clamp(1.0 - d / range, 0.0, 1.0);
		falloff *= falloff;
		float ndl = clamp(dot(n, toLight / max(d, 0.001)) * 0.6 + 0.4, 0.0, 1.0);
		sum += u_LightColor[i] * falloff * ndl;
	}
	return sum;
}

void main()
{
	Env env = EnvAt(u_TimeOfDay);

	// Distance runs from the camera target, not the eye: the orthographic eye
	// sits 55 units back, which would bury the whole screen in fog.
	float dist = length(v_WorldPos - u_FogOrigin);
	float heightFade = exp(-max(v_WorldPos.y, 0.0) * 0.13);
	float fog = clamp(1.0 - exp(-dist * env.fogDensity * heightFade), 0.0, 1.0);

	if (u_Mode == 3)
	{
		float r = length(v_Local.xz) * 2.0;
		// With real shadows the blob only needs to add a soft contact darkening.
		float strength = mix(0.38, 0.22, u_ShadowOn);
		float alpha = (1.0 - smoothstep(0.3, 1.0, r)) * strength * (1.0 - fog);
		FragColor = vec4(0.0, 0.0, 0.0, alpha);
		return;
	}

	vec3 n = normalize(v_Normal);
	vec3 base = v_Color * u_Tint;
	float emissive = u_Emissive + v_Glow;
	float gloss = 0.0;

	if (u_Mode == 1)
	{
		// Ground: concrete and dirt giving way to moss, further along as the stage rises.
		vec2 p = v_WorldPos.xz;
		float grain = fbm(p * 0.35);
		float patchMask = fbm(p * 0.09 + 17.0);
		float overgrowth = clamp(BlendedStage(p) / 3.0, 0.0, 1.0);

		vec3 concrete = vec3(0.34, 0.35, 0.33);
		vec3 dirt     = vec3(0.30, 0.26, 0.21);
		vec3 moss     = vec3(0.20, 0.31, 0.20);
		vec3 grass    = vec3(0.24, 0.34, 0.24);
		vec3 forest   = vec3(0.13, 0.22, 0.15);

		float road = (1.0 - smoothstep(1.9, 3.2, abs(p.x - roadCenter(p.y)))) * (1.0 - overgrowth * 0.6);
		vec3 soil = mix(grass, moss, smoothstep(0.35 - overgrowth * 0.3, 0.75 - overgrowth * 0.3, patchMask));
		vec3 track = mix(dirt, concrete, smoothstep(0.4, 0.7, grain) * (1.0 - overgrowth));

		base = mix(soil, track, road * 0.85);
		// Moss creeping over the road: the land is already turning.
		base = mix(base, moss, road * smoothstep(0.55 - overgrowth * 0.4, 0.85, patchMask) * 0.55);
		base = mix(base, forest, smoothstep(0.55, 1.0, overgrowth) * 0.5 * patchMask);
		base *= 0.85 + 0.30 * grain;

		// Fine detail: grass tufts off the road, pebbles and cracks on it.
		float tuft = valueNoise(p * vec2(4.1, 1.3)) * valueNoise(p * vec2(1.2, 3.7));
		base *= mix(1.0, 0.80 + 0.35 * tuft, 1.0 - road);
		float pebble = smoothstep(0.78, 0.86, valueNoise(p * 2.6 + 31.0));
		base = mix(base, base * 1.35 + vec3(0.03), pebble * road * 0.6);
		float crack = 1.0 - smoothstep(0.0, 0.035, abs(valueNoise(p * 0.9 + 7.0) - 0.5));
		base *= 1.0 - crack * road * (1.0 - overgrowth) * 0.35;

		// Damp low ground near water reads darker.
		float damp = (1.0 - smoothstep(6.0, 26.0, length(p - u_Damp.xy))) * u_Damp.z;
		base = mix(base, base * vec3(0.78, 0.86, 0.92), damp * 0.5);
		emissive = 0.0;
	}
	else if (u_Mode == 2)
	{
		// Reservoir. Ripple normals only, no reflection pass.
		vec2 p = v_WorldPos.xz;
		float w1 = sin(p.x * 1.30 + u_Time * 0.75);
		float w2 = sin(p.y * 1.70 - u_Time * 0.55);
		float w3 = sin((p.x + p.y) * 0.65 + u_Time * 0.35);
		n = normalize(vec3(w1 * 0.05 + w3 * 0.03, 1.0, w2 * 0.05 + w3 * 0.03));

		float fres = pow(1.0 - clamp(dot(n, normalize(u_CamPos - v_WorldPos)), 0.0, 1.0), 3.0);

		// Deeper toward the middle, with a pale lapping band along the shore.
		float edge = max(abs(v_Local.x), abs(v_Local.z)) * 2.0;
		vec3 deep = mix(vec3(0.05, 0.11, 0.13), vec3(0.10, 0.18, 0.18), smoothstep(0.4, 1.0, edge));
		base = mix(deep, env.skyColor * 0.9, 0.35 + fres * 0.55);
		float lap = 0.5 + 0.5 * sin(edge * 60.0 - u_Time * 1.6);
		float foam = smoothstep(0.90, 1.0, edge) * (0.55 + 0.45 * lap);
		base = mix(base, vec3(0.55, 0.62, 0.60), foam * 0.45);
		gloss = 1.0;
		emissive = 0.0;
	}

	// Wrapped diffuse keeps the shadow side readable and soft.
	float ndl = dot(n, env.sunDir);
	float wrap = clamp(ndl * 0.5 + 0.5, 0.0, 1.0);
	wrap *= wrap;

	float sunVis = CloudShade(v_WorldPos.xz) * SunShadow(v_WorldPos, n);

	// Models darken toward the ground they stand on, which seats them in the scene.
	float ao = 1.0;
	if (u_Mode == 0) ao = mix(0.55, 1.0, smoothstep(0.0, 0.9, v_WorldPos.y));

	// Lanterns and spore lights matter more once the sun is down.
	float night = 1.0 - smoothstep(0.10, 0.60, env.sunDir.y);
	vec3 lamp = PointLights(v_WorldPos, n) * (0.45 + 0.55 * night);

	vec3 viewDir = normalize(u_CamPos - v_WorldPos);
	vec3 ambient = mix(env.groundColor, env.skyColor, n.y * 0.5 + 0.5);
	vec3 color = base * (ambient * ao + env.sunColor * wrap * sunVis + lamp);

	if (u_Mode == 0)
	{
		// Rim light outlines silhouettes against the ground, tinted by the sky.
		float rim = pow(1.0 - clamp(dot(n, viewDir), 0.0, 1.0), 3.0);
		color += (env.skyColor * 0.6 + env.sunColor * 0.25) * rim * 0.35 * ao;
	}

	if (gloss > 0.0)
	{
		vec3 halfDir = normalize(viewDir + env.sunDir);
		float spec = pow(max(dot(n, halfDir), 0.0), 90.0);
		color += env.sunColor * spec * 0.9 * sunVis;
	}

	color += base * emissive;
	color = mix(color, vec3(1.0, 0.95, 0.85), clamp(u_Flash, 0.0, 1.0));

	// Muted palette: only spores and light are allowed to be saturated.
	float luma = dot(color, vec3(0.299, 0.587, 0.114));
	color = mix(vec3(luma), color, env.saturation);

	color = mix(color, env.fogColor, fog);

	FragColor = vec4(color, 1.0);
}
