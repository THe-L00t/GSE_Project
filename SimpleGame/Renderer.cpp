#include "stdafx.h"
#include "Renderer.h"

#include "Dependencies\freeglut.h"

#include <cstring>
#include <vector>

#include "Models.h"

// ASCII only: the original template's CP949 comments turned into mojibake in UTF-8 tools.

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
	shadowShader    = CompileShaders("Shaders/Lit.vs",       "Shaders/Shadow.fs");

	CreateVertexBufferObjects();
	CacheUniformLocations();
	LoadModels();
	CreateTargets(sizeX, sizeY);
	CreateShadowMap();

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

	if (sizeX != targetWidth || sizeY != targetHeight)
		CreateTargets(sizeX, sizeY);
}

void Renderer::CreateTargets(int sizeX, int sizeY)
{
	DeleteTargets();
	if (postShader == 0 || sizeX <= 0 || sizeY <= 0) return;

	targetWidth = sizeX;
	targetHeight = sizeY;

	glGenTextures(1, &sceneTex);
	glBindTexture(GL_TEXTURE_2D, sceneTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, sizeX, sizeY, 0, GL_RGBA, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenRenderbuffers(1, &sceneDepth);
	glBindRenderbuffer(GL_RENDERBUFFER, sceneDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, sizeX, sizeY);

	glGenFramebuffers(1, &sceneFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sceneDepth);
	bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

	// 4x MSAA smooths the long diagonal edges the quarter view is full of.
	GLint maxSamples = 0;
	glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
	samples = maxSamples < 4 ? (int)maxSamples : 4;
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
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	postReady = ok;
	std::cout << "Post: " << (postReady ? "HDR target ready" : "unavailable, drawing straight to the window")
		<< ", MSAA x" << (samples > 1 ? samples : 1) << "\n";
}

void Renderer::CreateShadowMap()
{
	if (shadowShader == 0 || litShader == 0) return;

	glGenTextures(1, &shadowTex);
	glBindTexture(GL_TEXTURE_2D, shadowTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kShadowSize, kShadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
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
	float texel = half * 2.0f / (float)kShadowSize;
	lx = floorf(lx / texel) * texel;
	ly = floorf(ly / texel) * texel;

	Mat4 proj = MatOrtho(lx - half, lx + half, ly - half, ly + half, -lz - depth, -lz + depth);
	lightViewProj = Mul(proj, view);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
	glViewport(0, 0, kShadowSize, kShadowSize);
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
	if (sceneDepth) glDeleteRenderbuffers(1, &sceneDepth);
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
}

void Renderer::CacheUniformLocations()
{
	lit.viewProj = glGetUniformLocation(litShader, "u_ViewProj");
	lit.model = glGetUniformLocation(litShader, "u_Model");
	lit.normalMat = glGetUniformLocation(litShader, "u_NormalMat");
	lit.tint = glGetUniformLocation(litShader, "u_Tint");
	lit.emissive = glGetUniformLocation(litShader, "u_Emissive");
	lit.phase = glGetUniformLocation(litShader, "u_Phase");
	lit.flash = glGetUniformLocation(litShader, "u_Flash");
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
	lit.lightCount = glGetUniformLocation(litShader, "u_LightCount");
	lit.lightPos = glGetUniformLocation(litShader, "u_LightPos");
	lit.lightColor = glGetUniformLocation(litShader, "u_LightColor");
	lit.shadowMap = glGetUniformLocation(litShader, "u_ShadowMap");
	lit.lightViewProj = glGetUniformLocation(litShader, "u_LightViewProj");
	lit.shadowOn = glGetUniformLocation(litShader, "u_ShadowOn");

	shadow.viewProj = glGetUniformLocation(shadowShader, "u_ViewProj");
	shadow.model = glGetUniformLocation(shadowShader, "u_Model");
	shadow.normalMat = glGetUniformLocation(shadowShader, "u_NormalMat");
	shadow.phase = glGetUniformLocation(shadowShader, "u_Phase");
	shadow.time = glGetUniformLocation(shadowShader, "u_Time");

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
}

void Renderer::LoadModels()
{
	std::vector<MeshData> meshData;
	int built = 0;
	LoadModelMeshes(meshData, built);

	meshes.resize(meshData.size());
	for (size_t i = 0; i < meshData.size(); ++i)
	{
		const std::vector<MeshVertex>& vertices = meshData[i].vertices;
		if (vertices.empty()) continue;

		glGenBuffers(1, &meshes[i].vbo);
		glBindBuffer(GL_ARRAY_BUFFER, meshes[i].vbo);
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertices.size() * sizeof(MeshVertex)), &vertices[0], GL_STATIC_DRAW);
		meshes[i].count = (int)vertices.size();
	}

	std::cout << "Models: " << ((int)meshData.size() - built) << " loaded from Cache/Models, " << built << " built.\n";
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

	glValidateProgram(program);
	glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(program, sizeof(errorLog), NULL, errorLog);
		std::cout << filenameVS << ", " << filenameFS << " Error validating shader program\n" << errorLog;
		return 0;
	}

	std::cout << filenameVS << ", " << filenameFS << " shader compiled.\n";
	return program;
}

