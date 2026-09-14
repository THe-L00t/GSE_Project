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

	CreateVertexBufferObjects();
	CacheLitLocations();
	LoadModels();

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

void Renderer::CacheLitLocations()
{
	lit.viewProj = glGetUniformLocation(litShader, "u_ViewProj");
	lit.model = glGetUniformLocation(litShader, "u_Model");
	lit.normalMat = glGetUniformLocation(litShader, "u_NormalMat");
	lit.tint = glGetUniformLocation(litShader, "u_Tint");
	lit.emissive = glGetUniformLocation(litShader, "u_Emissive");
	lit.phase = glGetUniformLocation(litShader, "u_Phase");
	lit.flash = glGetUniformLocation(litShader, "u_Flash");
	lit.mode = glGetUniformLocation(litShader, "u_Mode");
	lit.sunDir = glGetUniformLocation(litShader, "u_SunDir");
	lit.sunColor = glGetUniformLocation(litShader, "u_SunColor");
	lit.skyColor = glGetUniformLocation(litShader, "u_SkyColor");
	lit.groundColor = glGetUniformLocation(litShader, "u_GroundColor");
	lit.fogColor = glGetUniformLocation(litShader, "u_FogColor");
	lit.fogDensity = glGetUniformLocation(litShader, "u_FogDensity");
	lit.fogOrigin = glGetUniformLocation(litShader, "u_FogOrigin");
	lit.saturation = glGetUniformLocation(litShader, "u_Saturation");
	lit.time = glGetUniformLocation(litShader, "u_Time");
	lit.camPos = glGetUniformLocation(litShader, "u_CamPos");
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
			target->append(line.c_str());
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

void Renderer::BeginFrame(const Vec3& clearColor)
{
	glClearColor(clearColor.x, clearColor.y, clearColor.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::SetCamera(const Mat4& view, const Mat4& proj, const Vec3& eye, float pxPerUnit)
{
	viewProj = Mul(proj, view);
	camPos = eye;
	pixelsPerUnit = pxPerUnit;
}

void Renderer::SetEnv(const SceneEnv& sceneEnv, float seconds)
{
	env = sceneEnv;
	time = seconds;
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

	Vec3 sd = Normalize(env.sunDir);
	glUniform3f(lit.sunDir, sd.x, sd.y, sd.z);
	glUniform3f(lit.sunColor, env.sunColor.x, env.sunColor.y, env.sunColor.z);
	glUniform3f(lit.skyColor, env.skyColor.x, env.skyColor.y, env.skyColor.z);
	glUniform3f(lit.groundColor, env.groundColor.x, env.groundColor.y, env.groundColor.z);
	glUniform3f(lit.fogColor, env.fogColor.x, env.fogColor.y, env.fogColor.z);
	glUniform1f(lit.fogDensity, env.fogDensity);
	glUniform3f(lit.fogOrigin, env.fogOrigin.x, env.fogOrigin.y, env.fogOrigin.z);
	glUniform1f(lit.saturation, env.saturation);
	glUniform1f(lit.time, time);
	glUniform3f(lit.camPos, camPos.x, camPos.y, camPos.z);
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

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	glDisableVertexAttribArray(3);
}

void Renderer::DrawModel(int id, const Mat4& model, const DrawParams& params)
{
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
	Mat4 model = Mul(MatTranslate(Vec3(pos.x, 0.0f, pos.z)), MatScale(Vec3(radius * 2.0f, 1.0f, radius * 2.0f)));

	glDepthMask(GL_FALSE);
	BindLit(model, DrawParams(), 3);
	DrawMesh(MODEL_SHADOW);
	glDepthMask(GL_TRUE);
}

void Renderer::DrawGround(const Vec3& center, float extent)
{
	Mat4 model = Mul(MatTranslate(Vec3(center.x, 0.0f, center.z)), MatScale(Vec3(extent, 1.0f, extent)));
	BindLit(model, DrawParams(), 1);
	DrawMesh(MODEL_GROUND);
}

void Renderer::DrawWater(const Vec3& center, float sizeX, float sizeZ)
{
	Mat4 model = Mul(MatTranslate(center), MatScale(Vec3(sizeX, 1.0f, sizeZ)));
	BindLit(model, DrawParams(), 2);
	DrawMesh(MODEL_GROUND);
}

void Renderer::DrawSpores(const Vec3& center, const Vec3& field, const Vec3& color, float density, float size)
{
	if (density <= 0.001f) return;

	glUseProgram(particleShader);

	glUniformMatrix4fv(glGetUniformLocation(particleShader, "u_ViewProj"), 1, GL_FALSE, viewProj.m);
	glUniform3f(glGetUniformLocation(particleShader, "u_Center"), center.x, center.y, center.z);
	glUniform3f(glGetUniformLocation(particleShader, "u_Field"), field.x, field.y, field.z);
	glUniform1f(glGetUniformLocation(particleShader, "u_Time"), time);
	glUniform1f(glGetUniformLocation(particleShader, "u_PixelsPerUnit"), pixelsPerUnit);
	glUniform1f(glGetUniformLocation(particleShader, "u_Size"), size);
	glUniform3f(glGetUniformLocation(particleShader, "u_Color"), color.x, color.y, color.z);
	glUniform1f(glGetUniformLocation(particleShader, "u_Density"), density);

	int seedLoc = glGetAttribLocation(particleShader, "a_Seed");
	int randLoc = glGetAttribLocation(particleShader, "a_Rand");

	glBindBuffer(GL_ARRAY_BUFFER, vboSpores);
	glEnableVertexAttribArray(seedLoc);
	glVertexAttribPointer(seedLoc, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, 0);
	if (randLoc >= 0)
	{
		glEnableVertexAttribArray(randLoc);
		glVertexAttribPointer(randLoc, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
	}

	// Spores glow: additive, and they must not occlude one another.
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glDepthMask(GL_FALSE);

	glDrawArrays(GL_POINTS, 0, sporeCount);

	glDepthMask(GL_TRUE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisableVertexAttribArray(seedLoc);
	if (randLoc >= 0) glDisableVertexAttribArray(randLoc);
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

void Renderer::DrawOverlayQuad(int mode, float rx, float ry, float rw, float rh,
							   const Vec3& color, float alpha,
							   float vignette, float haze, const Vec3& hazeColor)
{
	glUseProgram(overlayShader);

	glUniform4f(glGetUniformLocation(overlayShader, "u_Rect"), rx, ry, rw, rh);
	glUniform1i(glGetUniformLocation(overlayShader, "u_Mode"), mode);
	glUniform4f(glGetUniformLocation(overlayShader, "u_Color"), color.x, color.y, color.z, alpha);
	glUniform1f(glGetUniformLocation(overlayShader, "u_Vignette"), vignette);
	glUniform1f(glGetUniformLocation(overlayShader, "u_Haze"), haze);
	glUniform3f(glGetUniformLocation(overlayShader, "u_HazeColor"), hazeColor.x, hazeColor.y, hazeColor.z);
	glUniform1f(glGetUniformLocation(overlayShader, "u_Time"), time);

	int posLoc = glGetAttribLocation(overlayShader, "a_Position");
	glBindBuffer(GL_ARRAY_BUFFER, vboScreen);
	glEnableVertexAttribArray(posLoc);
	glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(posLoc);
}

void Renderer::DrawRectPx(float x, float y, float w, float h, const Vec3& color, float alpha)
{
	// Pixel coordinates with the origin at the top-left, converted to NDC.
	float ndcX = (x / (float)windowSizeX) * 2.0f - 1.0f;
	float ndcY = 1.0f - ((y + h) / (float)windowSizeY) * 2.0f;
	float ndcW = (w / (float)windowSizeX) * 2.0f;
	float ndcH = (h / (float)windowSizeY) * 2.0f;

	DrawOverlayQuad(0, ndcX, ndcY, ndcW, ndcH, color, alpha, 0.0f, 0.0f, Vec3());
}

void Renderer::DrawAtmosphere(float vignette, float haze, const Vec3& hazeColor)
{
	DrawOverlayQuad(1, -1.0f, -1.0f, 2.0f, 2.0f, Vec3(), 1.0f, vignette, haze, hazeColor);
}

void Renderer::DrawFade(const Vec3& color, float alpha)
{
	if (alpha <= 0.001f) return;
	DrawOverlayQuad(0, -1.0f, -1.0f, 2.0f, 2.0f, color, alpha, 0.0f, 0.0f, Vec3());
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

	glDisableVertexAttribArray(attribPosition);
}

void Renderer::GetGLPosition(float x, float y, float* newX, float* newY)
{
	*newX = x * 2.f / windowSizeX;
	*newY = y * 2.f / windowSizeY;
}
