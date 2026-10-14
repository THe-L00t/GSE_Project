#version 330

#include "Env.glsl"

in float v_Alpha;
in float v_Rand;
in float v_Spin;

uniform vec3  u_Color;
uniform float u_DensityScale;
uniform float u_Time;
uniform int   u_Style;

layout(location = 0) out vec4 FragColor;

void main()
{
	vec2 d = gl_PointCoord - vec2(0.5);
	float r = length(d);
	float day = clamp(sin((u_TimeOfDay - 0.25) * 6.2831853), 0.0, 1.0);

	// Weather is lit by the sky rather than glowing: dim at night, bright by day.
	float light = 0.30 + 0.70 * smoothstep(0.0, 0.5, day);

	if (u_Style == 3)
	{
		// A thin streak leaning with the wind.
		float slant = -u_Weather.z * 0.35;
		float line = 1.0 - smoothstep(0.015, 0.055, abs(d.x + d.y * slant));
		float len = 1.0 - smoothstep(0.30, 0.50, abs(d.y));
		float alpha = line * len * 0.55 * u_DensityScale * v_Alpha;
		FragColor = vec4(u_Color * light, alpha);
		return;
	}

	if (u_Style == 4)
	{
		float flake = 1.0 - smoothstep(0.15, 0.50, r);
		float alpha = flake * 0.85 * u_DensityScale * v_Alpha;
		FragColor = vec4(u_Color * light, alpha);
		return;
	}

	if (u_Style == 5)
	{
		// A tumbling oval leaf, somewhere between rust, red and gold.
		vec2 q = vec2(cos(v_Spin) * d.x - sin(v_Spin) * d.y, sin(v_Spin) * d.x + cos(v_Spin) * d.y);
		float leaf = 1.0 - smoothstep(0.85, 1.0, length(q / vec2(0.46, 0.22)));
		vec3 rust = vec3(0.72, 0.36, 0.14);
		vec3 red = vec3(0.70, 0.20, 0.12);
		vec3 gold = vec3(0.86, 0.66, 0.22);
		vec3 tint = v_Rand < 0.5 ? mix(red, rust, v_Rand * 2.0) : mix(rust, gold, v_Rand * 2.0 - 1.0);
		float alpha = leaf * u_DensityScale * v_Alpha;
		FragColor = vec4(tint * u_Color * light, alpha);
		return;
	}

	if (u_Style == 1)
	{
		// Fireflies blink in short bursts and only come out after dusk.
		float core = 1.0 - smoothstep(0.02, 0.22, r);
		float halo = (1.0 - smoothstep(0.10, 0.50, r)) * 0.45;
		float blink = pow(max(sin(u_Time * (0.9 + v_Rand * 0.8) + v_Rand * 40.0), 0.0), 4.0);
		float night = 1.0 - smoothstep(0.0, 0.35, day);
		float alpha = (core * 1.8 + halo) * blink * night * u_DensityScale * v_Alpha;
		FragColor = vec4(u_Color, alpha);
		return;
	}

	if (u_Style == 2)
	{
		float speck = 1.0 - smoothstep(0.15, 0.50, r);
		float shimmer = 0.6 + 0.4 * sin(u_Time * 2.3 + v_Rand * 25.0);
		float alpha = speck * shimmer * smoothstep(0.05, 0.40, day) * u_DensityScale * v_Alpha;
		FragColor = vec4(u_Color, alpha);
		return;
	}

	// Soft core with a wide halo. Spores are the only high-saturation element.
	float core = 1.0 - smoothstep(0.06, 0.50, r);
	float halo = (1.0 - smoothstep(0.20, 0.50, r)) * 0.35;

	// Each spore breathes at its own pace.
	float pulse = 0.75 + 0.25 * sin(u_Time * 1.6 + v_Rand * 6.283);
	float density = u_DensityScale * SporeDensityAt(u_TimeOfDay);

	float alpha = (core + halo) * v_Alpha * density * pulse;
	FragColor = vec4(u_Color, alpha);
}
