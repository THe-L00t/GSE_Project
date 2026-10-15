#version 330

// Mode 0: bright pass, downsampling the scene into the bloom buffer.
// Mode 1: separable gaussian blur along u_Direction.
// Mode 2: composite - spore edge blur and colour bleed, bloom, tone mapping, grain.

in vec2 v_UV;

uniform int       u_Mode;
uniform sampler2D u_Source;
uniform sampler2D u_Bloom;
uniform sampler2D u_Volume;         // light scattered by the fog, quarter resolution
uniform float     u_VolumeOn;
uniform vec2      u_Texel;          // one texel of u_Source
uniform vec2      u_Direction;      // blur step in UV
uniform float     u_BloomStrength;
uniform float     u_SporeExposure;
uniform float     u_Time;
uniform float     u_Dream;          // a dream fragment playing
uniform float     u_Frost;          // cold
uniform float     u_Hunger;         // hunger or thirst, whichever is worse
uniform vec3      u_Grade;          // the region's colour cast
uniform float     u_Hurt;           // a fresh blow, or health running low
uniform vec2      u_Focus;          // the player on screen, kept sharp
uniform float     u_TiltShift;      // drawn in close, the rows above and below go soft
uniform float     u_Memo;           // grandmother's map note: paper and pencil
uniform float     u_Recall;         // a memory: sepia, the edges dimmed
uniform vec3      u_Ripple;         // xy centre on screen, z how far the level-up wave has run (0 none)
uniform vec4      u_Heat[2];        // lantern flames: xy on screen, z one world unit in screen heights, w strength

layout(location = 0) out vec4 FragColor;

const float kThreshold = 0.75;
const float kKnee = 0.25;

float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

