#pragma once

#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Dependencies\glew.h"
#include "Math3D.h"

struct DrawParams
{
	Vec3  tint{ 1.0f, 1.0f, 1.0f };
	float emissive = 0.0f;
	float phase = 0.0f;       // offsets shader animation so neighbours do not move in lockstep
	float flash = 0.0f;
};

class Renderer
{
public:
	Renderer(int sizeX, int sizeY);
	~Renderer();

	bool IsInitialized();
	void Resize(int sizeX, int sizeY);
	int  GetWidth() const { return (int)windowSizeX; }
	int  GetHeight() const { return (int)windowSizeY; }

	// Sun, palette, fog and spore density are evaluated in Shaders/Env.glsl from these values.
	void BeginFrame();
	void SetCamera(const Mat4& view, const Mat4& proj, const Vec3& eye, float pxPerUnit);
	void SetFrame(float dayTime, float exposure, const Vec3& fogCenter, float seconds);

	void DrawModel(int id, const Mat4& model, const DrawParams& params);
	void DrawModel(int id, const Vec3& pos, float yaw, const Vec3& scale, const DrawParams& params);
	void DrawShadow(const Vec3& pos, float radius);
	void DrawGround(const Vec3& center, float extent);
	void DrawWater(const Vec3& center, float sizeX, float sizeZ);
	void DrawSpores(const Vec3& center, const Vec3& field, const Vec3& color, float densityScale, float size);

	void BeginUI();
	void EndUI();
	void DrawRectPx(float x, float y, float w, float h, const Vec3& color, float alpha);
	void DrawBarPx(float x, float y, float w, float h, float fill, const Vec3& low, const Vec3& high);
	void DrawAtmosphere(float vignette, float haze, const Vec3& hazeColor);
	void DrawFade(const Vec3& color, float alpha);
	void DrawTexts(int x, int y, const char* text, const Vec3& color, bool large);
	int  TextWidth(const char* text, bool large);

	// Kept from the course template.
	void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);

private:
	struct GpuMesh
	{
		GLuint vbo = 0;
		int    count = 0;
	};

	struct LitLocations
	{
		GLint viewProj = -1;
		GLint model = -1;
		GLint normalMat = -1;
		GLint tint = -1;
		GLint emissive = -1;
		GLint phase = -1;
		GLint flash = -1;
		GLint mode = -1;
		GLint timeOfDay = -1;
		GLint sporeExposure = -1;
		GLint fogOrigin = -1;
		GLint time = -1;
		GLint camPos = -1;
	};

	struct OverlayLocations
	{
		GLint rect = -1;
		GLint mode = -1;
		GLint color = -1;
		GLint color2 = -1;
		GLint fill = -1;
		GLint vignette = -1;
		GLint haze = -1;
		GLint hazeColor = -1;
		GLint time = -1;
		GLint timeOfDay = -1;
		GLint sporeExposure = -1;
		GLint positionAttrib = -1;
	};

	struct ParticleLocations
	{
		GLint viewProj = -1;
		GLint center = -1;
		GLint field = -1;
		GLint time = -1;
		GLint pixelsPerUnit = -1;
		GLint size = -1;
		GLint color = -1;
		GLint densityScale = -1;
		GLint timeOfDay = -1;
		GLint seedAttrib = -1;
		GLint randAttrib = -1;
	};

	struct OverlayParams
	{
		int   mode = 0;
		Vec3  color;
		float alpha = 1.0f;
		Vec3  color2;
		float fill = 0.0f;
		float vignette = 0.0f;
		float haze = 0.0f;
		Vec3  hazeColor;
	};

	void Initialize(int sizeX, int sizeY);
	bool ReadFile(const char* filename, std::string* target);
	void AddShader(GLuint program, const char* shaderText, GLenum shaderType);
	GLuint CompileShaders(const char* filenameVS, const char* filenameFS);
	void CreateVertexBufferObjects();
	void CacheUniformLocations();
	void LoadModels();
	void GetGLPosition(float x, float y, float* newX, float* newY);

	void BindLit(const Mat4& model, const DrawParams& params, int mode);
	void DrawMesh(int id);
	void DrawPixelQuad(float x, float y, float w, float h, const OverlayParams& params);
	void DrawOverlayQuad(float rx, float ry, float rw, float rh, const OverlayParams& params);

	bool initialized = false;

	unsigned int windowSizeX = 0;
	unsigned int windowSizeY = 0;

	GLuint vboRect = 0;
	GLuint solidRectShader = 0;

	GLuint vao = 0;
	GLuint vboScreen = 0;   // unit quad in [0,1]^2 for screen space
	GLuint vboSpores = 0;
	int    sporeCount = 0;

	GLuint litShader = 0;
	GLuint particleShader = 0;
	GLuint overlayShader = 0;

	LitLocations         lit;
	OverlayLocations     overlay;
	ParticleLocations    particle;
	std::vector<GpuMesh> meshes;

	Mat4  viewProj;
	Vec3  camPos;
	Vec3  fogOrigin;
	float pixelsPerUnit = 1.0f;
	float timeOfDay = 0.0f;
	float sporeExposure = 0.0f;
	float time = 0.0f;
};
