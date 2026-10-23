#include "stdafx.h"
#include "Renderer.h"

#include <cstddef>
#include <cstring>
#include <vector>

#include "Models.h"
#include "Random.h"
#include "Texture.h"
#include "Dependencies\wglew.h"

// ASCII only: the original template's CP949 comments turned into mojibake in UTF-8 tools.

namespace
{
	const Vec3 kFireflyField(56.0f, 3.2f, 56.0f);   // the box the fireflies wander in, around the camera

	void LogValidation(GLuint program, const char* name)
	{
		if (program == 0) return;
		glValidateProgram(program);
		GLint ok = 0;
		glGetProgramiv(program, GL_VALIDATE_STATUS, &ok);
		if (ok) return;
		GLchar log[1024] = { 0 };
		glGetProgramInfoLog(program, sizeof(log), NULL, log);
		std::cout << name << " program does not validate: " << log << "\n";
	}

	// The far fog colour of Env.glsl, without the season tint: whatever the world leaves uncovered reads as mist.
	Vec3 FogColorAt(float timeOfDay, const WeatherParams& weather)
	{
		const Vec3 kFog[5] = { Vec3(0.05f, 0.07f, 0.13f), Vec3(0.50f, 0.45f, 0.58f), Vec3(0.63f, 0.70f, 0.65f),
							   Vec3(0.60f, 0.46f, 0.38f), Vec3(0.05f, 0.07f, 0.13f) };
		float t = Clampf(timeOfDay, 0.0f, 0.9999f) * 4.0f;
		int i = (int)floorf(t);
		float f = t - (float)i;
		f = f * f * (3.0f - 2.0f * f);
		Vec3 fog = LerpV(kFog[i], kFog[i + 1], f);

		float overcast = Clampf(weather.rain + weather.snow * 0.8f + weather.wind * 0.25f, 0.0f, 1.0f);
		float luma = fog.x * 0.299f + fog.y * 0.587f + fog.z * 0.114f;
		Vec3 grey = LerpV(Vec3(0.92f, 0.98f, 1.0f), Vec3(1.08f, 1.08f, 1.08f), weather.snow) * luma;
		return LerpV(fog, grey, overcast * 0.6f);
	}
}

Renderer::Renderer(int sizeX, int sizeY)
{
	Initialize(sizeX, sizeY);
}

Renderer::~Renderer()
{
}

void Renderer::Initialize(int sizeX, int sizeY)
{
	windowSizeX = sizeX;
	windowSizeY = sizeY;

	// A bound VAO is required on core profiles and harmless on compatibility ones.
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	solidRectShader = CompileShaders("Shaders/SolidRect.vs", "Shaders/SolidRect.fs");
	litShader       = CompileShaders("Shaders/Lit.vs",       "Shaders/Lit.fs");
	particleShader  = CompileShaders("Shaders/Particle.vs",  "Shaders/Particle.fs");
	overlayShader   = CompileShaders("Shaders/Overlay.vs",   "Shaders/Overlay.fs");
	postShader      = CompileShaders("Shaders/Post.vs",      "Shaders/Post.fs");
	volumeShader    = CompileShaders("Shaders/Post.vs",      "Shaders/Volume.fs");
	shadowShader    = CompileShaders("Shaders/Lit.vs",       "Shaders/Shadow.fs");
	textShader      = CompileShaders("Shaders/Text.vs",      "Shaders/Text.fs");

	CreateVertexBufferObjects();
	CacheUniformLocations();
	LoadModels();
	LoadGroundTextures();

	fonts[0].Load("Fonts/NanumGothic-Regular.ttf", 16.0f);
	fonts[1].Load("Fonts/NanumGothic-Bold.ttf", 22.0f);

	CreateTargets(sizeX, sizeY);
	CreateShadowMap();

	CreateNoiseTexture();

	// Every sampler has its own unit by now.
	LogValidation(litShader, "Lit");
	LogValidation(shadowShader, "Shadow");

	if (GLEW_VERSION_3_3 || GLEW_ARB_timer_query) glGenQueries(kQueryFrames * MARK_COUNT, &timeQueries[0][0]);
	std::cout << "GPU: " << (const char*)glGetString(GL_RENDERER) << "\n";

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glEnable(GL_PROGRAM_POINT_SIZE);

	if (solidRectShader > 0 && litShader > 0 && particleShader > 0 &&
		overlayShader > 0 && vboRect > 0 && !meshes.empty())
	{
		initialized = true;
	}
}

bool Renderer::IsInitialized()
{
	return initialized;
}

void Renderer::Resize(int sizeX, int sizeY)
{
	windowSizeX = sizeX;
	windowSizeY = sizeY;
	glViewport(0, 0, sizeX, sizeY);
	CreateTargets(sizeX, sizeY);
}

void Renderer::ApplySettings(const RenderSettings& next)
{
	bool targets = next.renderScale != settings.renderScale || next.msaa != settings.msaa;
	bool shadows = next.shadowSize != settings.shadowSize;
	settings = next;

	if (targets) CreateTargets((int)windowSizeX, (int)windowSizeY);
	if (shadows)
	{
		DeleteShadowMap();
		CreateShadowMap();
	}
	if (WGLEW_EXT_swap_control) wglSwapIntervalEXT(settings.vsync ? 1 : 0);
}

