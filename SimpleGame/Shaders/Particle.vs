#version 330

// Spore field. Every particle is animated on the GPU from a static seed and
// wrapped inside a box that follows the camera, so the field never runs out.
// Style 0: spores. Style 1: fireflies low over the ground. Style 2: pollen and dust.

in vec3 a_Seed;   // [0,1) position inside the field box
in vec3 a_Rand;   // per-particle randomness

uniform mat4  u_ViewProj;
uniform vec3  u_Center;
uniform vec3  u_Field;
uniform float u_Time;
uniform float u_PixelsPerUnit;
uniform float u_Size;
uniform int   u_Style;

out float v_Alpha;
out float v_Rand;

void main()
{
	vec3 s = a_Seed;
	float fadeIn;
	float fadeOut;

	if (u_Style == 1)
	{
		// Fireflies wander in slow loops instead of rising.
		float t = u_Time * (0.25 + a_Rand.x * 0.2);
		s.x = fract(s.x + sin(t + a_Rand.y * 6.283) * 0.012 + cos(t * 0.7 + a_Rand.z * 6.283) * 0.008);
		s.z = fract(s.z + cos(t * 0.9 + a_Rand.y * 6.283) * 0.012);
		s.y = 0.15 + 0.7 * s.y + sin(u_Time * 0.8 + a_Rand.z * 6.283) * 0.08;
		fadeIn = 1.0;
		fadeOut = 1.0;
	}
	else if (u_Style == 2)
	{
		// Pollen drifts with the breeze and sinks slowly.
		s.x = fract(s.x + u_Time * (0.004 + a_Rand.x * 0.004));
		s.z = fract(s.z + u_Time * 0.002 + sin(u_Time * 0.3 + a_Rand.y * 6.283) * 0.01);
		s.y = fract(s.y - u_Time * (0.003 + a_Rand.z * 0.004));
		fadeIn = smoothstep(0.0, 0.15, s.y);
		fadeOut = 1.0 - smoothstep(0.7, 1.0, s.y);
	}
	else
	{
		// Slow rise, gentle lateral drift.
		s.y = fract(s.y + u_Time * (0.008 + a_Rand.x * 0.016));
		s.x = fract(s.x + sin(u_Time * 0.15 + a_Rand.y * 6.283) * 0.020 + u_Time * 0.003);
		s.z = fract(s.z + cos(u_Time * 0.13 + a_Rand.z * 6.283) * 0.020);

		// Fade in off the ground, fade out before the top of the field.
		fadeIn = smoothstep(0.00, 0.10, s.y);
		fadeOut = 1.0 - smoothstep(0.55, 1.00, s.y);
	}

	vec3 world;
	world.x = u_Center.x + (s.x - 0.5) * u_Field.x;
	world.z = u_Center.z + (s.z - 0.5) * u_Field.z;
	world.y = s.y * u_Field.y;

	gl_Position = u_ViewProj * vec4(world, 1.0);

	v_Alpha = fadeIn * fadeOut;
	v_Rand = a_Rand.x;

	gl_PointSize = max(2.0, u_Size * u_PixelsPerUnit * (0.55 + a_Rand.x * 0.9));
}
