#include "stdafx.h"
#include "Renderer.h"

#include "Dependencies\freeglut.h"

#include <cstring>
#include <climits>
#include <vector>

// NOTE: this file is deliberately ASCII-only. The original template carried
// CP949 comments that render as mojibake in UTF-8 tools.

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	Initialize(windowSizeX, windowSizeY);
}


Renderer::~Renderer()
{
}

void Renderer::Initialize(int windowSizeX, int windowSizeY)
{
	m_WindowSizeX = windowSizeX;
	m_WindowSizeY = windowSizeY;

	// A bound VAO is required on core profiles and harmless on compatibility ones.
	glGenVertexArrays(1, &m_VAO);
	glBindVertexArray(m_VAO);

	m_SolidRectShader = CompileShaders("Shaders/SolidRect.vs", "Shaders/SolidRect.fs");
	m_LitShader       = CompileShaders("Shaders/Lit.vs",       "Shaders/Lit.fs");
	m_ParticleShader  = CompileShaders("Shaders/Particle.vs",  "Shaders/Particle.fs");
	m_OverlayShader   = CompileShaders("Shaders/Overlay.vs",   "Shaders/Overlay.fs");

	CreateVertexBufferObjects();

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glEnable(GL_PROGRAM_POINT_SIZE);

	if (m_SolidRectShader > 0 && m_LitShader > 0 && m_ParticleShader > 0 &&
		m_OverlayShader > 0 && m_VBORect > 0 && m_VBOBox > 0)
	{
		m_Initialized = true;
	}
}

bool Renderer::IsInitialized()
{
	return m_Initialized;
}

void Renderer::Resize(int windowSizeX, int windowSizeY)
{
	m_WindowSizeX = windowSizeX;
	m_WindowSizeY = windowSizeY;
	glViewport(0, 0, windowSizeX, windowSizeY);
}

void Renderer::CreateVertexBufferObjects()
{
	// --- template rect ---
	float rect[] =
	{
		-1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f, -1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f, 1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f,
		-1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f,  1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f, 1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f,
	};

	glGenBuffers(1, &m_VBORect);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_STATIC_DRAW);

	// --- unit cube: x/z in [-0.5, 0.5], y in [0, 1] so props sit on the ground ---
	const float h = 0.5f;
	float box[] =
	{
		// +Z
		-h, 0.f,  h, 0.f, 0.f, 1.f,   h, 0.f,  h, 0.f, 0.f, 1.f,   h, 1.f,  h, 0.f, 0.f, 1.f,
		-h, 0.f,  h, 0.f, 0.f, 1.f,   h, 1.f,  h, 0.f, 0.f, 1.f,  -h, 1.f,  h, 0.f, 0.f, 1.f,
		// -Z
		 h, 0.f, -h, 0.f, 0.f,-1.f,  -h, 0.f, -h, 0.f, 0.f,-1.f,  -h, 1.f, -h, 0.f, 0.f,-1.f,
		 h, 0.f, -h, 0.f, 0.f,-1.f,  -h, 1.f, -h, 0.f, 0.f,-1.f,   h, 1.f, -h, 0.f, 0.f,-1.f,
		// +X
		 h, 0.f,  h, 1.f, 0.f, 0.f,   h, 0.f, -h, 1.f, 0.f, 0.f,   h, 1.f, -h, 1.f, 0.f, 0.f,
		 h, 0.f,  h, 1.f, 0.f, 0.f,   h, 1.f, -h, 1.f, 0.f, 0.f,   h, 1.f,  h, 1.f, 0.f, 0.f,
		// -X
		-h, 0.f, -h,-1.f, 0.f, 0.f,  -h, 0.f,  h,-1.f, 0.f, 0.f,  -h, 1.f,  h,-1.f, 0.f, 0.f,
		-h, 0.f, -h,-1.f, 0.f, 0.f,  -h, 1.f,  h,-1.f, 0.f, 0.f,  -h, 1.f, -h,-1.f, 0.f, 0.f,
		// +Y
		-h, 1.f,  h, 0.f, 1.f, 0.f,   h, 1.f,  h, 0.f, 1.f, 0.f,   h, 1.f, -h, 0.f, 1.f, 0.f,
		-h, 1.f,  h, 0.f, 1.f, 0.f,   h, 1.f, -h, 0.f, 1.f, 0.f,  -h, 1.f, -h, 0.f, 1.f, 0.f,
		// -Y
		-h, 0.f, -h, 0.f,-1.f, 0.f,   h, 0.f, -h, 0.f,-1.f, 0.f,   h, 0.f,  h, 0.f,-1.f, 0.f,
		-h, 0.f, -h, 0.f,-1.f, 0.f,   h, 0.f,  h, 0.f,-1.f, 0.f,  -h, 0.f,  h, 0.f,-1.f, 0.f,
	};

	glGenBuffers(1, &m_VBOBox);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOBox);
	glBufferData(GL_ARRAY_BUFFER, sizeof(box), box, GL_STATIC_DRAW);

	// --- ground / water quad on the XZ plane ---
	float quad[] =
	{
		-h, 0.f, -h, 0.f, 1.f, 0.f,   h, 0.f, -h, 0.f, 1.f, 0.f,   h, 0.f,  h, 0.f, 1.f, 0.f,
		-h, 0.f, -h, 0.f, 1.f, 0.f,   h, 0.f,  h, 0.f, 1.f, 0.f,  -h, 0.f,  h, 0.f, 1.f, 0.f,
	};

	glGenBuffers(1, &m_VBOQuad);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

	// --- screen-space quad in [0,1]^2 ---
	float screen[] =
	{
		0.f, 0.f, 0.f,  1.f, 0.f, 0.f,  1.f, 1.f, 0.f,
		0.f, 0.f, 0.f,  1.f, 1.f, 0.f,  0.f, 1.f, 0.f,
	};

	glGenBuffers(1, &m_VBOScreen);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOScreen);
	glBufferData(GL_ARRAY_BUFFER, sizeof(screen), screen, GL_STATIC_DRAW);

	// --- spore field: static seeds, animated in the vertex shader ---
	m_SporeCount = 7000;
	std::vector<float> spores;
	spores.reserve(m_SporeCount * 6);
	srand(20260909);
	for (int i = 0; i < m_SporeCount; ++i)
	{
		spores.push_back(RandUnit());
		spores.push_back(RandUnit());
		spores.push_back(RandUnit());
		spores.push_back(RandUnit());
		spores.push_back(RandUnit());
		spores.push_back(RandUnit());
	}

	glGenBuffers(1, &m_VBOSpores);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOSpores);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(spores.size() * sizeof(float)), &spores[0], GL_STATIC_DRAW);
}