void Renderer::CreateTargets(int windowX, int windowY)
{
	DeleteTargets();
	if (postShader == 0 || windowX <= 0 || windowY <= 0) return;

	// The scene can render below window resolution; the composite pass scales it up.
	int sizeX = (int)((float)windowX * settings.renderScale + 0.5f);
	int sizeY = (int)((float)windowY * settings.renderScale + 0.5f);
	if (sizeX < 1) sizeX = 1;
	if (sizeY < 1) sizeY = 1;

	targetWidth = sizeX;
	targetHeight = sizeY;

	glGenTextures(1, &sceneTex);
	glBindTexture(GL_TEXTURE_2D, sceneTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, sizeX, sizeY, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Depth as a texture, so the light-shaft pass can find where each view ray ends.
	glGenTextures(1, &sceneDepth);
	glBindTexture(GL_TEXTURE_2D, sceneDepth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, sizeX, sizeY, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenFramebuffers(1, &sceneFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepth, 0);
	bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

	// MSAA smooths the long diagonal edges the quarter view is full of.
	GLint maxSamples = 0;
	glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
	samples = maxSamples < settings.msaa ? (int)maxSamples : settings.msaa;
	if (samples > 1)
	{
		glGenRenderbuffers(1, &msColor);
		glBindRenderbuffer(GL_RENDERBUFFER, msColor);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, sizeX, sizeY);

		glGenRenderbuffers(1, &msDepth);
		glBindRenderbuffer(GL_RENDERBUFFER, msDepth);
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, sizeX, sizeY);

		glGenFramebuffers(1, &msFbo);
		glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			glDeleteFramebuffers(1, &msFbo);
			glDeleteRenderbuffers(1, &msColor);
			glDeleteRenderbuffers(1, &msDepth);
			msFbo = msColor = msDepth = 0;
			samples = 0;
		}
	}

	bloomWidth = sizeX / 4 > 1 ? sizeX / 4 : 1;
	bloomHeight = sizeY / 4 > 1 ? sizeY / 4 : 1;
	for (int i = 0; i < 2; ++i)
	{
		glGenTextures(1, &bloomTex[i]);
		glBindTexture(GL_TEXTURE_2D, bloomTex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloomWidth, bloomHeight, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenFramebuffers(1, &bloomFbo[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTex[i], 0);
		ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

		glGenTextures(1, &volumeTex[i]);
		glBindTexture(GL_TEXTURE_2D, volumeTex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bloomWidth, bloomHeight, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glGenFramebuffers(1, &volumeFbo[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, volumeFbo[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, volumeTex[i], 0);
		volumeReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && (i == 0 || volumeReady);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	postReady = ok;
	std::cout << "Post: " << (postReady ? "HDR target ready" : "unavailable, drawing straight to the window")
		<< ", MSAA x" << (samples > 1 ? samples : 1) << "\n";
}

void Renderer::DeleteShadowMap()
{
	if (shadowFbo) glDeleteFramebuffers(1, &shadowFbo);
	if (shadowTex) glDeleteTextures(1, &shadowTex);
	shadowFbo = shadowTex = 0;
	shadowReady = false;
	shadowSize = 0;
}

void Renderer::CreateShadowMap()
{
	if (shadowShader == 0 || litShader == 0 || settings.shadowSize <= 0) return;
	shadowSize = settings.shadowSize;

	glGenTextures(1, &shadowTex);
	glBindTexture(GL_TEXTURE_2D, shadowTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize, shadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &shadowFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	shadowReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// The shadow map lives on texture unit 2, clear of the post-process units.
	glUseProgram(litShader);
	glUniform1i(lit.shadowMap, 2);
	glUseProgram(0);

	std::cout << "Shadows: " << (shadowReady ? "sun shadow map ready" : "unavailable") << "\n";
}

void Renderer::CreateNoiseTexture()
{
	const int size = 256;
	std::vector<float> values(size * size);
	for (int i = 0; i < size * size; ++i)
	{
		uint64_t h = MixSeed((uint64_t)i * 0x9E3779B97F4A7C15ULL + 0x51ED270BULL);
		values[i] = (float)(h >> 40) / 16777216.0f;
	}

	glGenTextures(1, &noiseTex);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, noiseTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, size, size, 0, GL_RED, GL_FLOAT, &values[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Unit 4 is left to the noise for the whole run; nothing else binds there.
	glActiveTexture(GL_TEXTURE0);

	glUseProgram(litShader);
	glUniform1i(glGetUniformLocation(litShader, "u_Noise"), 4);
	glUseProgram(0);
}

void Renderer::BindSceneTarget()
{
	if (postReady)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, samples > 1 ? msFbo : sceneFbo);
		glViewport(0, 0, targetWidth, targetHeight);
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, (GLsizei)windowSizeX, (GLsizei)windowSizeY);
	}
}

void Renderer::BeginShadowPass(const Vec3& focus)
{
	if (!shadowReady) return;

	// Same sun as EnvAt in Env.glsl, held above a floor so dawn and night shadows stay a sensible length.
	float ang = (timeOfDay - 0.25f) * 2.0f * kPi;
	float rise = Maxf(sinf(ang), -0.15f) * 0.9f + 0.18f;
	Vec3 sun = Normalize(Vec3(cosf(ang) * 0.75f, Maxf(rise, 0.30f), 0.42f));

	const float half = 30.0f;
	const float depth = 60.0f;
	Mat4 view = MatLookAt(sun * 100.0f, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));

	// Snap the focus to whole shadow texels so edges hold still while the camera glides.
	const float* m = view.m;
	float lx = m[0] * focus.x + m[4] * focus.y + m[8] * focus.z + m[12];
	float ly = m[1] * focus.x + m[5] * focus.y + m[9] * focus.z + m[13];
	float lz = m[2] * focus.x + m[6] * focus.y + m[10] * focus.z + m[14];
	float texel = half * 2.0f / (float)shadowSize;
	lx = floorf(lx / texel) * texel;
	ly = floorf(ly / texel) * texel;

	Mat4 proj = MatOrtho(lx - half, lx + half, ly - half, ly + half, -lz - depth, -lz + depth);
	lightViewProj = Mul(proj, view);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
	glViewport(0, 0, shadowSize, shadowSize);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glClear(GL_DEPTH_BUFFER_BIT);

	// Slope-scaled offset keeps the lit side of each surface from shadowing itself.
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f, 4.0f);

	glUseProgram(shadowShader);
	glUniformMatrix4fv(shadow.viewProj, 1, GL_FALSE, lightViewProj.m);
	glUniform1f(shadow.time, time);

	shadowPass = true;
}

void Renderer::EndShadowPass()
{
	FlushModels();
	MarkGpu(MARK_SHADOW);
	if (!shadowPass) return;

	shadowPass = false;
	shadowActive = true;
	glDisable(GL_POLYGON_OFFSET_FILL);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, shadowTex);
	glActiveTexture(GL_TEXTURE0);

	BindSceneTarget();
}

void Renderer::DeleteTargets()
{
	if (msFbo) glDeleteFramebuffers(1, &msFbo);
	if (msColor) glDeleteRenderbuffers(1, &msColor);
	if (msDepth) glDeleteRenderbuffers(1, &msDepth);
	if (sceneFbo) glDeleteFramebuffers(1, &sceneFbo);
	if (sceneTex) glDeleteTextures(1, &sceneTex);
	if (sceneDepth) glDeleteTextures(1, &sceneDepth);
	for (int i = 0; i < 2; ++i)
	{
		if (volumeFbo[i]) glDeleteFramebuffers(1, &volumeFbo[i]);
		if (volumeTex[i]) glDeleteTextures(1, &volumeTex[i]);
		volumeFbo[i] = volumeTex[i] = 0;
	}
	for (int i = 0; i < 2; ++i)
	{
		if (bloomFbo[i]) glDeleteFramebuffers(1, &bloomFbo[i]);
		if (bloomTex[i]) glDeleteTextures(1, &bloomTex[i]);
		bloomFbo[i] = bloomTex[i] = 0;
	}

	msFbo = msColor = msDepth = 0;
	sceneFbo = sceneTex = sceneDepth = 0;
	samples = 0;
	targetWidth = targetHeight = 0;
	postReady = false;
}

void Renderer::CreateVertexBufferObjects()
{
	float rect[] =
	{
		-1.f / windowSizeX, -1.f / windowSizeY, 0.f, -1.f / windowSizeX, 1.f / windowSizeY, 0.f, 1.f / windowSizeX, 1.f / windowSizeY, 0.f,
		-1.f / windowSizeX, -1.f / windowSizeY, 0.f,  1.f / windowSizeX, 1.f / windowSizeY, 0.f, 1.f / windowSizeX, -1.f / windowSizeY, 0.f,
	};

	glGenBuffers(1, &vboRect);
	glBindBuffer(GL_ARRAY_BUFFER, vboRect);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_STATIC_DRAW);

	float screen[] =
	{
		0.f, 0.f, 0.f,  1.f, 0.f, 0.f,  1.f, 1.f, 0.f,
		0.f, 0.f, 0.f,  1.f, 1.f, 0.f,  0.f, 1.f, 0.f,
	};

	glGenBuffers(1, &vboScreen);
	glBindBuffer(GL_ARRAY_BUFFER, vboScreen);
	glBufferData(GL_ARRAY_BUFFER, sizeof(screen), screen, GL_STATIC_DRAW);

	sporeCount = 7000;
	std::vector<float> spores;
	spores.reserve(sporeCount * 6);
	srand(20260909);
	for (int i = 0; i < sporeCount; ++i)
	{
		spores.push_back(RandUnit());
		spores.push_back(RandUnit());
		spores.push_back(RandUnit());
		spores.push_back(RandUnit());
		spores.push_back(RandUnit());
		spores.push_back(RandUnit());
	}

	glGenBuffers(1, &vboSpores);
	glBindBuffer(GL_ARRAY_BUFFER, vboSpores);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(spores.size() * sizeof(float)), &spores[0], GL_STATIC_DRAW);

	// The fireflies' seeds stay on the CPU too, so their light on the ground can follow them.
	fireflySeeds.assign(spores.begin(), spores.begin() + kFireflyCount * 6);

	glGenBuffers(1, &vboText);
	glGenBuffers(1, &vboInstances);

	glGenBuffers(1, &boneBuffer);
	glGenTextures(1, &boneTex);
}

