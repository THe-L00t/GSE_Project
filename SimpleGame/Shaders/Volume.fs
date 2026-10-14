#version 330

// Light scattered toward the eye by the low fog: sun shafts cut by the shadow map, and the torch cone.
// Runs at quarter resolution; the result is blurred and added in the composite pass.

#include "Env.glsl"

in vec2 v_UV;

uniform sampler2D       u_Depth;          // scene depth
uniform sampler2DShadow u_ShadowMap;
uniform mat4  u_InvViewProj;
uniform mat4  u_LightViewProj;
uniform float u_ShadowOn;
uniform int   u_Steps;
uniform float u_Time;
uniform vec4  u_TorchPos;                 // xyz, w = 1 while lit
uniform vec3  u_TorchDir;

layout(location = 0) out vec4 FragColor;

const float kFogTop = 12.0;               // shafts live in the air below this height
const vec3  kTorchColor = vec3(1.00, 0.88, 0.68);

vec3 Unproject(vec2 uv, float depth)
{
	vec4 p = u_InvViewProj * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
	return p.xyz / p.w;
}

float SunVisibility(vec3 pos)
{
	if (u_ShadowOn < 0.5) return 1.0;
	vec4 lp = u_LightViewProj * vec4(pos, 1.0);
	vec3 sc = lp.xyz / lp.w * 0.5 + 0.5;
	if (sc.x <= 0.0 || sc.x >= 1.0 || sc.y <= 0.0 || sc.y >= 1.0 || sc.z >= 1.0) return 1.0;
	return texture(u_ShadowMap, vec3(sc.xy, sc.z - 0.001));
}

float HenyeyGreenstein(float cosTheta, float g)
{
	float g2 = g * g;
	return (1.0 - g2) / (12.566 * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

void main()
{
	float depth = texture(u_Depth, v_UV).r;
	vec3 nearPos = Unproject(v_UV, 0.0);
	vec3 farPos = Unproject(v_UV, depth);
	vec3 ray = farPos - nearPos;
	float rayLen = length(ray);
	vec3 dir = ray / max(rayLen, 0.0001);

	// Clip the view ray to the fog layer; the long empty stretch above it scatters nothing worth marching.
	float tTop = dir.y < -0.0001 ? (nearPos.y - kFogTop) / -dir.y : 0.0;
	float t0 = clamp(tTop, 0.0, rayLen);
	float t1 = rayLen;
	if (t1 - t0 < 0.01)
	{
		FragColor = vec4(0.0);
		return;
	}

	Env env = EnvAt(u_TimeOfDay);

	// Shafts read best with the sun low; at noon they thin to a haze.
	float lowSun = 1.0 - smoothstep(0.15, 0.75, env.sunDir.y);
	float phase = mix(0.0796, HenyeyGreenstein(dot(dir, env.sunDir), 0.55), 0.6);
	vec3 sunLight = env.sunColor * phase * (0.3 + 1.1 * lowSun);
	float baseDensity = (env.fogDensity * 1.6 + 0.004) * (1.0 + u_Weather.x * 0.8);

	// Interleaved gradient noise staggers the samples per pixel; the blur afterwards hides the grain.
	float jitter = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715)) + u_Time * 0.618));
	float stepLen = (t1 - t0) / float(u_Steps);

	vec3 light = vec3(0.0);
	float transmit = 1.0;
	for (int i = 0; i < 32; ++i)
	{
		if (i >= u_Steps) break;
		vec3 pos = nearPos + dir * (t0 + (float(i) + jitter) * stepLen);
		float density = baseDensity * exp(-max(pos.y, 0.0) * 0.22);

		vec3 inscatter = sunLight * SunVisibility(pos);

		if (u_TorchPos.w > 0.5)
		{
			vec3 toPos = pos - u_TorchPos.xyz;
			float d = length(toPos);
			float cone = smoothstep(0.86, 0.95, dot(toPos / max(d, 0.001), u_TorchDir));
			float reach = 1.0 - smoothstep(6.0, 12.0, d);
			inscatter += kTorchColor * cone * reach * 2.4 / (1.0 + d * d * 0.08);
		}

		light += transmit * inscatter * density * stepLen;
		transmit *= exp(-density * stepLen);
	}

	FragColor = vec4(light, 1.0);
}