void Renderer::AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		fprintf(stderr, "Error creating shader type %d\n", ShaderType);
	}

	const GLchar* p[1];
	p[0] = pShaderText;
	GLint Lengths[1];

	size_t slen = strlen(pShaderText);
	if (slen > INT_MAX) {
		// Handle error
	}
	GLint len = (GLint)slen;

	Lengths[0] = len;
	glShaderSource(ShaderObj, 1, p, Lengths);
	glCompileShader(ShaderObj);

	GLint success;
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024];
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
	}

	glAttachShader(ShaderProgram, ShaderObj);
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
	GLuint ShaderProgram = glCreateProgram();

	if (ShaderProgram == 0) {
		fprintf(stderr, "Error creating shader program\n");
		return 0;
	}

	std::string vs, fs;

	if (!ReadFile(filenameVS, &vs)) {
		printf("Error compiling vertex shader\n");
		return 0;
	}

	if (!ReadFile(filenameFS, &fs)) {
		printf("Error compiling fragment shader\n");
		return 0;
	}

	AddShader(ShaderProgram, vs.c_str(), GL_VERTEX_SHADER);
	AddShader(ShaderProgram, fs.c_str(), GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { 0 };

	glLinkProgram(ShaderProgram);
	glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &Success);

	if (Success == 0) {
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		std::cout << filenameVS << ", " << filenameFS << " Error linking shader program\n" << ErrorLog;
		return 0;
	}

	glValidateProgram(ShaderProgram);
	glGetProgramiv(ShaderProgram, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		std::cout << filenameVS << ", " << filenameFS << " Error validating shader program\n" << ErrorLog;
		return 0;
	}

	std::cout << filenameVS << ", " << filenameFS << " shader compiled.\n";
	return ShaderProgram;
}

// ---------------------------------------------------------------- frame ----