void Renderer::CacheUniformLocations()
{
	lit.viewProj = glGetUniformLocation(litShader, "u_ViewProj");
	lit.model = glGetUniformLocation(litShader, "u_Model");
	lit.mode = glGetUniformLocation(litShader, "u_Mode");
	lit.timeOfDay = glGetUniformLocation(litShader, "u_TimeOfDay");
	lit.sporeExposure = glGetUniformLocation(litShader, "u_SporeExposure");
	lit.fogOrigin = glGetUniformLocation(litShader, "u_FogOrigin");
	lit.time = glGetUniformLocation(litShader, "u_Time");
	lit.camPos = glGetUniformLocation(litShader, "u_CamPos");
	lit.stage = glGetUniformLocation(litShader, "u_Stage");
	lit.neighborStage = glGetUniformLocation(litShader, "u_NeighborStage");
	lit.chunkCenter = glGetUniformLocation(litShader, "u_ChunkCenter");
	lit.chunkSize = glGetUniformLocation(litShader, "u_ChunkSize");
	lit.damp = glGetUniformLocation(litShader, "u_Damp");
	lit.road = glGetUniformLocation(litShader, "u_Road");
	lit.city = glGetUniformLocation(litShader, "u_City");
	lit.lightCount = glGetUniformLocation(litShader, "u_LightCount");
	lit.lightPos = glGetUniformLocation(litShader, "u_LightPos");
	lit.lightColor = glGetUniformLocation(litShader, "u_LightColor");
	lit.shadowMap = glGetUniformLocation(litShader, "u_ShadowMap");
	lit.lightViewProj = glGetUniformLocation(litShader, "u_LightViewProj");
	lit.shadowOn = glGetUniformLocation(litShader, "u_ShadowOn");
	lit.season = glGetUniformLocation(litShader, "u_Season");
	lit.weather = glGetUniformLocation(litShader, "u_Weather");
	lit.torchPos = glGetUniformLocation(litShader, "u_TorchPos");
	lit.torchDir = glGetUniformLocation(litShader, "u_TorchDir");
	lit.glowCount = glGetUniformLocation(litShader, "u_GlowCount");
	lit.glowPos = glGetUniformLocation(litShader, "u_GlowPos");
	lit.cutaway = glGetUniformLocation(litShader, "u_Cutaway");
	lit.material = glGetUniformLocation(litShader, "u_Material");
	lit.groundTex = glGetUniformLocation(litShader, "u_GroundTex");

	// Each sampler keeps its own unit for the whole run.
	glUseProgram(litShader);
	glUniform1i(glGetUniformLocation(litShader, "u_Albedo"), kUnitAlbedo);
	glUniform1i(glGetUniformLocation(litShader, "u_NormalMap"), kUnitNormal);
	glUniform1i(glGetUniformLocation(litShader, "u_Mask"), kUnitMask);
	glUniform1i(glGetUniformLocation(litShader, "u_GroundAlbedo"), kUnitGroundAlbedo);
	glUniform1i(glGetUniformLocation(litShader, "u_GroundNormal"), kUnitGroundNormal);
	glUniform1i(glGetUniformLocation(litShader, "u_GroundHeight"), kUnitGroundHeight);
	glUniform1i(glGetUniformLocation(litShader, "u_Bones"), kUnitBones);
	glUniform4f(lit.material, 0.0f, 0.0f, 0.0f, 0.0f);
	glUniform1f(lit.groundTex, 0.0f);
	glUseProgram(0);

	shadow.viewProj = glGetUniformLocation(shadowShader, "u_ViewProj");
	shadow.model = glGetUniformLocation(shadowShader, "u_Model");
	shadow.time = glGetUniformLocation(shadowShader, "u_Time");
	glUseProgram(shadowShader);
	glUniform1i(glGetUniformLocation(shadowShader, "u_Bones"), kUnitBones);
	glUseProgram(0);

	overlay.rect = glGetUniformLocation(overlayShader, "u_Rect");
	overlay.mode = glGetUniformLocation(overlayShader, "u_Mode");
	overlay.color = glGetUniformLocation(overlayShader, "u_Color");
	overlay.color2 = glGetUniformLocation(overlayShader, "u_Color2");
	overlay.fill = glGetUniformLocation(overlayShader, "u_Fill");
	overlay.vignette = glGetUniformLocation(overlayShader, "u_Vignette");
	overlay.haze = glGetUniformLocation(overlayShader, "u_Haze");
	overlay.hazeColor = glGetUniformLocation(overlayShader, "u_HazeColor");
	overlay.time = glGetUniformLocation(overlayShader, "u_Time");
	overlay.timeOfDay = glGetUniformLocation(overlayShader, "u_TimeOfDay");
	overlay.sporeExposure = glGetUniformLocation(overlayShader, "u_SporeExposure");
	overlay.season = glGetUniformLocation(overlayShader, "u_Season");
	overlay.weather = glGetUniformLocation(overlayShader, "u_Weather");
	overlay.positionAttrib = glGetAttribLocation(overlayShader, "a_Position");

	particle.viewProj = glGetUniformLocation(particleShader, "u_ViewProj");
	particle.center = glGetUniformLocation(particleShader, "u_Center");
	particle.field = glGetUniformLocation(particleShader, "u_Field");
	particle.time = glGetUniformLocation(particleShader, "u_Time");
	particle.pixelsPerUnit = glGetUniformLocation(particleShader, "u_PixelsPerUnit");
	particle.size = glGetUniformLocation(particleShader, "u_Size");
	particle.color = glGetUniformLocation(particleShader, "u_Color");
	particle.densityScale = glGetUniformLocation(particleShader, "u_DensityScale");
	particle.timeOfDay = glGetUniformLocation(particleShader, "u_TimeOfDay");
	particle.style = glGetUniformLocation(particleShader, "u_Style");
	particle.season = glGetUniformLocation(particleShader, "u_Season");
	particle.weather = glGetUniformLocation(particleShader, "u_Weather");
	particle.windDrift = glGetUniformLocation(particleShader, "u_WindDrift");
	particle.seedAttrib = glGetAttribLocation(particleShader, "a_Seed");
	particle.randAttrib = glGetAttribLocation(particleShader, "a_Rand");

	post.mode = glGetUniformLocation(postShader, "u_Mode");
	post.source = glGetUniformLocation(postShader, "u_Source");
	post.bloom = glGetUniformLocation(postShader, "u_Bloom");
	post.texel = glGetUniformLocation(postShader, "u_Texel");
	post.direction = glGetUniformLocation(postShader, "u_Direction");
	post.bloomStrength = glGetUniformLocation(postShader, "u_BloomStrength");
	post.sporeExposure = glGetUniformLocation(postShader, "u_SporeExposure");
	post.time = glGetUniformLocation(postShader, "u_Time");
	post.positionAttrib = glGetAttribLocation(postShader, "a_Position");
	post.volume = glGetUniformLocation(postShader, "u_Volume");
	post.volumeOn = glGetUniformLocation(postShader, "u_VolumeOn");
	post.dream = glGetUniformLocation(postShader, "u_Dream");
	post.frost = glGetUniformLocation(postShader, "u_Frost");
	post.hunger = glGetUniformLocation(postShader, "u_Hunger");
	post.grade = glGetUniformLocation(postShader, "u_Grade");
	post.hurt = glGetUniformLocation(postShader, "u_Hurt");
	post.focus = glGetUniformLocation(postShader, "u_Focus");
	post.tiltShift = glGetUniformLocation(postShader, "u_TiltShift");
	post.memo = glGetUniformLocation(postShader, "u_Memo");
	post.recall = glGetUniformLocation(postShader, "u_Recall");
	post.ripple = glGetUniformLocation(postShader, "u_Ripple");
	post.heat = glGetUniformLocation(postShader, "u_Heat");
	post.depth = glGetUniformLocation(postShader, "u_Depth");
	post.ao = glGetUniformLocation(postShader, "u_Ao");

	volume.invViewProj = glGetUniformLocation(volumeShader, "u_InvViewProj");
	volume.lightViewProj = glGetUniformLocation(volumeShader, "u_LightViewProj");
	volume.shadowOn = glGetUniformLocation(volumeShader, "u_ShadowOn");
	volume.steps = glGetUniformLocation(volumeShader, "u_Steps");
	volume.time = glGetUniformLocation(volumeShader, "u_Time");
	volume.timeOfDay = glGetUniformLocation(volumeShader, "u_TimeOfDay");
	volume.sporeExposure = glGetUniformLocation(volumeShader, "u_SporeExposure");
	volume.season = glGetUniformLocation(volumeShader, "u_Season");
	volume.weather = glGetUniformLocation(volumeShader, "u_Weather");
	volume.torchPos = glGetUniformLocation(volumeShader, "u_TorchPos");
	volume.torchDir = glGetUniformLocation(volumeShader, "u_TorchDir");
	volume.positionAttrib = glGetAttribLocation(volumeShader, "a_Position");
	glUseProgram(volumeShader);
	glUniform1i(glGetUniformLocation(volumeShader, "u_Depth"), 0);
	glUniform1i(glGetUniformLocation(volumeShader, "u_ShadowMap"), 2);
	glUseProgram(0);

	textScreen = glGetUniformLocation(textShader, "u_Screen");
	textAtlas = glGetUniformLocation(textShader, "u_Atlas");
}

void Renderer::LoadModels()
{
	std::vector<MeshData> meshData;
	int built = 0;
	LoadModelMeshes(meshData, built);

	meshes.resize(meshData.size());
	modelQueue.resize(meshData.size());
	skeletons.resize(meshData.size());
	int assets = 0;
	for (size_t i = 0; i < meshData.size(); ++i)
	{
		// A model made in Blender wins; the shape recipe stands in when its file is missing.
		if (LoadAssetModel((int)i))
		{
			++assets;
			continue;
		}

		const std::vector<MeshVertex>& vertices = meshData[i].vertices;
		if (vertices.empty()) continue;

		glGenBuffers(1, &meshes[i].vbo);
		glBindBuffer(GL_ARRAY_BUFFER, meshes[i].vbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(MeshVertex)), &vertices[0], GL_STATIC_DRAW);
		meshes[i].count = (int)vertices.size();
	}

	std::cout << "Models: " << assets << " from Assets/Models, the rest from shape recipes ("
		<< ((int)meshData.size() - built) << " cached, " << built << " built).\n";
}

bool Renderer::LoadAssetModel(int id)
{
	std::string path;
	if (!FindDataFile(std::string("Assets/Models/") + ModelName(id) + ".gsm", path)) return false;

	AssetMesh asset;
	if (!LoadAssetMesh(path, asset)) return false;

	GpuMesh& g = meshes[id];
	glGenBuffers(1, &g.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(asset.vertices.size() * sizeof(AssetVertex)), &asset.vertices[0], GL_STATIC_DRAW);

	g.index32 = asset.vertices.size() > 65535;
	glGenBuffers(1, &g.ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ebo);
	if (g.index32)
	{
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(asset.indices.size() * sizeof(uint32_t)), &asset.indices[0], GL_STATIC_DRAW);
	}
	else
	{
		std::vector<uint16_t> shortIndices(asset.indices.begin(), asset.indices.end());
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(shortIndices.size() * sizeof(uint16_t)), &shortIndices[0], GL_STATIC_DRAW);
	}
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	g.count = (int)asset.indices.size();
	g.asset = true;
	g.specular = asset.specular;
	g.emissive = asset.emissive;

	// A missing map falls back to a plain one, so the model still draws: white, flat, unoccluded and rough.
	const unsigned char fallback[3][4] = { { 255, 255, 255, 255 }, { 128, 128, 255, 255 }, { 255, 210, 0, 255 } };
	for (int t = 0; t < 3; ++t)
	{
		if (!asset.textures[t].empty()) g.textures[t] = LoadTexture2D("Assets/Textures/" + asset.textures[t]);
		if (g.textures[t] == 0) g.textures[t] = SolidTexture(fallback[t][0], fallback[t][1], fallback[t][2], fallback[t][3]);
	}

	if (asset.skeleton.BoneCount() > 0)
	{
		skeletons[id] = asset.skeleton;
		std::string clips;
		if (FindDataFile(std::string("Assets/Models/") + ModelName(id) + ".gsa", clips))
			LoadAssetClips(clips, skeletons[id]);
		g.skinned = true;
	}
	return true;
}

