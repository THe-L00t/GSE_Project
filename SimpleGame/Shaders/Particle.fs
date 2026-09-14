#version 330

#include "Env.glsl"

in float v_Alpha;
in float v_Rand;

uniform vec3  u_Color;
uniform float u_DensityScale;
uniform float u_Time;

layout(location = 0) out vec4 FragColor;

void main()
{
	vec2 d = gl_PointCoord - vec2(0.5);
	float r = length(d);

	// Soft core with a wide halo. Spores are the only high-saturation element.
	float core = smoothstep(0.50, 0.06, r);
	float halo = smoothstep(0.50, 0.20, r) * 0.35;

	// Each spore breathes at its own pace.
	float pulse = 0.75 + 0.25 * sin(u_Time * 1.6 + v_Rand * 6.283);
	float density = u_DensityScale * SporeDensityAt(u_TimeOfDay);

	float alpha = (core + halo) * v_Alpha * density * pulse;
	FragColor = vec4(u_Color, alpha);
}