void Renderer::BeginFrame(const Vec3& clearColor)
{
	glClearColor(clearColor.x, clearColor.y, clearColor.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::SetCamera(const Mat4& view, const Mat4& proj, const Vec3& camPos, float pixelsPerUnit)
{
	m_ViewProj = Mul(proj, view);
	m_CamPos = camPos;
	m_PixelsPerUnit = pixelsPerUnit;
}

void Renderer::SetEnv(const SceneEnv& env, float time)
{
	m_Env = env;
	m_Time = time;
}

void Renderer::BindLit(const Mat4& model, const Vec3& color, float emissive, int mode)
{
	glUseProgram(m_LitShader);

	float normalMat[9];
	MatNormal3x3(model, normalMat);

	glUniformMatrix4fv(glGetUniformLocation(m_LitShader, "u_ViewProj"), 1, GL_FALSE, m_ViewProj.m);
	glUniformMatrix4fv(glGetUniformLocation(m_LitShader, "u_Model"), 1, GL_FALSE, model.m);
	glUniformMatrix3fv(glGetUniformLocation(m_LitShader, "u_NormalMat"), 1, GL_FALSE, normalMat);

	glUniform3f(glGetUniformLocation(m_LitShader, "u_BaseColor"), color.x, color.y, color.z);
	glUniform1f(glGetUniformLocation(m_LitShader, "u_Emissive"), emissive);
	glUniform1i(glGetUniformLocation(m_LitShader, "u_Mode"), mode);

	Vec3 sd = Normalize(m_Env.sunDir);
	glUniform3f(glGetUniformLocation(m_LitShader, "u_SunDir"), sd.x, sd.y, sd.z);
	glUniform3f(glGetUniformLocation(m_LitShader, "u_SunColor"), m_Env.sunColor.x, m_Env.sunColor.y, m_Env.sunColor.z);
	glUniform3f(glGetUniformLocation(m_LitShader, "u_SkyColor"), m_Env.skyColor.x, m_Env.skyColor.y, m_Env.skyColor.z);
	glUniform3f(glGetUniformLocation(m_LitShader, "u_GroundColor"), m_Env.groundColor.x, m_Env.groundColor.y, m_Env.groundColor.z);
	glUniform3f(glGetUniformLocation(m_LitShader, "u_FogColor"), m_Env.fogColor.x, m_Env.fogColor.y, m_Env.fogColor.z);
	glUniform1f(glGetUniformLocation(m_LitShader, "u_FogDensity"), m_Env.fogDensity);
	glUniform3f(glGetUniformLocation(m_LitShader, "u_FogOrigin"), m_Env.fogOrigin.x, m_Env.fogOrigin.y, m_Env.fogOrigin.z);
	glUniform1f(glGetUniformLocation(m_LitShader, "u_Saturation"), m_Env.saturation);
	glUniform1f(glGetUniformLocation(m_LitShader, "u_Time"), m_Time);
	glUniform3f(glGetUniformLocation(m_LitShader, "u_CamPos"), m_CamPos.x, m_CamPos.y, m_CamPos.z);
}

void Renderer::DrawBox(const Mat4& model, const Vec3& color, float emissive)
{
	BindLit(model, color, emissive, 0);

	int posLoc = glGetAttribLocation(m_LitShader, "a_Position");
	int nrmLoc = glGetAttribLocation(m_LitShader, "a_Normal");

	glBindBuffer(GL_ARRAY_BUFFER, m_VBOBox);
	glEnableVertexAttribArray(posLoc);
	glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, 0);
	if (nrmLoc >= 0)
	{
		glEnableVertexAttribArray(nrmLoc);
		glVertexAttribPointer(nrmLoc, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
	}

	glDrawArrays(GL_TRIANGLES, 0, 36);

	glDisableVertexAttribArray(posLoc);
	if (nrmLoc >= 0) glDisableVertexAttribArray(nrmLoc);
}

void Renderer::DrawBox(const Vec3& pos, const Vec3& size, float yaw, const Vec3& color, float emissive)
{
	Mat4 model = Mul(Mul(MatTranslate(pos), MatRotateY(yaw)), MatScale(size));
	DrawBox(model, color, emissive);
}

void Renderer::DrawGround(const Vec3& center, float extent)
{
	Mat4 model = Mul(MatTranslate(Vec3(center.x, 0.0f, center.z)), MatScale(Vec3(extent, 1.0f, extent)));
	BindLit(model, Vec3(1.0f, 1.0f, 1.0f), 0.0f, 1);

	int posLoc = glGetAttribLocation(m_LitShader, "a_Position");
	int nrmLoc = glGetAttribLocation(m_LitShader, "a_Normal");

	glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
	glEnableVertexAttribArray(posLoc);
	glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, 0);
	if (nrmLoc >= 0)
	{
		glEnableVertexAttribArray(nrmLoc);
		glVertexAttribPointer(nrmLoc, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
	}

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(posLoc);
	if (nrmLoc >= 0) glDisableVertexAttribArray(nrmLoc);
}

void Renderer::DrawWater(const Vec3& center, float sizeX, float sizeZ)
{
	Mat4 model = Mul(MatTranslate(Vec3(center.x, center.y, center.z)), MatScale(Vec3(sizeX, 1.0f, sizeZ)));
	BindLit(model, Vec3(1.0f, 1.0f, 1.0f), 0.0f, 2);

	int posLoc = glGetAttribLocation(m_LitShader, "a_Position");
	int nrmLoc = glGetAttribLocation(m_LitShader, "a_Normal");

	glBindBuffer(GL_ARRAY_BUFFER, m_VBOQuad);
	glEnableVertexAttribArray(posLoc);
	glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, 0);
	if (nrmLoc >= 0)
	{
		glEnableVertexAttribArray(nrmLoc);
		glVertexAttribPointer(nrmLoc, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
	}

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(posLoc);
	if (nrmLoc >= 0) glDisableVertexAttribArray(nrmLoc);
}

void Renderer::DrawSpores(const Vec3& center, const Vec3& field, const Vec3& color, float density, float size)
{
	if (density <= 0.001f) return;

	glUseProgram(m_ParticleShader);

	glUniformMatrix4fv(glGetUniformLocation(m_ParticleShader, "u_ViewProj"), 1, GL_FALSE, m_ViewProj.m);
	glUniform3f(glGetUniformLocation(m_ParticleShader, "u_Center"), center.x, center.y, center.z);
	glUniform3f(glGetUniformLocation(m_ParticleShader, "u_Field"), field.x, field.y, field.z);
	glUniform1f(glGetUniformLocation(m_ParticleShader, "u_Time"), m_Time);
	glUniform1f(glGetUniformLocation(m_ParticleShader, "u_PixelsPerUnit"), m_PixelsPerUnit);
	glUniform1f(glGetUniformLocation(m_ParticleShader, "u_Size"), size);
	glUniform3f(glGetUniformLocation(m_ParticleShader, "u_Color"), color.x, color.y, color.z);
	glUniform1f(glGetUniformLocation(m_ParticleShader, "u_Density"), density);

	int seedLoc = glGetAttribLocation(m_ParticleShader, "a_Seed");
	int randLoc = glGetAttribLocation(m_ParticleShader, "a_Rand");

	glBindBuffer(GL_ARRAY_BUFFER, m_VBOSpores);
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

	glDrawArrays(GL_POINTS, 0, m_SporeCount);

	glDepthMask(GL_TRUE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisableVertexAttribArray(seedLoc);
	if (randLoc >= 0) glDisableVertexAttribArray(randLoc);
}

// ---------------------------------------------------------- screen space ----

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
	glUseProgram(m_OverlayShader);

	glUniform4f(glGetUniformLocation(m_OverlayShader, "u_Rect"), rx, ry, rw, rh);
	glUniform1i(glGetUniformLocation(m_OverlayShader, "u_Mode"), mode);
	glUniform4f(glGetUniformLocation(m_OverlayShader, "u_Color"), color.x, color.y, color.z, alpha);
	glUniform1f(glGetUniformLocation(m_OverlayShader, "u_Vignette"), vignette);
	glUniform1f(glGetUniformLocation(m_OverlayShader, "u_Haze"), haze);
	glUniform3f(glGetUniformLocation(m_OverlayShader, "u_HazeColor"), hazeColor.x, hazeColor.y, hazeColor.z);
	glUniform1f(glGetUniformLocation(m_OverlayShader, "u_Time"), m_Time);

	int posLoc = glGetAttribLocation(m_OverlayShader, "a_Position");
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOScreen);
	glEnableVertexAttribArray(posLoc);
	glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(posLoc);
}

void Renderer::DrawRectPx(float x, float y, float w, float h, const Vec3& color, float alpha)
{
	// Pixel coordinates with the origin at the top-left, converted to NDC.
	float ndcX = (x / (float)m_WindowSizeX) * 2.0f - 1.0f;
	float ndcY = 1.0f - ((y + h) / (float)m_WindowSizeY) * 2.0f;
	float ndcW = (w / (float)m_WindowSizeX) * 2.0f;
	float ndcH = (h / (float)m_WindowSizeY) * 2.0f;

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
	glOrtho(0.0, (double)m_WindowSizeX, 0.0, (double)m_WindowSizeY, -1.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	// The raster colour is latched when the raster position is set.
	glColor3f(color.x, color.y, color.z);
	glRasterPos2i(x, (int)m_WindowSizeY - y);

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

// ------------------------------------------------------ template leftover ----

void Renderer::DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a)
{
	float newX, newY;

	GetGLPosition(x, y, &newX, &newY);

	glUseProgram(m_SolidRectShader);

	glUniform4f(glGetUniformLocation(m_SolidRectShader, "u_Trans"), newX, newY, 0, size);
	glUniform4f(glGetUniformLocation(m_SolidRectShader, "u_Color"), r, g, b, a);

	int attribPosition = glGetAttribLocation(m_SolidRectShader, "a_Position");
	glEnableVertexAttribArray(attribPosition);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glVertexAttribPointer(attribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(attribPosition);
}

void Renderer::GetGLPosition(float x, float y, float* newX, float* newY)
{
	*newX = x * 2.f / m_WindowSizeX;
	*newY = y * 2.f / m_WindowSizeY;
}