void Renderer::BeginFrame()
{
	BindSceneTarget();
	shadowActive = false;

	// The ground plane always fills the orthographic view, so the clear colour never shows.
	glClearColor(0.05f, 0.06f, 0.07f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::EndScene()
{
	if (!postReady) return;

	if (samples > 1)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFbo);
		glBlitFramebuffer(0, 0, targetWidth, targetHeight, 0, 0, targetWidth, targetHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);

	glUseProgram(postShader);
	glUniform1i(post.source, 0);
	glUniform1i(post.bloom, 1);
	glUniform1f(post.bloomStrength, 0.65f);
	glUniform1f(post.sporeExposure, sporeExposure);
	glUniform1f(post.time, time);

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

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, bloomTex[0]);
	glActiveTexture(GL_TEXTURE0);
	DrawPostPass(0, (int)windowSizeX, (int)windowSizeY, 2, sceneTex, (float)targetWidth, (float)targetHeight);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisableVertexAttribArray(post.positionAttrib);

	glEnable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
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

int Renderer::TakeDrawCalls()
{
	int count = drawCalls;
	drawCalls = 0;
	return count;
}

void Renderer::SetCamera(const Mat4& view, const Mat4& proj, const Vec3& eye, float pxPerUnit)
{
	viewProj = Mul(proj, view);
	camPos = eye;
	pixelsPerUnit = pxPerUnit;
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

void Renderer::SetFrame(float dayTime, float exposure, const Vec3& fogCenter, float seconds)
{
	timeOfDay = dayTime;
	sporeExposure = exposure;
	fogOrigin = fogCenter;
	time = seconds;
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

void Renderer::BindLit(const Mat4& model, const DrawParams& params, int mode)
{
	glUseProgram(litShader);

	float normalMat[9];
	MatNormal3x3(model, normalMat);

	glUniformMatrix4fv(lit.viewProj, 1, GL_FALSE, viewProj.m);
	glUniformMatrix4fv(lit.model, 1, GL_FALSE, model.m);
	glUniformMatrix3fv(lit.normalMat, 1, GL_FALSE, normalMat);

	glUniform3f(lit.tint, params.tint.x, params.tint.y, params.tint.z);
	glUniform1f(lit.emissive, params.emissive);
	glUniform1f(lit.phase, params.phase);
	glUniform1f(lit.flash, params.flash);
	glUniform1i(lit.mode, mode);

	glUniform1f(lit.timeOfDay, timeOfDay);
	glUniform1f(lit.sporeExposure, sporeExposure);
	glUniform3f(lit.fogOrigin, fogOrigin.x, fogOrigin.y, fogOrigin.z);
	glUniform1f(lit.time, time);
	glUniform3f(lit.camPos, camPos.x, camPos.y, camPos.z);
	glUniformMatrix4fv(lit.lightViewProj, 1, GL_FALSE, lightViewProj.m);
	glUniform1f(lit.shadowOn, shadowActive ? 1.0f : 0.0f);

	// Uniforms stay with the program, so the light list goes up once per change.
	if (lightsDirty)
	{
		glUniform1i(lit.lightCount, lightCount);
		glUniform4fv(lit.lightPos, kMaxLights, lightPos);
		glUniform3fv(lit.lightColor, kMaxLights, lightColor);
		lightsDirty = false;
	}
}

void Renderer::DrawMesh(int id)
{
	if (id < 0 || id >= (int)meshes.size() || meshes[id].count == 0) return;

	// Attribute locations are fixed by layout qualifiers in Lit.vs.
	const GLsizei stride = (GLsizei)sizeof(MeshVertex);
	glBindBuffer(GL_ARRAY_BUFFER, meshes[id].vbo);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 3));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 6));
	glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(float) * 9));

	glDrawArrays(GL_TRIANGLES, 0, meshes[id].count);
	++drawCalls;

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	glDisableVertexAttribArray(3);
}

void Renderer::DrawModel(int id, const Mat4& model, const DrawParams& params)
{
	if (shadowPass)
	{
		float normalMat[9];
		MatNormal3x3(model, normalMat);
		glUniformMatrix4fv(shadow.model, 1, GL_FALSE, model.m);
		glUniformMatrix3fv(shadow.normalMat, 1, GL_FALSE, normalMat);
		glUniform1f(shadow.phase, params.phase);
		DrawMesh(id);
		return;
	}

	BindLit(model, params, 0);
	DrawMesh(id);
}