void Renderer::LoadGroundTextures()
{
	const char* layers[5] = { "grass", "moss", "dirt", "concrete", "forest" };
	std::vector<std::string> albedo, normal, height;
	for (int i = 0; i < 5; ++i)
	{
		std::string base = std::string("Assets/Textures/ground_") + layers[i];
		albedo.push_back(base + "_albedo.jpg");
		normal.push_back(base + "_normal.jpg");
		height.push_back(base + "_height.jpg");
	}

	groundAlbedo = LoadTextureArray(albedo);
	groundNormal = LoadTextureArray(normal);
	groundHeight = LoadTextureArray(height);
	if (groundAlbedo == 0 || groundNormal == 0 || groundHeight == 0)
	{
		// All or nothing: the ground falls back to its painted colours.
		if (groundAlbedo) glDeleteTextures(1, &groundAlbedo);
		if (groundNormal) glDeleteTextures(1, &groundNormal);
		if (groundHeight) glDeleteTextures(1, &groundHeight);
		groundAlbedo = groundNormal = groundHeight = 0;
	}
	std::cout << "Ground: " << (groundAlbedo ? "Blender layers loaded" : "painted colours") << "\n";
}

const Skeleton* Renderer::ModelSkeleton(int id) const
{
	if (id < 0 || id >= (int)meshes.size() || !meshes[id].skinned) return nullptr;
	return &skeletons[id];
}

void Renderer::AddShader(GLuint program, const char* shaderText, GLenum shaderType)
{
	GLuint shaderObj = glCreateShader(shaderType);

	if (shaderObj == 0)
	{
		fprintf(stderr, "Error creating shader type %d\n", shaderType);
	}

	const GLchar* p[1];
	p[0] = shaderText;
	GLint lengths[1];
	lengths[0] = (GLint)strlen(shaderText);

	glShaderSource(shaderObj, 1, p, lengths);
	glCompileShader(shaderObj);

	GLint success;
	glGetShaderiv(shaderObj, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		GLchar infoLog[1024];
		glGetShaderInfoLog(shaderObj, 1024, NULL, infoLog);
		fprintf(stderr, "Error compiling shader type %d: '%s'\n", shaderType, infoLog);
	}

	glAttachShader(program, shaderObj);
}

bool Renderer::ReadFile(const char* filename, std::string* target)
{
	// The working directory differs between running from Visual Studio and
	// running the built exe, so try the usual spots.
	const char* prefixes[] = { "", "./", "../SimpleGame/", "./SimpleGame/", "../../SimpleGame/" };
	const std::string includeDirective = "#include \"";

	for (int i = 0; i < 5; ++i)
	{
		std::string path = std::string(prefixes[i]) + filename;
		std::ifstream file(path.c_str());
		if (file.fail())
		{
			file.close();
			continue;
		}

		std::string line;
		while (getline(file, line))
		{
			// GLSL has no includes; a line like #include "Env.glsl" splices in a file from Shaders/.
			if (line.compare(0, includeDirective.size(), includeDirective) == 0)
			{
				size_t close = line.find('"', includeDirective.size());
				std::string name = line.substr(includeDirective.size(), close == std::string::npos ? std::string::npos : close - includeDirective.size());
				if (!ReadFile((std::string("Shaders/") + name).c_str(), target))
					std::cout << "Shader include " << name << " not found.\n";
				continue;
			}

			target->append(line);
			target->append("\n");
		}
		file.close();
		return true;
	}

	std::cout << filename << " file loading failed.. \n";
	return false;
}

GLuint Renderer::CompileShaders(const char* filenameVS, const char* filenameFS)
{
	GLuint program = glCreateProgram();

	if (program == 0)
	{
		fprintf(stderr, "Error creating shader program\n");
		return 0;
	}

	std::string vs, fs;

	if (!ReadFile(filenameVS, &vs))
	{
		printf("Error compiling vertex shader\n");
		return 0;
	}

	if (!ReadFile(filenameFS, &fs))
	{
		printf("Error compiling fragment shader\n");
		return 0;
	}

	AddShader(program, vs.c_str(), GL_VERTEX_SHADER);
	AddShader(program, fs.c_str(), GL_FRAGMENT_SHADER);

	GLint success = 0;
	GLchar errorLog[1024] = { 0 };

	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);

	if (success == 0)
	{
		glGetProgramInfoLog(program, sizeof(errorLog), NULL, errorLog);
		std::cout << filenameVS << ", " << filenameFS << " Error linking shader program\n" << errorLog;
		return 0;
	}

	// Validation waits for LogValidation at the end of Initialize: right after linking every sampler still
	// points at unit 0, and samplers of different kinds on one unit would fail it for no real reason.
	std::cout << filenameVS << ", " << filenameFS << " shader compiled.\n";
	return program;
}

void Renderer::MarkGpu(int mark)
{
	if (timeQueries[queryFrame][mark] != 0) glQueryCounter(timeQueries[queryFrame][mark], GL_TIMESTAMP);
}

void Renderer::EndFrame()
{
	if (timeQueries[0][0] == 0) return;
	MarkGpu(MARK_UI);
	queryPending[queryFrame] = true;
	queryFrame = (queryFrame + 1) % kQueryFrames;

	// The slot about to be reused was written kQueryFrames - 1 frames ago; read it only once it is ready.
	if (!queryPending[queryFrame]) return;
	GLint ready = 0;
	glGetQueryObjectiv(timeQueries[queryFrame][MARK_UI], GL_QUERY_RESULT_AVAILABLE, &ready);
	if (!ready) return;

	GLuint64 stamp[MARK_COUNT];
	for (int i = 0; i < MARK_COUNT; ++i)
		glGetQueryObjectui64v(timeQueries[queryFrame][i], GL_QUERY_RESULT, &stamp[i]);
	queryPending[queryFrame] = false;

	gpuTimes.shadow = (float)(stamp[MARK_SHADOW] - stamp[MARK_BEGIN]) * 1e-6f;
	gpuTimes.scene = (float)(stamp[MARK_SCENE] - stamp[MARK_SHADOW]) * 1e-6f;
	gpuTimes.post = (float)(stamp[MARK_POST] - stamp[MARK_SCENE]) * 1e-6f;
	gpuTimes.ui = (float)(stamp[MARK_UI] - stamp[MARK_POST]) * 1e-6f;
}

