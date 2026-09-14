#version 330

// Mode 0: flat rect (HUD panels, fades).
// Mode 1: full-screen atmosphere pass (vignette + spore haze).
// Mode 2: progress bar (track, fill coloured by amount, leading-edge highlight).

#include "Env.glsl"

in vec2 v_UV;

uniform int   u_Mode;
uniform vec4  u_Color;
uniform vec3  u_Color2;
uniform float u_Fill;
uniform float u_Vignette;
uniform float u_Haze;
uniform vec3  u_HazeColor;
uniform float u_Time;

layout(location = 0) out vec4 FragColor;

void main()
{
	if (u_Mode == 0)
	{
		FragColor = u_Color;
		return;
	}

	if (u_Mode == 2)
	{
		float fill = clamp(u_Fill, 0.0, 1.0);
		float inside = 1.0 - step(fill, v_UV.x);
		float leading = smoothstep(fill - 0.04, fill, v_UV.x) * inside;
		float shade = 0.82 + 0.18 * v_UV.y;
		vec3 fillColor = mix(u_Color.rgb, u_Color2, fill) * shade + vec3(leading * 0.25);
		vec3 color = mix(vec3(0.05, 0.07, 0.07), fillColor, inside);
		float alpha = mix(0.55, 0.88, inside) * u_Color.a;
		FragColor = vec4(color, alpha);
		return;
	}

	vec2 uv = v_UV;
	float d = length((uv - vec2(0.5)) * vec2(1.15, 1.0));

	// Vignette closes in as spore exposure rises.
	float vig = smoothstep(0.34, 0.80, d) * u_Vignette;

	// Haze breathes, bleeds inward from the edges, and borrows the time-of-day fog tint.
	float breathe = 0.72 + 0.28 * sin(u_Time * 0.55);
	float edgeMask = smoothstep(0.10, 0.72, d);
	float haze = u_Haze * breathe * (0.30 + 0.70 * edgeMask);
	vec3 hazeColor = mix(u_HazeColor, EnvAt(u_TimeOfDay).fogColor, 0.35);

	vec3 color = mix(hazeColor, vec3(0.02, 0.03, 0.05), vig / max(vig + haze, 0.0001));
	float alpha = clamp(vig * 0.85 + haze, 0.0, 0.92);

	FragColor = vec4(color, alpha);
}