void Renderer::DrawModel(int id, const Vec3& pos, float yaw, const Vec3& scale, const DrawParams& params)
{
	Mat4 model = Mul(Mul(MatTranslate(pos), MatRotateY(yaw)), MatScale(scale));
	DrawModel(id, model, params);
}

void Renderer::DrawShadow(const Vec3& pos, float radius)
{
	if (shadowPass) return;

	Mat4 model = Mul(MatTranslate(Vec3(pos.x, 0.0f, pos.z)), MatScale(Vec3(radius * 2.0f, 1.0f, radius * 2.0f)));

	glDepthMask(GL_FALSE);
	BindLit(model, DrawParams(), 3);
	DrawMesh(MODEL_SHADOW);
	glDepthMask(GL_TRUE);
}

void Renderer::DrawGround(const Vec3& center, float extent, const GroundParams& params)
{
	if (shadowPass) return;

	Mat4 model = Mul(MatTranslate(Vec3(center.x, 0.0f, center.z)), MatScale(Vec3(extent, 1.0f, extent)));
	BindLit(model, DrawParams(), 1);

	glUniform1f(lit.stage, params.stage);
	glUniform4f(lit.neighborStage, params.neighborStage[0], params.neighborStage[1], params.neighborStage[2], params.neighborStage[3]);
	glUniform2f(lit.chunkCenter, center.x, center.z);
	glUniform1f(lit.chunkSize, params.chunkSize);
	glUniform3f(lit.damp, params.dampCenter.x, params.dampCenter.z, params.dampStrength);
	glUniform1f(lit.road, params.road);

	DrawMesh(MODEL_GROUND);
}

void Renderer::DrawWater(const Vec3& center, float sizeX, float sizeZ)
{
	if (shadowPass) return;

	Mat4 model = Mul(MatTranslate(center), MatScale(Vec3(sizeX, 1.0f, sizeZ)));
	BindLit(model, DrawParams(), 2);
	DrawMesh(MODEL_GROUND);
}

void Renderer::DrawSpores(const Vec3& center, const Vec3& field, const Vec3& color, float densityScale, float size)
{
	DrawMotes(0, 0, sporeCount, center, field, color, densityScale, size);
}

void Renderer::DrawAmbientMotes(const Vec3& center)
{
	// Slices of the spore seed buffer; each style animates its own way in Particle.vs.
	DrawMotes(1, 0, 260, center, Vec3(56.0f, 3.2f, 56.0f), Vec3(0.85f, 1.00f, 0.45f), 1.0f, 0.09f);
	DrawMotes(2, 1000, 1400, center, Vec3(60.0f, 9.0f, 60.0f), Vec3(0.95f, 0.92f, 0.78f), 0.35f, 0.05f);
}

void Renderer::DrawMotes(int style, int first, int count, const Vec3& center, const Vec3& field, const Vec3& color, float densityScale, float size)
{
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

	glBindBuffer(GL_ARRAY_BUFFER, vboSpores);
	glEnableVertexAttribArray(particle.seedAttrib);
	glVertexAttribPointer(particle.seedAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, 0);
	if (particle.randAttrib >= 0)
	{
		glEnableVertexAttribArray(particle.randAttrib);
		glVertexAttribPointer(particle.randAttrib, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
	}

	// Spores glow: additive, and they must not occlude one another.
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
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
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}

void Renderer::DrawOverlayQuad(float rx, float ry, float rw, float rh, const OverlayParams& params)
{
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

	void* font = large ? GLUT_BITMAP_HELVETICA_18 : GLUT_BITMAP_9_BY_15;

	// Bitmap text goes through fixed-function raster state, so the programmable
	// pipeline is unbound for the duration.
	glUseProgram(0);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0, (double)windowSizeX, 0.0, (double)windowSizeY, -1.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	// The raster colour is latched when the raster position is set.
	glColor3f(color.x, color.y, color.z);
	glRasterPos2i(x, (int)windowSizeY - y);

	for (const char* c = text; *c != 0; ++c)
		glutBitmapCharacter(font, *c);

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

int Renderer::TextWidth(const char* text, bool large)
{
	if (text == NULL) return 0;
	void* font = large ? GLUT_BITMAP_HELVETICA_18 : GLUT_BITMAP_9_BY_15;
	return glutBitmapLength(font, (const unsigned char*)text);
}

void Renderer::DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a)
{
	float newX, newY;

	GetGLPosition(x, y, &newX, &newY);

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