void Renderer::BeginFrame()
{
	MarkGpu(MARK_BEGIN);
	BindSceneTarget();
	shadowActive = false;

	// The ground should fill the view; if a frame ever leaves a gap, it shows the fog instead of black.
	Vec3 clear = FogColorAt(timeOfDay, weather);
	glClearColor(clear.x, clear.y, clear.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::EndScene()
{
	FlushModels();
	MarkGpu(MARK_SCENE);
	if (!postReady)
	{
		MarkGpu(MARK_POST);
		return;
	}

	if (samples > 1)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFbo);
		glBlitFramebuffer(0, 0, targetWidth, targetHeight, 0, 0, targetWidth, targetHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		if (VolumeOn() || AoOn())
			glBlitFramebuffer(0, 0, targetWidth, targetHeight, 0, 0, targetWidth, targetHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);

	if (VolumeOn()) DrawVolumePass();

	glUseProgram(postShader);
	glUniform1i(post.source, 0);
	glUniform1i(post.bloom, 1);
	glUniform1f(post.bloomStrength, 0.65f);
	glUniform1f(post.sporeExposure, sporeExposure);
	glUniform1f(post.time, time);
	glUniform1f(post.dream, screenFx.dream);
	glUniform1f(post.frost, screenFx.frost);
	glUniform1f(post.hunger, screenFx.hunger);
	glUniform3f(post.grade, screenFx.grade.x, screenFx.grade.y, screenFx.grade.z);
	glUniform1f(post.hurt, screenFx.hurt);
	float focusU = 0.5f, focusV = 0.5f;
	ProjectUv(screenFx.focus, focusU, focusV);
	glUniform2f(post.focus, focusU, focusV);
	glUniform1f(post.tiltShift, screenFx.tiltShift);
	glUniform1f(post.memo, screenFx.memo);
	glUniform1f(post.recall, screenFx.recall);
	float rippleU = 0.5f, rippleV = 0.5f;
	ProjectUv(screenFx.rippleAt, rippleU, rippleV);
	glUniform3f(post.ripple, rippleU, rippleV, screenFx.ripple);

	// One world unit in screen heights: the orthographic view keeps it the same everywhere on screen.
	float heat[8] = {};
	for (int i = 0; i < 2; ++i)
	{
		float* h = &heat[i * 4];
		if (screenFx.heatStrength[i] <= 0.0f || !ProjectUv(screenFx.heat[i], h[0], h[1])) continue;
		h[2] = pixelsPerUnit / (float)targetHeight;
		h[3] = screenFx.heatStrength[i];
	}
	glUniform4fv(post.heat, 2, heat);

	glBindBuffer(GL_ARRAY_BUFFER, vboScreen);
	glEnableVertexAttribArray(post.positionAttrib);
	glVertexAttribPointer(post.positionAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	const float bw = (float)bloomWidth;
	const float bh = (float)bloomHeight;
	DrawPostPass(bloomFbo[0], bloomWidth, bloomHeight, 0, sceneTex, (float)targetWidth, (float)targetHeight);

	// Two rounds of separable blur, the second twice as wide, give a soft broad glow.
	for (int round = 0; round < 2; ++round)
	{
		float spread = (float)(round + 1);
		glUniform2f(post.direction, spread / bw, 0.0f);
		DrawPostPass(bloomFbo[1], bloomWidth, bloomHeight, 1, bloomTex[0], bw, bh);
		glUniform2f(post.direction, 0.0f, spread / bh);
		DrawPostPass(bloomFbo[0], bloomWidth, bloomHeight, 1, bloomTex[1], bw, bh);
	}

	// One narrow blur takes the per-pixel jitter out of the shafts.
	if (VolumeOn())
	{
		glUniform2f(post.direction, 1.0f / bw, 0.0f);
		DrawPostPass(volumeFbo[1], bloomWidth, bloomHeight, 1, volumeTex[0], bw, bh);
		glUniform2f(post.direction, 0.0f, 1.0f / bh);
		DrawPostPass(volumeFbo[0], bloomWidth, bloomHeight, 1, volumeTex[1], bw, bh);
	}

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, AoOn() ? sceneDepth : 0);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, VolumeOn() ? volumeTex[0] : 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, bloomTex[0]);
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(post.volume, 3);
	glUniform1f(post.volumeOn, VolumeOn() ? 1.0f : 0.0f);
	glUniform1i(post.depth, 4);
	glUniform3f(post.ao, AoOn() ? 1.0f : 0.0f, depthRange, pixelsPerUnit / (float)targetHeight);
	DrawPostPass(0, (int)windowSizeX, (int)windowSizeY, 2, sceneTex, (float)targetWidth, (float)targetHeight);

	// Unit 4 goes back to the noise the lit shader reads there; the depth only borrowed it for the composite.
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, noiseTex);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisableVertexAttribArray(post.positionAttrib);

	glEnable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	MarkGpu(MARK_POST);
}

void Renderer::DrawPostPass(GLuint target, int width, int height, int mode, GLuint source, float sourceWidth, float sourceHeight)
{
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glViewport(0, 0, width, height);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, source);
	glUniform1i(post.mode, mode);
	glUniform2f(post.texel, 1.0f / sourceWidth, 1.0f / sourceHeight);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	++drawCalls;
}

bool Renderer::VolumeOn() const
{
	return settings.volumetric > 0 && volumeShader != 0 && volumeReady && postReady;
}

bool Renderer::AoOn() const
{
	return settings.ssao && postReady;
}

void Renderer::DrawVolumePass()
{
	glUseProgram(volumeShader);
	Mat4 invViewProj = Inverse(viewProj);
	glUniformMatrix4fv(volume.invViewProj, 1, GL_FALSE, invViewProj.m);
	glUniformMatrix4fv(volume.lightViewProj, 1, GL_FALSE, lightViewProj.m);
	glUniform1f(volume.shadowOn, shadowActive ? 1.0f : 0.0f);
	glUniform1i(volume.steps, settings.volumetric >= 2 ? 24 : 12);
	glUniform1f(volume.time, time);
	glUniform1f(volume.timeOfDay, timeOfDay);
	glUniform1f(volume.sporeExposure, sporeExposure);
	UploadWeather(volume.season, volume.weather);
	glUniform4f(volume.torchPos, torchPos.x, torchPos.y, torchPos.z, torchOn ? 1.0f : 0.0f);
	glUniform3f(volume.torchDir, torchDir.x, torchDir.y, torchDir.z);

	glBindFramebuffer(GL_FRAMEBUFFER, volumeFbo[0]);
	glViewport(0, 0, bloomWidth, bloomHeight);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, sceneDepth);

	glBindBuffer(GL_ARRAY_BUFFER, vboScreen);
	glEnableVertexAttribArray(volume.positionAttrib);
	glVertexAttribPointer(volume.positionAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(volume.positionAttrib);
	++drawCalls;
}

void Renderer::SetTorch(bool on, const Vec3& pos, const Vec3& dir)
{
	torchOn = on;
	torchPos = pos;
	torchDir = dir;
}

void Renderer::SetCutaway(const Vec3& playerPos, float strength)
{
	cutawayPos = playerPos;
	cutawayStrength = strength;
}

int Renderer::TakeDrawCalls()
{
	int count = drawCalls;
	drawCalls = 0;
	return count;
}

void Renderer::SetCamera(const Mat4& view, const Mat4& proj, const Vec3& eye, float pxPerUnit)
{
	viewProj = Mul(proj, view);
	depthRange = 2.0f / Maxf(fabsf(proj.m[10]), 1e-6f);
	camPos = eye;
	// Point sprites are sized in target pixels, which shrink with the render scale.
	pixelsPerUnit = pxPerUnit * (postReady ? (float)targetHeight / (float)windowSizeY : 1.0f);
}

bool Renderer::WorldToScreen(const Vec3& pos, float& sx, float& sy) const
{
	const float* m = viewProj.m;
	float cx = m[0] * pos.x + m[4] * pos.y + m[8] * pos.z + m[12];
	float cy = m[1] * pos.x + m[5] * pos.y + m[9] * pos.z + m[13];
	float cw = m[3] * pos.x + m[7] * pos.y + m[11] * pos.z + m[15];
	if (cw <= 1e-5f) return false;

	sx = (cx / cw * 0.5f + 0.5f) * (float)windowSizeX;
	sy = (0.5f - cy / cw * 0.5f) * (float)windowSizeY;
	return true;
}

bool Renderer::ProjectUv(const Vec3& pos, float& u, float& v) const
{
	float sx, sy;
	if (!WorldToScreen(pos, sx, sy)) return false;
	u = sx / (float)windowSizeX;
	v = 1.0f - sy / (float)windowSizeY;
	return true;
}

void Renderer::SetFrame(float dayTime, float exposure, const Vec3& fogCenter, float seconds)
{
	timeOfDay = dayTime;
	sporeExposure = exposure;
	fogOrigin = fogCenter;
	time = seconds;
}

void Renderer::UploadWeather(GLint seasonLoc, GLint weatherLoc) const
{
	glUniform4f(seasonLoc, weather.summer, weather.autumn, weather.winter, weather.snowCover);
	glUniform4f(weatherLoc, weather.rain, weather.snow, weather.wind, weather.wetness);
}

void Renderer::ClearLights()
{
	lightCount = 0;
	lightsDirty = true;
}

void Renderer::AddLight(const Vec3& pos, const Vec3& color, float radius)
{
	if (lightCount >= kMaxLights) return;

	float* p = &lightPos[lightCount * 4];
	p[0] = pos.x;
	p[1] = pos.y;
	p[2] = pos.z;
	p[3] = radius;

	float* c = &lightColor[lightCount * 3];
	c[0] = color.x;
	c[1] = color.y;
	c[2] = color.z;

	++lightCount;
	lightsDirty = true;
}

void Renderer::BindLit(const Mat4& model, int mode)
{
	glUseProgram(litShader);

	glUniformMatrix4fv(lit.viewProj, 1, GL_FALSE, viewProj.m);
	glUniformMatrix4fv(lit.model, 1, GL_FALSE, model.m);
	glUniform1i(lit.mode, mode);

	glUniform1f(lit.timeOfDay, timeOfDay);
	glUniform1f(lit.sporeExposure, sporeExposure);
	glUniform3f(lit.fogOrigin, fogOrigin.x, fogOrigin.y, fogOrigin.z);
	glUniform1f(lit.time, time);
	glUniform3f(lit.camPos, camPos.x, camPos.y, camPos.z);
	glUniformMatrix4fv(lit.lightViewProj, 1, GL_FALSE, lightViewProj.m);
	glUniform1f(lit.shadowOn, shadowActive ? 1.0f : 0.0f);
	UploadWeather(lit.season, lit.weather);
	glUniform4f(lit.torchPos, torchPos.x, torchPos.y, torchPos.z, torchOn ? 1.0f : 0.0f);
	glUniform3f(lit.torchDir, torchDir.x, torchDir.y, torchDir.z);
	glUniform4f(lit.cutaway, cutawayPos.x, cutawayPos.y, cutawayPos.z, cutawayStrength);

	// Uniforms stay with the program, so the light list goes up once per change.
	if (lightsDirty)
	{
		glUniform1i(lit.lightCount, lightCount);
		glUniform4fv(lit.lightPos, kMaxLights, lightPos);
		glUniform3fv(lit.lightColor, kMaxLights, lightColor);
		lightsDirty = false;
	}
	if (glowsDirty)
	{
		glUniform1i(lit.glowCount, glowCount);
		glUniform4fv(lit.glowPos, kMaxGlows, glowPos);
		glowsDirty = false;
	}

	// A single draw reads the instance attributes as constants: identity placement, no tint.
	glVertexAttrib4f(4, 1.0f, 0.0f, 0.0f, 0.0f);
	glVertexAttrib4f(5, 0.0f, 1.0f, 0.0f, 0.0f);
	glVertexAttrib4f(6, 0.0f, 0.0f, 1.0f, 0.0f);
	glVertexAttrib4f(7, 0.0f, 0.0f, 0.0f, 1.0f);
	glVertexAttrib4f(8, 1.0f, 1.0f, 1.0f, 0.0f);
	glVertexAttrib4f(9, 0.0f, 0.0f, 0.0f, 0.0f);
	glVertexAttrib4f(10, -1.0f, 0.0f, 0.0f, 0.0f);
}

