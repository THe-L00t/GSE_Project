// Time-of-day palette shared by Lit, Overlay and Particle. Pulled in by Renderer::ReadFile.

uniform float u_TimeOfDay;      // [0,1), 0 is midnight
uniform float u_SporeExposure;  // [0,1]

struct Env
{
	vec3  sunDir;               // direction TOWARD the sun
	vec3  sunColor;
	vec3  skyColor;             // ambient from above
	vec3  groundColor;          // ambient bounce
	vec3  fogColor;
	float fogDensity;
	float saturation;
};

// midnight -> dawn (violet) -> noon (pale green) -> dusk (orange) -> midnight
const vec3 kSunColor[5] = vec3[5](vec3(0.17, 0.20, 0.36), vec3(0.64, 0.49, 0.70), vec3(0.98, 1.00, 0.90), vec3(1.00, 0.64, 0.36), vec3(0.17, 0.20, 0.36));
const vec3 kSkyColor[5] = vec3[5](vec3(0.07, 0.09, 0.16), vec3(0.30, 0.27, 0.41), vec3(0.40, 0.46, 0.42), vec3(0.38, 0.32, 0.30), vec3(0.07, 0.09, 0.16));
const vec3 kGroundColor[5] = vec3[5](vec3(0.02, 0.03, 0.05), vec3(0.06, 0.06, 0.10), vec3(0.09, 0.12, 0.09), vec3(0.07, 0.06, 0.06), vec3(0.02, 0.03, 0.05));
const vec3 kFogColor[5] = vec3[5](vec3(0.05, 0.07, 0.13), vec3(0.50, 0.45, 0.58), vec3(0.63, 0.70, 0.65), vec3(0.60, 0.46, 0.38), vec3(0.05, 0.07, 0.13));
const float kFogDensity[5] = float[5](0.028, 0.032, 0.012, 0.019, 0.028);
const float kSaturation[5] = float[5](0.60, 0.70, 0.80, 0.76, 0.60);

Env EnvAt(float timeOfDay)
{
	float t = clamp(timeOfDay, 0.0, 0.9999) * 4.0;
	int i = int(floor(t));
	float f = t - float(i);
	f = f * f * (3.0 - 2.0 * f);

	Env e;
	e.sunColor = mix(kSunColor[i], kSunColor[i + 1], f);
	e.skyColor = mix(kSkyColor[i], kSkyColor[i + 1], f);
	e.groundColor = mix(kGroundColor[i], kGroundColor[i + 1], f);
	e.fogColor = mix(kFogColor[i], kFogColor[i + 1], f);
	e.fogDensity = mix(kFogDensity[i], kFogDensity[i + 1], f) + u_SporeExposure * 0.010;
	e.saturation = mix(kSaturation[i], kSaturation[i + 1], f);

	// The sun rises at 0.25 and sets at 0.75. A floor keeps night readable.
	float ang = (timeOfDay - 0.25) * 6.2831853;
	e.sunDir = normalize(vec3(cos(ang) * 0.75, max(sin(ang), -0.15) * 0.9 + 0.18, 0.42));
	return e;
}

// The field is thickest at night; the village was engulfed this morning.
float SporeDensityAt(float timeOfDay)
{
	float day = clamp(sin((timeOfDay - 0.25) * 6.2831853), 0.0, 1.0);
	return 0.72 + 0.28 * (1.0 - day);
}
