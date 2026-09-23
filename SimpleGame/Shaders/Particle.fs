#version 330

#include "Env.glsl"

in float v_Alpha;
in float v_Rand;

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
