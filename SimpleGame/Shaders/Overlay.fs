#version 330

// Mode 0: flat rect (HUD panels, bars, fades).
// Mode 1: full-screen atmosphere pass (vignette + spore haze).

in vec2 v_UV;

uniform int   u_Mode;
uniform vec4  u_Color;
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

	vec2 uv = v_UV;
	float d = length((uv - vec2(0.5)) * vec2(1.15, 1.0));

	// Vignette closes in as spore exposure rises.
	float vig = smoothstep(0.34, 0.80, d) * u_Vignette;

	// Haze breathes, and bleeds inward from the edges when exposure is high.
	float breathe = 0.72 + 0.28 * sin(u_Time * 0.55);
	float edge = smoothstep(0.10, 0.72, d);
	float haze = u_Haze * breathe * (0.30 + 0.70 * edge);

	vec3 color = mix(u_HazeColor, vec3(0.02, 0.03, 0.05), vig / max(vig + haze, 0.0001));
	float alpha = clamp(vig * 0.85 + haze, 0.0, 0.92);

	FragColor = vec4(color, alpha);
}
