#version 330

// Mode 0: model. Mode 1: procedural ground. Mode 2: reservoir water. Mode 3: blob shadow.

#include "Env.glsl"

in vec3  v_WorldPos;
in vec3  v_Normal;
in vec3  v_Color;
in vec3  v_Local;
in float v_Glow;
in vec3  v_Tint;
in float v_Emissive;
in float v_Flash;
in float v_Dissolve;
in float v_Cutaway;
in vec2  v_UV;
in vec4  v_Tangent;
in float v_MossMask;
in float v_MossGrowth;
in float v_Foliage;

// Blender-made models: x = 1 when textured, y = specular strength, z = glow strength.
uniform vec4      u_Material;
uniform sampler2D u_Albedo;
uniform sampler2D u_NormalMap;
uniform sampler2D u_Mask;           // r occlusion, g roughness, b glow

// Ground layers made in Blender: 0 grass, 1 moss, 2 dirt, 3 concrete, 4 forest floor.
uniform float          u_GroundTex;
uniform sampler2DArray u_GroundAlbedo;
uniform sampler2DArray u_GroundNormal;
uniform sampler2DArray u_GroundHeight;

uniform int   u_Mode;
uniform vec3  u_FogOrigin;      // camera target; fog thickens away from it
uniform float u_Time;
uniform vec3  u_CamPos;

uniform float u_Stage;          // ground naturalisation, 0 bare .. 3 overgrown
uniform vec4  u_NeighborStage;  // -x, +x, -z, +z
uniform vec2  u_ChunkCenter;
uniform float u_ChunkSize;      // zero turns neighbour blending off
uniform vec3  u_Damp;           // x, z, strength
uniform float u_Road;           // 1 near the village; the road wears away to 0 far out
uniform float u_City;           // 1 on Route 32, where the old town may still stand

const int kMaxLights = 8;
uniform int   u_LightCount;
uniform vec4  u_LightPos[kMaxLights];     // xyz position, w radius
uniform vec3  u_LightColor[kMaxLights];

// Fireflies near the middle of the view, followed on the CPU: xyz position, w brightness.
const int kMaxGlows = 12;
uniform int   u_GlowCount;
uniform vec4  u_GlowPos[kMaxGlows];

uniform vec4  u_Cutaway;        // player's feet xyz, w how open the view through occluders is

uniform vec4  u_TorchPos;       // xyz, w = 1 while the torch is lit
uniform vec3  u_TorchDir;

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

uniform sampler2D u_Noise;      // one random value per lattice cell, 256x256, repeating

// One filtered fetch instead of four hashes: sampling between the two texel centres with a
// smoothstepped offset gives the same smooth value noise, and the ground calls it ~30 times a pixel.
float valueNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return texture(u_Noise, (i + f + 0.5) / 256.0).r;
}