void Renderer::BindMesh(int id)
{
	// Attribute locations are fixed by layout qualifiers in Lit.vs.
	const GpuMesh& mesh = meshes[id];
	glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
	for (GLuint loc = 0; loc <= 3; ++loc)
		glEnableVertexAttribArray(loc);

	if (!mesh.asset)
	{
		const GLsizei stride = (GLsizei)sizeof(MeshVertex);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3));
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 6));
		glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 9));

		// A recipe mesh has no maps and no bones: the shader reads these as constants.
		for (GLuint loc = 11; loc <= 14; ++loc)
			glDisableVertexAttribArray(loc);
		glVertexAttrib4f(11, 0.0f, 0.0f, 0.0f, 0.0f);
		glVertexAttrib4f(12, 1.0f, 0.0f, 0.0f, 1.0f);
		glVertexAttrib4f(13, 0.0f, 0.0f, 0.0f, 0.0f);
		glVertexAttrib4f(14, 0.0f, 0.0f, 0.0f, 0.0f);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		if (!shadowPass) glUniform4f(lit.material, 0.0f, 0.0f, 0.0f, 0.0f);
		return;
	}

	const GLsizei stride = (GLsizei)sizeof(AssetVertex);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(AssetVertex, px));
	glVertexAttribPointer(1, 3, GL_SHORT, GL_TRUE, stride, (void*)offsetof(AssetVertex, normal));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)offsetof(AssetVertex, color));
	glVertexAttribPointer(3, 1, GL_UNSIGNED_INT, GL_FALSE, stride, (void*)offsetof(AssetVertex, anim));
	for (GLuint loc = 11; loc <= 14; ++loc)
		glEnableVertexAttribArray(loc);
	glVertexAttribPointer(11, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(AssetVertex, u));
	glVertexAttribPointer(12, 4, GL_SHORT, GL_TRUE, stride, (void*)offsetof(AssetVertex, tangent));
	glVertexAttribPointer(13, 4, GL_UNSIGNED_BYTE, GL_FALSE, stride, (void*)offsetof(AssetVertex, joints));
	glVertexAttribPointer(14, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)offsetof(AssetVertex, weights));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);

	if (!shadowPass)
	{
		for (int t = 0; t < 3; ++t)
		{
			glActiveTexture(GL_TEXTURE0 + kUnitAlbedo + t);
			glBindTexture(GL_TEXTURE_2D, mesh.textures[t]);
		}
		glActiveTexture(GL_TEXTURE0);
		glUniform4f(lit.material, 1.0f, mesh.specular, mesh.emissive, 0.0f);
	}
}

void Renderer::DrawMesh(int id)
{
	if (id < 0 || id >= (int)meshes.size() || meshes[id].count == 0) return;

	BindMesh(id);
	if (meshes[id].asset)
		glDrawElements(GL_TRIANGLES, meshes[id].count, meshes[id].index32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (void*)0);
	else
		glDrawArrays(GL_TRIANGLES, 0, meshes[id].count);
	++drawCalls;

	for (GLuint loc = 0; loc <= 3; ++loc)
		glDisableVertexAttribArray(loc);
	for (GLuint loc = 11; loc <= 14; ++loc)
		glDisableVertexAttribArray(loc);
}

void Renderer::DrawModel(int id, const Mat4& model, const DrawParams& params)
{
	if (id < 0 || id >= (int)meshes.size() || meshes[id].count == 0) return;

	Instance instance;
	for (int i = 0; i < 16; ++i) instance.model[i] = model.m[i];
	instance.tint[0] = params.tint.x;
	instance.tint[1] = params.tint.y;
	instance.tint[2] = params.tint.z;
	instance.emissive = params.emissive;
	instance.phase = params.phase;
	instance.flash = params.flash;
	instance.dissolve = params.dissolve;
	instance.cutaway = ModelOccluder(id).radius > 0.0f ? 1.0f : 0.0f;
	instance.boneBase = -1.0f;
	instance.moss = params.moss;
	instance.spare[0] = instance.spare[1] = 0.0f;

	// A rigged model brings its pose: three rows of each bone's matrix go into this pass's bone buffer.
	if (params.skin != nullptr && meshes[id].skinned && (int)params.skin->size() == skeletons[id].BoneCount())
	{
		instance.boneBase = (float)(boneData.size() / 12);
		for (size_t b = 0; b < params.skin->size(); ++b)
		{
			const float* m = (*params.skin)[b].m;
			for (int row = 0; row < 3; ++row)
			{
				boneData.push_back(m[row]);
				boneData.push_back(m[4 + row]);
				boneData.push_back(m[8 + row]);
				boneData.push_back(m[12 + row]);
			}
		}
	}
	modelQueue[id].push_back(instance);
}

void Renderer::DrawModel(int id, const Vec3& pos, float yaw, const Vec3& scale, const DrawParams& params)
{
	Mat4 model = Mul(Mul(MatTranslate(pos), MatRotateY(yaw)), MatScale(scale));
	DrawModel(id, model, params);
}

void Renderer::DrawShadow(const Vec3& pos, float radius)
{
	if (shadowPass || MODEL_SHADOW >= (int)meshes.size()) return;

	Mat4 model = Mul(MatTranslate(Vec3(pos.x, 0.0f, pos.z)), MatScale(Vec3(radius * 2.0f, 1.0f, radius * 2.0f)));

	Instance instance = {};
	for (int i = 0; i < 16; ++i) instance.model[i] = model.m[i];
	instance.tint[0] = instance.tint[1] = instance.tint[2] = 1.0f;
	instance.boneBase = -1.0f;
	discQueue.push_back(instance);
}

void Renderer::DrawDecal(const Vec3& pos, float yaw, float sizeX, float sizeZ, const Vec3& color, float strength, float ring)
{
	if (shadowPass || MODEL_SHADOW >= (int)meshes.size() || strength <= 0.001f) return;

	Mat4 model = Mul(Mul(MatTranslate(Vec3(pos.x, 0.0f, pos.z)), MatRotateY(yaw)), MatScale(Vec3(sizeX, 1.0f, sizeZ)));

	// Rides the blob shadow queue; a non-zero emissive tells Lit.fs it is a decal.
	Instance instance = {};
	for (int i = 0; i < 16; ++i) instance.model[i] = model.m[i];
	instance.tint[0] = color.x;
	instance.tint[1] = color.y;
	instance.tint[2] = color.z;
	instance.emissive = strength;
	instance.flash = ring;
	instance.boneBase = -1.0f;
	discQueue.push_back(instance);
}

