#version 330

// Spore field. Every particle is animated on the GPU from a static seed and
// wrapped inside a box that follows the camera, so the field never runs out.

in vec3 a_Seed;   // [0,1) position inside the field box
in vec3 a_Rand;   // per-particle randomness

uniform mat4  u_ViewProj;
uniform vec3  u_Center;
uniform vec3  u_Field;
uniform float u_Time;
uniform float u_PixelsPerUnit;
uniform float u_Size;

out float v_Alpha;
out float v_Rand;

void main()
{
	vec3 s = a_Seed;

	// Slow rise, gentle lateral drift.
	s.y = fract(s.y + u_Time * (0.008 + a_Rand.x * 0.016));
	s.x = fract(s.x + sin(u_Time * 0.15 + a_Rand.y * 6.283) * 0.020 + u_Time * 0.003);
	s.z = fract(s.z + cos(u_Time * 0.13 + a_Rand.z * 6.283) * 0.020);

	vec3 world;
	world.x = u_Center.x + (s.x - 0.5) * u_Field.x;
	world.z = u_Center.z + (s.z - 0.5) * u_Field.z;
	world.y = s.y * u_Field.y;

	gl_Position = u_ViewProj * vec4(world, 1.0);

	// Fade in off the ground, fade out before the top of the field.
	float fadeIn  = smoothstep(0.00, 0.10, s.y);
	float fadeOut = 1.0 - smoothstep(0.55, 1.00, s.y);
	v_Alpha = fadeIn * fadeOut;
	v_Rand = a_Rand.x;

	gl_PointSize = max(2.0, u_Size * u_PixelsPerUnit * (0.55 + a_Rand.x * 0.9));
}
