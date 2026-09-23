#version 330

// Mode 0: bright pass, downsampling the scene into the bloom buffer.
// Mode 1: separable gaussian blur along u_Direction.
// Mode 2: composite - spore edge blur and colour bleed, bloom, tone mapping, grain.

in vec2 v_UV;

uniform int       u_Mode;
uniform sampler2D u_Source;
uniform sampler2D u_Bloom;
uniform vec2      u_Texel;          // one texel of u_Source
uniform vec2      u_Direction;      // blur step in UV
uniform float     u_BloomStrength;
uniform float     u_SporeExposure;
uniform float     u_Time;

layout(location = 0) out vec4 FragColor;

const float kThreshold = 0.75;
const float kKnee = 0.25;

float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
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
	vec3 c = texture(u_Source, uv).rgb;

	// Heavy spore exposure blurs the edges of sight and lets the colours slip apart.
	vec2 fromCenter = uv - vec2(0.5);
	float edge = smoothstep(0.18, 0.72, length(fromCenter * vec2(1.15, 1.0)));
	float amount = edge * smoothstep(0.10, 0.90, u_SporeExposure);
	if (amount > 0.002)
	{
		float radius = amount * 7.0;
		float spin = hash12(gl_FragCoord.xy) * 6.2831853;
		vec3 acc = c;
		for (int i = 0; i < 8; ++i)
		{
			float a = spin + float(i) * 0.7853982;
			acc += texture(u_Source, uv + vec2(cos(a), sin(a)) * radius * u_Texel).rgb;
		}
		acc /= 9.0;

		vec2 shift = fromCenter * amount * 0.014;
		float red = texture(u_Source, uv + shift).r;
		float blue = texture(u_Source, uv - shift).b;

		c = mix(c, acc, amount);
		c.r = mix(c.r, red, amount * 0.7);
		c.b = mix(c.b, blue, amount * 0.7);
	}

	c += texture(u_Bloom, uv).rgb * u_BloomStrength;
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