// Smooth value noise from the hash, for the frost; this pass has no noise texture bound.
float valueNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = hash12(i);
	float b = hash12(i + vec2(1.0, 0.0));
	float c = hash12(i + vec2(0.0, 1.0));
	float d = hash12(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

vec3 BrightPass(vec2 uv)
{
	// Four bilinear taps average a 4x4 block, enough for a quarter-size target.
	vec3 c = texture(u_Source, uv + u_Texel * vec2(-1.0, -1.0)).rgb;
	c += texture(u_Source, uv + u_Texel * vec2(1.0, -1.0)).rgb;
	c += texture(u_Source, uv + u_Texel * vec2(-1.0, 1.0)).rgb;
	c += texture(u_Source, uv + u_Texel * vec2(1.0, 1.0)).rgb;
	c *= 0.25;

	float peak = max(c.r, max(c.g, c.b));
	float soft = clamp(peak - kThreshold + kKnee, 0.0, 2.0 * kKnee);
	soft = soft * soft / (4.0 * kKnee + 0.0001);
	float weight = max(soft, peak - kThreshold) / max(peak, 0.0001);
	return c * weight;
}

vec3 Blur(vec2 uv)
{
	vec3 c = texture(u_Source, uv).rgb * 0.2270270270;
	c += texture(u_Source, uv + u_Direction * 1.3846153846).rgb * 0.3162162162;
	c += texture(u_Source, uv - u_Direction * 1.3846153846).rgb * 0.3162162162;
	c += texture(u_Source, uv + u_Direction * 3.2307692308).rgb * 0.0702702703;
	c += texture(u_Source, uv - u_Direction * 3.2307692308).rgb * 0.0702702703;
	return c;
}

// Linear below the shoulder, so the graded palette is kept; highlights roll off instead of clipping.
vec3 ToneMap(vec3 c)
{
	const float shoulder = 0.72;
	vec3 over = max(c - shoulder, 0.0);
	vec3 rolled = shoulder + (1.0 - shoulder) * (1.0 - exp(-over / (1.0 - shoulder)));
	return mix(c, rolled, step(shoulder, c));
}

vec3 Composite(vec2 uv)
{
	// In a dream the picture wavers, like air over a summer road.
	if (u_Dream > 0.002)
		uv += vec2(sin(uv.y * 22.0 + u_Time * 1.7), cos(uv.x * 18.0 + u_Time * 1.3)) * 0.0025 * u_Dream;

	// Growing stronger bends the air: one ring of refraction runs out from the player and settles.
	if (u_Ripple.z > 0.0 && u_Ripple.z < 1.0)
	{
		vec2 aspect = vec2(u_Texel.y / u_Texel.x, 1.0);
		vec2 d = (uv - u_Ripple.xy) * aspect;
		float r = length(d);
		float band = 1.0 - smoothstep(0.0, 0.07, abs(r - u_Ripple.z * 1.1));
		uv -= d / max(r, 0.0001) / aspect * band * 0.014 * (1.0 - u_Ripple.z);
	}

	// Air shimmers in a column over each lantern flame, widest at the fire and thinning as it rises.
	for (int i = 0; i < 2; ++i)
	{
		if (u_Heat[i].w <= 0.0) continue;
		vec2 d = (uv - u_Heat[i].xy) * vec2(u_Texel.y / u_Texel.x, 1.0) / u_Heat[i].z;
		float width = 0.45 + max(d.y, 0.0) * 0.25;
		float column = (1.0 - smoothstep(0.3, 1.0, abs(d.x) / width)) * smoothstep(-0.3, 0.1, d.y) * (1.0 - smoothstep(0.8, 2.6, d.y));
		float wobble = sin(uv.y * 180.0 - u_Time * 7.0 + d.x * 3.0) * 0.6 + sin(uv.y * 97.0 - u_Time * 4.3) * 0.4;
		uv.x += wobble * column * u_Heat[i].w * 0.0016;
	}

	vec3 c = texture(u_Source, uv).rgb;

	// Heavy spore exposure blurs the edges of sight and lets the colours slip apart; a dream melts them the same way.
	// Drawn in close, the rows above and below the player soften like a lens focused on a miniature.
	vec2 fromCenter = uv - vec2(0.5);
	float edge = smoothstep(0.18, 0.72, length(fromCenter * vec2(1.15, 1.0)));
	float amount = edge * max(smoothstep(0.10, 0.90, u_SporeExposure), u_Dream * 0.9);
	float tilt = u_TiltShift * smoothstep(0.08, 0.42, abs(uv.y - u_Focus.y));
	if (amount > 0.002 || tilt > 0.002)
	{
		float radius = max(amount * 7.0, tilt * 6.0);
		float spin = hash12(gl_FragCoord.xy) * 6.2831853;
		vec3 acc = c;
		for (int i = 0; i < 8; ++i)
		{
			float a = spin + float(i) * 0.7853982;
			acc += texture(u_Source, uv + vec2(cos(a), sin(a)) * radius * u_Texel).rgb;
		}
		acc /= 9.0;
		c = mix(c, acc, max(amount, tilt));

		vec2 shift = fromCenter * amount * 0.014;
		float red = texture(u_Source, uv + shift).r;
		float blue = texture(u_Source, uv - shift).b;
		c.r = mix(c.r, red, amount * 0.7);
		c.b = mix(c.b, blue, amount * 0.7);
	}

	// Each region keeps its own light: the open road a touch cool, the ginkgo town warm.
	c *= u_Grade;

	float radial = length(fromCenter * vec2(1.15, 1.0));

	// A map note reads as the scene sketched on old paper: graphite along the edges, hatching in the shadows.
	if (u_Memo > 0.002)
	{
		vec2 px = gl_FragCoord.xy;
		vec2 o = u_Texel * 1.5;
		const vec3 kLuma = vec3(0.299, 0.587, 0.114);
		float gx = dot(texture(u_Source, uv + vec2(o.x, 0.0)).rgb - texture(u_Source, uv - vec2(o.x, 0.0)).rgb, kLuma);
		float gy = dot(texture(u_Source, uv + vec2(0.0, o.y)).rgb - texture(u_Source, uv - vec2(0.0, o.y)).rgb, kLuma);
		float luma = dot(c, kLuma);

		float fiber = valueNoise(px * vec2(0.9, 0.07)) * 0.5 + valueNoise(px * 0.3) * 0.5;
		vec3 paper = vec3(0.93, 0.89, 0.78) * (0.90 + 0.12 * fiber);
		float line = clamp(length(vec2(gx, gy)) * 5.0, 0.0, 1.0);
		float hatch = smoothstep(0.55, 0.85, fract((px.x + px.y) * 0.16 + valueNoise(px * 0.05) * 0.6));
		float shade = 1.0 - smoothstep(0.06, 0.40, luma);
		float graphite = max(line, hatch * shade * 0.8) * (0.75 + 0.25 * fiber);
		vec3 sketch = paper * (1.0 - graphite * 0.62) * mix(0.78, 1.0, smoothstep(0.0, 0.5, luma));
		c = mix(c, sketch, u_Memo * 0.78);
	}

	// A memory: an old photograph, browned and dim at the corners, without the dream's waver.
	if (u_Recall > 0.002)
	{
		float luma = dot(c, vec3(0.299, 0.587, 0.114));
		vec3 sepia = luma * vec3(1.12, 0.94, 0.72) + vec3(0.04, 0.025, 0.0);
		c = mix(c, sepia, u_Recall * 0.8);
		c *= 1.0 - smoothstep(0.30, 0.85, radial) * u_Recall * 0.65;
	}

	// Remembered colour: faded toward a warm sepia and lifted, an old photograph left in the sun.
	if (u_Dream > 0.002)
	{
		float luma = dot(c, vec3(0.299, 0.587, 0.114));
		vec3 warm = luma * vec3(1.15, 0.98, 0.78) + vec3(0.05, 0.035, 0.01);
		c = mix(c, warm, u_Dream * 0.65);
	}

	// Hunger and thirst drain the colour first, then close sight in from the edges.
	if (u_Hunger > 0.002)
	{
		float luma = dot(c, vec3(0.299, 0.587, 0.114));
		c = mix(c, vec3(luma), u_Hunger * 0.7);
		c *= 1.0 - smoothstep(0.62 - u_Hunger * 0.32, 0.80, radial) * u_Hunger * 0.75;
	}

	// Cold tints everything blue, and frost grows in from the edges, ragged where the crystals reach further.
	if (u_Frost > 0.002)
	{
		c *= mix(vec3(1.0), vec3(0.88, 0.95, 1.08), u_Frost * 0.6);
		vec2 q = uv * vec2(u_Texel.y / u_Texel.x, 1.0) * 9.0;
		float crystals = valueNoise(q) * 0.6 + valueNoise(q * 3.1) * 0.3 + valueNoise(q * 9.7) * 0.1;
		float reach = 0.72 - u_Frost * 0.30;
		float rime = smoothstep(reach, reach + 0.12, radial + (crystals - 0.5) * 0.18);
		vec3 ice = vec3(0.78, 0.87, 0.95) * (0.75 + 0.35 * crystals);
		c = mix(c, ice, rime * (0.35 + 0.4 * u_Frost));
	}

	// A blow leaves a dark red at the edges of sight.
	if (u_Hurt > 0.002)
		c = mix(c, vec3(0.50, 0.04, 0.03), smoothstep(0.35, 0.80, radial) * u_Hurt * 0.7);

	c += texture(u_Bloom, uv).rgb * u_BloomStrength * (1.0 - u_Memo * 0.7);
	c += texture(u_Volume, uv).rgb * u_VolumeOn;
	c = ToneMap(c);

	// A trace of grain breaks up banding in the fog gradients.
	c += (hash12(gl_FragCoord.xy + fract(u_Time) * 97.0) - 0.5) * (1.5 / 255.0);
	return c;
}

void main()
{
	if (u_Mode == 0)
		FragColor = vec4(BrightPass(v_UV), 1.0);
	else if (u_Mode == 1)
		FragColor = vec4(Blur(v_UV), 1.0);
	else
		FragColor = vec4(Composite(v_UV), 1.0);
}