void Renderer::FlushModels()
{
	// Pack every queued instance into one buffer; each mesh then draws its run in one call.
	instanceData.clear();
	std::vector<int>& first = instanceFirst;
	first.assign(meshes.size() + 1, 0);
	for (size_t id = 0; id < meshes.size(); ++id)
	{
		first[id] = (int)instanceData.size();
		instanceData.insert(instanceData.end(), modelQueue[id].begin(), modelQueue[id].end());
		modelQueue[id].clear();
	}
	first[meshes.size()] = (int)instanceData.size();
	int discFirst = (int)instanceData.size();
	instanceData.insert(instanceData.end(), discQueue.begin(), discQueue.end());
	discQueue.clear();

	if (instanceData.empty())
	{
		boneData.clear();
		return;
	}

	// This pass's poses go up once; every rigged instance reads its own rows from them.
	if (!boneData.empty())
	{
		glBindBuffer(GL_TEXTURE_BUFFER, boneBuffer);
		GLsizeiptr boneBytes = (GLsizeiptr)(boneData.size() * sizeof(float));
		if (boneBytes > boneCapacity) boneCapacity = boneBytes * 2;
		glBufferData(GL_TEXTURE_BUFFER, boneCapacity, NULL, GL_STREAM_DRAW);
		glBufferSubData(GL_TEXTURE_BUFFER, 0, boneBytes, &boneData[0]);
		glBindBuffer(GL_TEXTURE_BUFFER, 0);
		glActiveTexture(GL_TEXTURE0 + kUnitBones);
		glBindTexture(GL_TEXTURE_BUFFER, boneTex);
		glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, boneBuffer);
		glActiveTexture(GL_TEXTURE0);
		boneData.clear();
	}

	Mat4 identity;
	if (shadowPass)
	{
		glUseProgram(shadowShader);
		glUniformMatrix4fv(shadow.model, 1, GL_FALSE, identity.m);
	}
	else
	{
		BindLit(identity, 0);
	}

	glBindBuffer(GL_ARRAY_BUFFER, vboInstances);
	// Grow the buffer rarely and orphan it every flush: a fresh allocation per flush made the driver
	// reallocate, and writing into storage the GPU still reads would make it wait.
	GLsizeiptr bytes = (GLsizeiptr)(instanceData.size() * sizeof(Instance));
	if (bytes > instanceCapacity) instanceCapacity = bytes * 2;
	glBufferData(GL_ARRAY_BUFFER, instanceCapacity, NULL, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, &instanceData[0]);

	for (GLuint loc = 4; loc <= 10; ++loc)
	{
		glEnableVertexAttribArray(loc);
		glVertexAttribDivisor(loc, 1);
	}

	for (size_t id = 0; id < meshes.size(); ++id)
		DrawInstances((int)id, first[id], first[id + 1] - first[id]);

	// Blob shadows go last: blended over the ground and the models, writing no depth.
	if (discFirst < (int)instanceData.size())
	{
		glUniform1i(lit.mode, 3);
		glDepthMask(GL_FALSE);
		DrawInstances(MODEL_SHADOW, discFirst, (int)instanceData.size() - discFirst);
		glDepthMask(GL_TRUE);
	}

	for (GLuint loc = 4; loc <= 10; ++loc)
	{
		glVertexAttribDivisor(loc, 0);
		glDisableVertexAttribArray(loc);
	}
	for (GLuint loc = 0; loc <= 3; ++loc)
		glDisableVertexAttribArray(loc);
	for (GLuint loc = 11; loc <= 14; ++loc)
		glDisableVertexAttribArray(loc);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Renderer::DrawInstances(int mesh, int start, int count)
{
	if (count <= 0 || meshes[mesh].count == 0) return;

	BindMesh(mesh);

	const GLsizei stride = (GLsizei)sizeof(Instance);
	const size_t base = (size_t)start * sizeof(Instance);
	glBindBuffer(GL_ARRAY_BUFFER, vboInstances);
	for (int c = 0; c < 4; ++c)
		glVertexAttribPointer(4 + c, 4, GL_FLOAT, GL_FALSE, stride, (void*)(base + sizeof(float) * 4 * c));
	glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, stride, (void*)(base + sizeof(float) * 16));
	glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, (void*)(base + sizeof(float) * 20));
	glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, stride, (void*)(base + sizeof(float) * 24));

	const GpuMesh& m = meshes[mesh];
	if (m.asset)
		glDrawElementsInstanced(GL_TRIANGLES, m.count, m.index32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (void*)0, count);
	else
		glDrawArraysInstanced(GL_TRIANGLES, 0, m.count, count);
	++drawCalls;
}

void Renderer::DrawGround(const Vec3& center, float extent, const GroundParams& params)
{
	if (shadowPass) return;

	Mat4 model = Mul(MatTranslate(Vec3(center.x, 0.0f, center.z)), MatScale(Vec3(extent, 1.0f, extent)));
	BindLit(model, 1);

	glUniform1f(lit.stage, params.stage);
	glUniform4f(lit.neighborStage, params.neighborStage[0], params.neighborStage[1], params.neighborStage[2], params.neighborStage[3]);
	glUniform2f(lit.chunkCenter, center.x, center.z);
	glUniform1f(lit.chunkSize, params.chunkSize);
	glUniform3f(lit.damp, params.dampCenter.x, params.dampCenter.z, params.dampStrength);
	glUniform1f(lit.road, params.road);
	glUniform1f(lit.city, params.city);

	glUniform1f(lit.groundTex, groundAlbedo ? 1.0f : 0.0f);
	if (groundAlbedo)
	{
		const GLuint arrays[3] = { groundAlbedo, groundNormal, groundHeight };
		for (int i = 0; i < 3; ++i)
		{
			glActiveTexture(GL_TEXTURE0 + kUnitGroundAlbedo + i);
			glBindTexture(GL_TEXTURE_2D_ARRAY, arrays[i]);
		}
		glActiveTexture(GL_TEXTURE0);
	}

	DrawMesh(MODEL_GROUND);
	glUniform1f(lit.groundTex, 0.0f);
}

void Renderer::DrawWater(const Vec3& center, float sizeX, float sizeZ)
{
	if (shadowPass) return;

	Mat4 model = Mul(MatTranslate(center), MatScale(Vec3(sizeX, 1.0f, sizeZ)));
	BindLit(model, 2);
	DrawMesh(MODEL_GROUND);
}

void Renderer::DrawSpores(const Vec3& center, const Vec3& field, const Vec3& color, float densityScale, float size)
{
	DrawMotes(0, 0, (int)((float)sporeCount * settings.particles), center, field, color, densityScale, size);
}

void Renderer::DrawAmbientMotes(const Vec3& center)
{
	// Slices of the spore seed buffer; each style animates its own way in Particle.vs.
	DrawMotes(1, 0, kFireflyCount, center, kFireflyField, Vec3(0.85f, 1.00f, 0.45f), 1.0f, 0.09f);
	DrawMotes(2, 1000, 1400, center, Vec3(60.0f, 9.0f, 60.0f), Vec3(0.95f, 0.92f, 0.78f), 0.35f, 0.05f);
}

void Renderer::PlaceFireflyLights(const Vec3& center)
{
	glowCount = 0;
	glowsDirty = true;

	// Fireflies only come out after dusk, as in Particle.fs.
	float day = Saturatef(sinf((timeOfDay - 0.25f) * 2.0f * kPi));
	float night = 1.0f - SmoothStep(0.0f, 0.35f, day);
	if (night <= 0.01f) return;

	// The same motion and blink as style 1 in Particle.vs and Particle.fs, for the ones near the middle of the view.
	struct Glow
	{
		Vec3  pos;
		float strength;
		float score;
	};
	Glow best[kMaxGlows];
	int found = 0;
	for (int i = 0; i < kFireflyCount; ++i)
	{
		const float* s = &fireflySeeds[i * 6];
		const float* r = s + 3;

		float t = time * (0.25f + r[0] * 0.2f);
		float sx = s[0] + sinf(t + r[1] * 6.283f) * 0.012f + cosf(t * 0.7f + r[2] * 6.283f) * 0.008f;
		float sz = s[2] + cosf(t * 0.9f + r[1] * 6.283f) * 0.012f;
		sx -= floorf(sx);
		sz -= floorf(sz);
		float sy = 0.15f + 0.7f * s[1] + sinf(time * 0.8f + r[2] * 6.283f) * 0.08f;

		float dx = sx * kFireflyField.x - center.x + kFireflyField.x * 0.5f;
		float dz = sz * kFireflyField.z - center.z + kFireflyField.z * 0.5f;
		dx -= floorf(dx / kFireflyField.x) * kFireflyField.x + kFireflyField.x * 0.5f;
		dz -= floorf(dz / kFireflyField.z) * kFireflyField.z + kFireflyField.z * 0.5f;
		Vec3 pos(center.x + dx, sy * kFireflyField.y, center.z + dz);
		float dist = DistXZ(pos, fogOrigin);
		if (dist > 16.0f) continue;

		float blink = powf(Maxf(sinf(time * (0.9f + r[0] * 0.8f) + r[0] * 40.0f), 0.0f), 4.0f);
		float strength = blink * night;
		if (strength < 0.15f) continue;

		// Keep the brightest near ones; a short list, so a plain replace-the-weakest is enough.
		Glow g = { pos, strength, strength / (1.0f + dist * 0.15f) };
		if (found < kMaxGlows)
		{
			best[found++] = g;
			continue;
		}
		int weakest = 0;
		for (int k = 1; k < kMaxGlows; ++k)
		{
			if (best[k].score < best[weakest].score) weakest = k;
		}
		if (g.score > best[weakest].score) best[weakest] = g;
	}

	for (int i = 0; i < found; ++i)
	{
		float* p = &glowPos[i * 4];
		p[0] = best[i].pos.x;
		p[1] = best[i].pos.y;
		p[2] = best[i].pos.z;
		p[3] = best[i].strength;
	}
	glowCount = found;
}