// Rings spreading from raindrops: one drop per cell at its own moment, kept inside its cell so a single
// lookup is enough. xy tilts the normal outward from the drop, z is how bright the ring is.
vec3 Raindrops(vec2 p, float rate)
{
	vec2 cell = floor(p);
	float h = hash21(cell);
	float h2 = hash21(cell + 17.31);
	float cycle = u_Time * rate + h * 7.0;
	float phase = fract(cycle);
	float live = step(hash21(cell + floor(cycle) * 3.7), 0.6);   // not every cell gets a drop each round

	vec2 d = fract(p) - (0.3 + 0.4 * vec2(h, h2));
	float r = length(d);
	float ring = (1.0 - smoothstep(0.0, 0.04, abs(r - phase * 0.3))) * (1.0 - phase) * live;
	return vec3(d / max(r, 0.001) * ring, ring);
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

// How much of the old town still stands: whole around the start of Route 32, gone four chunks out.
// Echoed by UrbanAt() in ChunkMap.h, as are the street widths below.
float urbanAt(vec2 p)
{
	return 1.0 - smoothstep(30.0, 90.0, max(abs(p.x), abs(p.y)));
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

const float kCutRadius = 1.1;   // the player and a little of the ground around them

float CapsuleSdf(vec3 p, vec3 a, vec3 b, float radius)
{
	vec3 pa = p - a;
	vec3 ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h) - radius;
}

// Sphere-traces a capsule around the player from this fragment, away from the eye. Reaching it means the
// fragment stands between the eye and the player; the closest pass of the ray gives a soft edge.
// The view is orthographic, so every ray runs the same way and the hole is a cylinder.
float CutawayMask(vec3 pos)
{
	vec3 toEye = normalize(u_CamPos - u_FogOrigin);
	vec3 a = u_Cutaway.xyz + vec3(0.0, 0.3, 0.0);
	vec3 b = u_Cutaway.xyz + vec3(0.0, 1.7, 0.0);

	// Only what is nearer the eye than the player can hide them.
	if (dot(pos - (a + b) * 0.5, toEye) < 0.2) return 0.0;

	// Before marching, the ray's true closest pass in closed form (the distance to the capsule's axis is
	// convex along the ray). The march can only find a pass as close or farther, so where even the true one
	// misses the soft edge, the mask is zero and the march is skipped: it runs only in and around the hole.
	vec3 dir = -toEye;
	vec3 ab = b - a;
	vec3 w = pos - a;
	float abab = dot(ab, ab);
	float abDir = dot(ab, dir);
	float u = clamp((dot(w, ab) - abDir * dot(w, dir)) / max(abab - abDir * abDir, 1e-5), 0.0, 1.0);
	float s = max(dot(ab * u - w, dir), 0.0);
	u = clamp(dot(w + dir * s, ab) / abab, 0.0, 1.0);
	if (length(w + dir * s - ab * u) - kCutRadius > 0.36) return 0.0;

	float t = 0.0;
	float closest = 1e3;
	for (int i = 0; i < 32; ++i)
	{
		float d = CapsuleSdf(pos - toEye * t, a, b, kCutRadius);
		closest = min(closest, d);
		if (d < 0.01) break;
		t += max(d * 0.8, 0.05);
		if (t > 40.0) break;
	}
	return 1.0 - smoothstep(0.0, 0.35, closest);
}

// Each firefly leaves a small yellow-green pool of light on whatever is just below it.
vec3 FireflyGlow(vec3 pos)
{
	float sum = 0.0;
	for (int i = 0; i < kMaxGlows; ++i)
	{
		if (i >= u_GlowCount) break;
		float d = length(u_GlowPos[i].xyz - pos);
		float falloff = clamp(1.0 - d / 2.4, 0.0, 1.0);
		sum += falloff * falloff * u_GlowPos[i].w;
	}
	return vec3(0.85, 1.00, 0.45) * sum * 0.8;
}

// A warm cone from the hand torch, soft at its rim. The light it throws back off the ground is a
// separate point light added by the game.
vec3 TorchLight(vec3 pos, vec3 n)
{
	if (u_TorchPos.w < 0.5) return vec3(0.0);
	vec3 toLight = u_TorchPos.xyz - pos;
	float d = length(toLight);
	vec3 l = toLight / max(d, 0.001);
	float cone = smoothstep(0.84, 0.95, dot(-l, u_TorchDir));
	float falloff = clamp(1.0 - d / 14.0, 0.0, 1.0);
	float ndl = clamp(dot(n, l) * 0.7 + 0.3, 0.0, 1.0);
	return vec3(1.00, 0.88, 0.68) * 2.6 * cone * falloff * falloff * ndl;
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
		// Footprints use the same disc, with their own colour and strength and a firmer edge.
		// A decal with a ring width draws only a band at its rim, for the light that spreads from a level-up.
		bool print = v_Emissive > 0.0;
		float strength = print ? v_Emissive : mix(0.38, 0.22, u_ShadowOn);
		float alpha = (1.0 - smoothstep(print ? 0.55 : 0.3, 1.0, r)) * strength * (1.0 - fog);
		if (v_Flash > 0.0)
			alpha = (1.0 - smoothstep(0.0, v_Flash, abs(r - 1.0 + v_Flash))) * strength * (1.0 - fog);
		FragColor = vec4(print ? v_Tint : vec3(0.0), alpha);
		return;
	}

	// A released creature comes apart into spore light from the feet up; the edge of the cut glows.
	float release = 0.0;
	if (u_Mode == 0 && v_Dissolve > 0.0)
	{
		float cut = valueNoise(v_Local.xz * 6.0 + vec2(v_Local.y * 4.3, -v_Local.y * 3.1)) * 0.7
				  + clamp(v_Local.y / 1.6, 0.0, 1.0) * 0.3;
		float gone = v_Dissolve * 1.02;
		if (cut < gone) discard;
		release = 1.0 - smoothstep(0.0, 0.08, cut - gone);
	}

	// A tree or roof in front of the player opens up; dithered, so the hole thins out at its edge instead of
	// ending on a hard line, and what is left of the edge picks up a faint spore light.
	float seeRim = 0.0;
	if (u_Mode == 0 && v_Cutaway > 0.5 && u_Cutaway.w > 0.001)
	{
		float opening = CutawayMask(v_WorldPos) * u_Cutaway.w;
		if (opening > hash21(gl_FragCoord.xy)) discard;
		seeRim = opening;
	}

	vec3 n = normalize(v_Normal);
	vec3 base = v_Color * v_Tint;
	float emissive = v_Emissive + v_Glow;
	float gloss = 0.0;
	vec3 reflected = vec3(0.0);     // what still water mirrors, added after the lighting
	bool textured = u_Mode == 0 && u_Material.x > 0.5;
	float texAo = 1.0;
	float rough = 1.0;

	if (textured)
	{
		vec3 albedo = texture(u_Albedo, v_UV).rgb;
		vec3 maskTex = texture(u_Mask, v_UV).rgb;
		vec3 nt = texture(u_NormalMap, v_UV).rgb * 2.0 - 1.0;
		vec3 t = normalize(v_Tangent.xyz - n * dot(n, v_Tangent.xyz));
		vec3 b = cross(n, t) * v_Tangent.w;
		n = normalize(t * nt.x + b * nt.y + n * max(nt.z, 0.05));

		base = albedo * v_Color * v_Tint;
		texAo = maskTex.r;
		rough = maskTex.g;
		// Glow is painted into the mask and breathes with a pulse; the instance's own glow (a windup's tell,
		// a relic's glint) lifts the whole model only a little, so the painted lights stay the brightest.
		emissive = maskTex.b * u_Material.z * (0.75 + 2.5 * v_Glow) + v_Glow * 0.6 * (1.0 - maskTex.b) + v_Emissive * 0.35;

		// The world taking back what was made: moss spreads first where the model was painted to take it.
		if (v_MossGrowth > 0.001)
		{
			float speck = valueNoise(v_WorldPos.xz * 2.3 + v_WorldPos.y * 1.7);
			float cover = smoothstep(1.0 - v_MossGrowth - 0.06, 1.0 - v_MossGrowth + 0.06, v_MossMask * 0.75 + speck * 0.3);
			cover *= smoothstep(-0.35, 0.3, n.y);
			vec3 mossColor = mix(vec3(0.17, 0.29, 0.19), vec3(0.31, 0.45, 0.27), valueNoise(v_WorldPos.xz * 7.0 + v_WorldPos.y * 3.0));
			base = mix(base, mossColor * v_Tint, cover * 0.92);
			rough = mix(rough, 1.0, cover);
			emissive += cover * v_Glow * 0.3;
		}
	}

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

		float road = (1.0 - smoothstep(1.9, 3.2, abs(p.x - roadCenter(p.y)))) * (1.0 - overgrowth * 0.6) * u_Road;
		float toMoss = smoothstep(0.35 - overgrowth * 0.3, 0.75 - overgrowth * 0.3, patchMask);
		float toConcrete = smoothstep(0.4, 0.7, grain) * (1.0 - overgrowth);
		float onRoad = road * 0.85;
		// Moss creeping over the road: the land is already turning.
		float mossOver = road * smoothstep(0.55 - overgrowth * 0.4, 0.85, patchMask) * 0.55;
		float toForest = smoothstep(0.55, 1.0, overgrowth) * 0.5 * patchMask;

		// The old town: asphalt streets, pavements and paved lots, with nature breaking through as it thins out.
		// Cross streets run along the chunk borders; every other one is wide, with pavements and a crossing.
		float urban = u_City * urbanAt(p);
		float pave = 0.0;
		float asphalt = 0.0;
		float kerb = 0.0;
		float slabs = 0.0;
		float yellowPaint = 0.0;
		float whitePaint = 0.0;
		float stain = 0.0;
		if (urban > 0.001)
		{
			float dx = abs(p.x - roadCenter(p.y));
			float k = floor((p.y - 12.0) / 24.0 + 0.5);
			float dz = abs(p.y - (k * 24.0 + 12.0));
			bool wide = mod(k, 2.0) < 0.5;
			float sideHalf = wide ? 2.6 : 1.6;

			float breakAt = 0.42 + 0.30 * urban - 0.15 * overgrowth;
			float breach = smoothstep(breakAt, breakAt + 0.10, fbm(p * 0.21 + 3.0));
			pave = smoothstep(0.05, 0.35, urban) * (1.0 - breach);
			stain = smoothstep(0.62 - overgrowth * 0.3, 0.9, patchMask) * 0.5;

			float mainAsphalt = 1.0 - smoothstep(3.5, 3.7, dx);
			float sideAsphalt = 1.0 - smoothstep(sideHalf - 0.1, sideHalf + 0.1, dz);
			asphalt = max(mainAsphalt, sideAsphalt);
			float mainWalk = 1.0 - smoothstep(5.9, 6.1, dx);
			float sideWalk = wide ? 1.0 - smoothstep(3.7, 3.9, dz) : 0.0;
			float walk = max(mainWalk, sideWalk) * (1.0 - asphalt);
			kerb = walk * max(1.0 - smoothstep(0.05, 0.2, dx - 3.6), wide ? 1.0 - smoothstep(0.05, 0.2, dz - sideHalf) : 0.0);
			vec2 cell = abs(fract(p * 2.0) - 0.5);
			slabs = walk * smoothstep(0.42, 0.48, max(cell.x, cell.y));

			// Worn paint: the centre line and edge lines of the road, a zebra crossing at each wide street,
			// and a dashed line down the wide streets.
			float wear = smoothstep(0.25, 0.65, valueNoise(p * 1.3 + 9.0)) * (1.0 - stain);
			float open = 1.0 - sideAsphalt;
			yellowPaint = (1.0 - smoothstep(0.04, 0.07, abs(dx - 0.12))) * open * mainAsphalt;
			whitePaint = (1.0 - smoothstep(0.04, 0.07, abs(dx - 3.25))) * open;
			if (wide)
			{
				float band = step(sideHalf + 0.8, dz) * step(dz, sideHalf + 3.8) * step(dx, 3.3);
				whitePaint = max(whitePaint, band * step(0.5, fract((p.x - roadCenter(p.y)) / 0.9)));
				whitePaint = max(whitePaint, (1.0 - smoothstep(0.04, 0.07, dz)) * step(0.5, fract(p.x / 3.0)) * step(4.0, dx));
			}
			yellowPaint *= wear;
			whitePaint *= wear;
		}

		if (u_GroundTex > 0.5)
		{
			// The same mix as the painted ground below, as weights over the Blender layers, sharpened by
			// each layer's height so moss fills the gaps between stones instead of fading over them.
			float w[5];
			float keep = (1.0 - mossOver) * (1.0 - toForest);
			w[0] = (1.0 - toMoss) * (1.0 - onRoad) * keep;
			w[1] = toMoss * (1.0 - onRoad) * keep + mossOver * (1.0 - toForest);
			w[2] = onRoad * (1.0 - toConcrete) * keep;
			w[3] = onRoad * toConcrete * keep;
			w[4] = toForest;

			// The town paves over all of it, its paving stained with moss.
			for (int i = 0; i < 5; ++i)
				w[i] *= 1.0 - pave;
			w[3] += pave * (1.0 - stain);
			w[1] += pave * stain;

			vec2 uv = vec2(p.x, -p.y) * 0.22;
			float top = 0.0;
			float h[5];
			for (int i = 0; i < 5; ++i)
			{
				h[i] = texture(u_GroundHeight, vec3(uv, float(i))).r;
				top = max(top, w[i] + h[i] * 0.5);
			}
			float total = 0.0;
			for (int i = 0; i < 5; ++i)
			{
				w[i] = max(w[i] + h[i] * 0.5 - (top - 0.22), 0.0);
				total += w[i];
			}

			vec3 albedoSum = vec3(0.0);
			vec3 normalSum = vec3(0.0);
			// Every layer is read even at zero weight: a branch per pixel would break the mipmap lookup.
			for (int i = 0; i < 5; ++i)
			{
				float wi = w[i] / max(total, 1e-4);
				albedoSum += texture(u_GroundAlbedo, vec3(uv, float(i))).rgb * wi;
				normalSum += (texture(u_GroundNormal, vec3(uv, float(i))).rgb * 2.0 - 1.0) * wi;
			}
			base = albedoSum;
			// Images load top row first, so the baked +v runs toward +z here while u runs along +x.
			vec3 nt = normalize(normalSum + vec3(0.0, 0.0, 1e-4));
			n = normalize(vec3(nt.x, nt.z, nt.y));
			base *= 0.9 + 0.2 * grain;
		}
		else
		{
			vec3 soil = mix(grass, moss, toMoss);
			vec3 track = mix(dirt, concrete, toConcrete);
			base = mix(soil, track, onRoad);
			base = mix(base, moss, mossOver);
			base = mix(base, forest, toForest);
			base = mix(base, mix(concrete, moss, stain), pave);
			base *= 0.85 + 0.30 * grain;
		}

		// Asphalt is the paving darkened; pavements get their slabs and kerbs; the markings are worn paint.
		base = mix(base, base * vec3(0.42, 0.44, 0.48), asphalt * pave * (1.0 - stain));
		base *= 1.0 - slabs * pave * 0.22;
		base = mix(base, base * 1.30 + vec3(0.03), kerb * pave);
		base = mix(base, vec3(0.66, 0.55, 0.24), yellowPaint * pave);
		base = mix(base, vec3(0.70, 0.70, 0.66), whitePaint * pave);
		road = max(road, pave);

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

		// Seasons: lush summer grass, autumn yellowing under drifts of fallen leaves, a dull winter.
		float field = 1.0 - road;
		base = mix(base, base * vec3(0.92, 1.12, 0.90), u_Season.x * 0.5 * field);
		base = mix(base, base * vec3(1.18, 0.96, 0.68), u_Season.y * 0.55 * field);
		// The season and weather terms are skipped outright while they are off: the branches are uniform.
		if (u_Season.y > 0.005)
		{
			float litter = smoothstep(0.45, 0.70, fbm(p * 0.6 + 5.0)) * u_Season.y;
			vec3 leafColor = mix(vec3(0.44, 0.22, 0.09), vec3(0.52, 0.40, 0.14), valueNoise(p * 3.1));
			base = mix(base, leafColor * (0.8 + 0.4 * grain), litter * (0.65 - 0.35 * road));
		}
		float baseLuma = dot(base, vec3(0.299, 0.587, 0.114));
		base = mix(base, vec3(baseLuma) * vec3(0.95, 0.97, 1.0), u_Season.z * 0.35);

		// Rain soaks in: darker ground, and puddles that catch the light.
		if (u_Weather.w > 0.005)
		{
			float puddle = smoothstep(0.62, 0.78, valueNoise(p * 0.45 + 3.0)) * u_Weather.w;
			base *= 1.0 - 0.25 * u_Weather.w - 0.20 * puddle;
			gloss = puddle * 0.8;

			// While it is still raining the puddles are stippled with spreading rings that catch the sun.
			if (u_Weather.x > 0.02 && puddle > 0.02)
			{
				vec3 drop = Raindrops(p * 2.2, 1.1) + Raindrops(p * 2.2 + 0.5, 1.37);
				float wet = puddle * u_Weather.x;
				n = normalize(n + vec3(drop.x, 0.0, drop.y) * 0.5 * wet);
				base += vec3(0.05) * drop.z * wet;
			}
		}

		// Snow lies in drifts first, then covers everything; the road keeps its wheel ruts longest.
		if (u_Season.w > 0.001)
		{
			float drift = fbm(p * 0.22 + 41.0) * 0.6 + grain * 0.4;
			float cover = u_Season.w * (1.0 - 0.25 * road);
			float snow = smoothstep(1.0 - cover - 0.08, 1.0 - cover + 0.08, drift) * smoothstep(0.0, 0.05, cover);
			base = mix(base, vec3(0.84, 0.88, 0.93) * (0.92 + 0.12 * grain), snow);
			gloss *= 1.0 - snow;
		}
		emissive = 0.0;
	}
	else if (u_Mode == 2)
	{
		// Reservoir. Ripple normals, and a reflection of the sky, the sun and the lamps without a second pass.
		vec2 p = v_WorldPos.xz;
		float w1 = sin(p.x * 1.30 + u_Time * 0.75);
		float w2 = sin(p.y * 1.70 - u_Time * 0.55);
		float w3 = sin((p.x + p.y) * 0.65 + u_Time * 0.35);
		n = normalize(vec3(w1 * 0.05 + w3 * 0.03, 1.0, w2 * 0.05 + w3 * 0.03));

		// Rain stipples the surface with rings; even on a dry day a drip lands now and then.
		vec3 drop = Raindrops(p * 1.6, 0.9) * mix(0.12, 1.0, u_Weather.x);
		if (u_Weather.x > 0.02) drop += Raindrops(p * 2.3 + 0.5, 1.3) * u_Weather.x;
		n = normalize(n + vec3(drop.x, 0.0, drop.y) * 0.35);

		vec3 eye = normalize(u_CamPos - v_WorldPos);
		float fres = pow(1.0 - clamp(dot(n, eye), 0.0, 1.0), 3.0);

		// Deeper toward the middle, with a pale lapping band along the shore.
		float edge = max(abs(v_Local.x), abs(v_Local.z)) * 2.0;
		vec3 deep = mix(vec3(0.05, 0.11, 0.13), vec3(0.10, 0.18, 0.18), smoothstep(0.4, 1.0, edge));
		base = mix(deep, env.skyColor * 0.9, 0.20 + fres * 0.30);

		// The sky seen in the water: hazy at the horizon, clearer overhead, stronger at a grazing angle.
		vec3 r = reflect(-eye, n);
		vec3 sky = mix(env.fogColor, env.skyColor * 1.5, smoothstep(0.0, 0.7, r.y));
		reflected = sky * (0.25 + 0.55 * fres);
		reflected += env.sunColor * (pow(max(dot(r, env.sunDir), 0.0), 220.0) * 2.5 + pow(max(dot(r, env.sunDir), 0.0), 12.0) * 0.15);

		// Lanterns and the deer's light stretch into broken streaks across the ripples.
		for (int i = 0; i < kMaxLights; ++i)
		{
			if (i >= u_LightCount) break;
			vec3 toLight = u_LightPos[i].xyz - v_WorldPos;
			float d = length(toLight);
			float reach = clamp(1.0 - d / (u_LightPos[i].w * 2.5), 0.0, 1.0);
			reflected += u_LightColor[i] * pow(max(dot(r, toLight / max(d, 0.001)), 0.0), 40.0) * reach * 0.8;
		}

		float lap = 0.5 + 0.5 * sin(edge * 60.0 - u_Time * 1.6);
		float foam = smoothstep(0.90, 1.0, edge) * (0.55 + 0.45 * lap);
		base = mix(base, vec3(0.55, 0.62, 0.60), foam * 0.45);
		reflected *= 1.0 - foam * 0.6;
		gloss = 0.4;   // the mirrored sun above carries most of the highlight now
		emissive = 0.0;
	}

	if (u_Mode == 0 && emissive < 0.3)
	{
		// Leaves turn in autumn and dull in winter; anything glowing keeps its colour. Blender models say
		// which parts are leaves; shape recipes are guessed from how green they are.
		float greenness = base.g - max(base.r, base.b);
		float foliage = textured ? v_Foliage : smoothstep(0.015, 0.07, greenness);
		float luma = dot(base, vec3(0.299, 0.587, 0.114));
		float tone = hash21(floor(v_WorldPos.xz * 0.35));
		vec3 autumnLeaf = mix(vec3(0.66, 0.26, 0.09), vec3(0.74, 0.56, 0.14), tone) * luma * 2.3;
		base = mix(base, autumnLeaf, foliage * u_Season.y * 0.85);
		base = mix(base, vec3(luma) * vec3(0.90, 0.96, 0.92), foliage * u_Season.z * 0.45);

		// Rain darkens, and snow settles on whatever faces the sky.
		base *= 1.0 - 0.15 * u_Weather.w;
		float settle = smoothstep(0.55, 0.85, n.y) * u_Season.w;
		base = mix(base, vec3(0.86, 0.89, 0.94), settle * 0.9);
	}

	// Wrapped diffuse keeps the shadow side readable and soft.
	float ndl = dot(n, env.sunDir);
	float wrap = clamp(ndl * 0.5 + 0.5, 0.0, 1.0);
	wrap *= wrap;

	float sunVis = CloudShade(v_WorldPos.xz) * SunShadow(v_WorldPos, n);

	// Models darken toward the ground they stand on, which seats them in the scene.
	float ao = 1.0;
	if (u_Mode == 0) ao = mix(0.55, 1.0, smoothstep(0.0, 0.9, v_WorldPos.y));
	// Baked occlusion: deep in crevices for the sky's light, half as much for direct light.
	float directAo = mix(1.0, texAo, 0.5);
	ao *= texAo;

	// Lanterns and spore lights matter more once the sun is down.
	float night = 1.0 - smoothstep(0.10, 0.60, env.sunDir.y);
	vec3 lamp = (PointLights(v_WorldPos, n) + TorchLight(v_WorldPos, n)) * (0.45 + 0.55 * night) + FireflyGlow(v_WorldPos);

	vec3 viewDir = normalize(u_CamPos - v_WorldPos);
	vec3 ambient = mix(env.groundColor, env.skyColor, n.y * 0.5 + 0.5);
	vec3 color = base * (ambient * ao + (env.sunColor * wrap * sunVis + lamp) * directAo);

	if (u_Mode == 0)
	{
		// Rim light outlines silhouettes against the ground, tinted by the sky.
		float rim = pow(1.0 - clamp(dot(n, viewDir), 0.0, 1.0), 3.0);
		color += (env.skyColor * 0.6 + env.sunColor * 0.25) * rim * 0.35 * ao;
	}

	// Painted models are mostly matte; glazed jars, glass and wet metal catch a little of the sun.
	if (textured)
	{
		float shine = 1.0 - rough;
		vec3 halfDir = normalize(viewDir + env.sunDir);
		float spec = pow(max(dot(n, halfDir), 0.0), mix(10.0, 80.0, shine)) * shine * shine * u_Material.y * 2.0;
		color += env.sunColor * spec * sunVis * directAo;
	}

	if (gloss > 0.0)
	{
		vec3 halfDir = normalize(viewDir + env.sunDir);
		float spec = pow(max(dot(n, halfDir), 0.0), 90.0);
		color += env.sunColor * spec * 0.9 * sunVis;
	}

	color += base * emissive + reflected;
	color = mix(color, vec3(1.0, 0.95, 0.85), clamp(v_Flash, 0.0, 1.0));

	// Muted palette: only spores and light are allowed to be saturated.
	float luma = dot(color, vec3(0.299, 0.587, 0.114));
	color = mix(vec3(luma), color, env.saturation);
	color += vec3(0.55, 0.95, 0.80) * (release * 2.2 + seeRim * 0.25);

	color = mix(color, env.fogColor, fog);

	FragColor = vec4(color, 1.0);
}