void Renderer::DrawWeather(const Vec3& center)
{
	// Heavier weather draws more of its slice, so a drizzle is sparse rather than faint.
	float leaves = weather.autumn * (0.12f + 0.88f * weather.wind);
	DrawMotes(3, 0, (int)(2600.0f * weather.rain * settings.particles), center, Vec3(46.0f, 14.0f, 46.0f), Vec3(0.72f, 0.80f, 0.88f), Saturatef(weather.rain * 1.5f), 0.6f);
	DrawMotes(4, 2600, (int)(2400.0f * weather.snow * settings.particles), center, Vec3(50.0f, 14.0f, 50.0f), Vec3(0.95f, 0.97f, 1.00f), Saturatef(weather.snow * 1.5f), 0.16f);
	DrawMotes(5, 5000, (int)(700.0f * leaves * settings.particles), center, Vec3(50.0f, 8.0f, 50.0f), Vec3(1.00f, 1.00f, 1.00f), Saturatef(leaves * 2.0f), 0.32f);
}

void Renderer::DrawMotes(int style, int first, int count, const Vec3& center, const Vec3& field, const Vec3& color, float densityScale, float size)
{
	// Motes blend over the models and test against their depth.
	FlushModels();
	if (shadowPass || densityScale <= 0.001f) return;
	if (first + count > sporeCount) count = sporeCount - first;
	if (count <= 0) return;

	glUseProgram(particleShader);
	glUniform1i(particle.style, style);

	glUniformMatrix4fv(particle.viewProj, 1, GL_FALSE, viewProj.m);
	glUniform3f(particle.center, center.x, center.y, center.z);
	glUniform3f(particle.field, field.x, field.y, field.z);
	glUniform1f(particle.time, time);
	glUniform1f(particle.pixelsPerUnit, pixelsPerUnit);
	glUniform1f(particle.size, size);
	glUniform3f(particle.color, color.x, color.y, color.z);
	glUniform1f(particle.densityScale, densityScale);
	glUniform1f(particle.timeOfDay, timeOfDay);
	UploadWeather(particle.season, particle.weather);
	glUniform2f(particle.windDrift, weather.driftX, weather.driftZ);

	glBindBuffer(GL_ARRAY_BUFFER, vboSpores);
	glEnableVertexAttribArray(particle.seedAttrib);
	glVertexAttribPointer(particle.seedAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, 0);
	if (particle.randAttrib >= 0)
	{
		glEnableVertexAttribArray(particle.randAttrib);
		glVertexAttribPointer(particle.randAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
	}

	// Spores glow: additive, and they must not occlude one another. Rain, snow and leaves are lit, not glowing.
	glBlendFunc(GL_SRC_ALPHA, style >= 3 ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE);
	glDepthMask(GL_FALSE);

	glDrawArrays(GL_POINTS, first, count);
	++drawCalls;

	glDepthMask(GL_TRUE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisableVertexAttribArray(particle.seedAttrib);
	if (particle.randAttrib >= 0) glDisableVertexAttribArray(particle.randAttrib);
}

void Renderer::BeginUI()
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::EndUI()
{
	FlushText();
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}

void Renderer::DrawOverlayQuad(float rx, float ry, float rw, float rh, const OverlayParams& params)
{
	// Text queued so far belongs under this quad.
	FlushText();
	glUseProgram(overlayShader);

	glUniform4f(overlay.rect, rx, ry, rw, rh);
	glUniform1i(overlay.mode, params.mode);
	glUniform4f(overlay.color, params.color.x, params.color.y, params.color.z, params.alpha);
	glUniform3f(overlay.color2, params.color2.x, params.color2.y, params.color2.z);
	glUniform1f(overlay.fill, params.fill);
	glUniform1f(overlay.vignette, params.vignette);
	glUniform1f(overlay.haze, params.haze);
	glUniform3f(overlay.hazeColor, params.hazeColor.x, params.hazeColor.y, params.hazeColor.z);
	glUniform1f(overlay.time, time);
	glUniform1f(overlay.timeOfDay, timeOfDay);
	glUniform1f(overlay.sporeExposure, sporeExposure);
	UploadWeather(overlay.season, overlay.weather);

	glBindBuffer(GL_ARRAY_BUFFER, vboScreen);
	glEnableVertexAttribArray(overlay.positionAttrib);
	glVertexAttribPointer(overlay.positionAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	++drawCalls;

	glDisableVertexAttribArray(overlay.positionAttrib);
}

void Renderer::DrawPixelQuad(float x, float y, float w, float h, const OverlayParams& params)
{
	// Pixel coordinates with the origin at the top-left, converted to NDC.
	float ndcX = (x / (float)windowSizeX) * 2.0f - 1.0f;
	float ndcY = 1.0f - ((y + h) / (float)windowSizeY) * 2.0f;
	float ndcW = (w / (float)windowSizeX) * 2.0f;
	float ndcH = (h / (float)windowSizeY) * 2.0f;

	DrawOverlayQuad(ndcX, ndcY, ndcW, ndcH, params);
}

void Renderer::DrawRectPx(float x, float y, float w, float h, const Vec3& color, float alpha)
{
	OverlayParams params;
	params.color = color;
	params.alpha = alpha;
	DrawPixelQuad(x, y, w, h, params);
}

void Renderer::DrawBarPx(float x, float y, float w, float h, float fill, const Vec3& low, const Vec3& high)
{
	OverlayParams params;
	params.mode = 2;
	params.color = low;
	params.color2 = high;
	params.fill = fill;
	DrawPixelQuad(x, y, w, h, params);
}

void Renderer::DrawAtmosphere(float vignette, float haze, const Vec3& hazeColor)
{
	OverlayParams params;
	params.mode = 1;
	params.vignette = vignette;
	params.haze = haze;
	params.hazeColor = hazeColor;
	DrawOverlayQuad(-1.0f, -1.0f, 2.0f, 2.0f, params);
}

void Renderer::DrawFade(const Vec3& color, float alpha)
{
	if (alpha <= 0.001f) return;

	OverlayParams params;
	params.color = color;
	params.alpha = alpha;
	DrawOverlayQuad(-1.0f, -1.0f, 2.0f, 2.0f, params);
}

void Renderer::DrawTexts(int x, int y, const char* text, const Vec3& color, bool large)
{
	if (text == NULL || *text == 0) return;

	const int face = large ? 1 : 0;
	Font& font = fonts[face];
	if (!font.IsLoaded()) return;

	std::vector<TextVertex>& queue = textQueue[face];
	float pen = (float)x;
	while (*text != 0)
	{
		const Glyph& g = font.Get(NextCodepoint(text));

		// Whole-pixel corners keep the glyphs crisp.
		if (g.w > 0)
		{
			float x0 = floorf(pen + 0.5f) + (float)g.x0;
			float y0 = (float)(y + g.y0);
			float x1 = x0 + (float)g.w;
			float y1 = y0 + (float)g.h;

			TextVertex a = { x0, y0, g.u0, g.v0, color.x, color.y, color.z };
			TextVertex b = { x1, y0, g.u1, g.v0, color.x, color.y, color.z };
			TextVertex c = { x1, y1, g.u1, g.v1, color.x, color.y, color.z };
			TextVertex d = { x0, y1, g.u0, g.v1, color.x, color.y, color.z };
			queue.push_back(a);
			queue.push_back(b);
			queue.push_back(c);
			queue.push_back(a);
			queue.push_back(c);
			queue.push_back(d);
		}

		pen += g.advance;
	}
}

int Renderer::TextWidth(const char* text, bool large)
{
	if (text == NULL) return 0;

	Font& font = fonts[large ? 1 : 0];
	if (!font.IsLoaded()) return 0;

	float width = 0.0f;
	while (*text != 0)
		width += font.Get(NextCodepoint(text)).advance;
	return (int)ceilf(width);
}

void Renderer::FlushText()
{
	if (textQueue[0].empty() && textQueue[1].empty()) return;

	glUseProgram(textShader);
	glUniform2f(textScreen, (float)windowSizeX, (float)windowSizeY);
	glUniform1i(textAtlas, 0);
	glActiveTexture(GL_TEXTURE0);

	const GLsizei stride = (GLsizei)sizeof(TextVertex);
	glBindBuffer(GL_ARRAY_BUFFER, vboText);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	for (int face = 0; face < 2; ++face)
	{
		std::vector<TextVertex>& queue = textQueue[face];
		if (queue.empty()) continue;

		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(queue.size() * sizeof(TextVertex)), &queue[0], GL_STREAM_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 2));
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 4));

		glBindTexture(GL_TEXTURE_2D, fonts[face].Texture());
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)queue.size());
		++drawCalls;
		queue.clear();
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
}

void Renderer::DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a)
{
	float newX, newY;

	GetGLPosition(x, y, &newX, &newY);

	FlushText();
	glUseProgram(solidRectShader);

	glUniform4f(glGetUniformLocation(solidRectShader, "u_Trans"), newX, newY, 0, size);
	glUniform4f(glGetUniformLocation(solidRectShader, "u_Color"), r, g, b, a);

	int attribPosition = glGetAttribLocation(solidRectShader, "a_Position");
	glEnableVertexAttribArray(attribPosition);
	glBindBuffer(GL_ARRAY_BUFFER, vboRect);
	glVertexAttribPointer(attribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	++drawCalls;

	glDisableVertexAttribArray(attribPosition);
}

void Renderer::GetGLPosition(float x, float y, float* newX, float* newY)
{
	*newX = x * 2.f / windowSizeX;
	*newY = y * 2.f / windowSizeY;
}
