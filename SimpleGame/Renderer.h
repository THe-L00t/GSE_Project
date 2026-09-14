#pragma once

#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Dependencies\glew.h"
#include "Math3D.h"

struct SceneEnv
{
	Vec3  sunDir{ 0.4f, 0.8f, 0.3f };         // direction TOWARD the sun
	Vec3  sunColor{ 0.9f, 0.9f, 0.85f };
	Vec3  skyColor{ 0.30f, 0.35f, 0.38f };    // ambient from above
	Vec3  groundColor{ 0.10f, 0.12f, 0.10f }; // ambient bounce
	Vec3  fogColor{ 0.55f, 0.60f, 0.62f };
	Vec3  fogOrigin;                          // fog distance is measured from here (the camera target)
	float fogDensity = 0.030f;
	float saturation = 0.80f;
};

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

	void BeginFrame(const Vec3& clearColor);
	void SetCamera(const Mat4& view, const Mat4& proj, const Vec3& eye, float pxPerUnit);
	void SetEnv(const SceneEnv& sceneEnv, float seconds);

	void DrawModel(int id, const Mat4& model, const DrawParams& params);
	void DrawModel(int id, const Vec3& pos, float yaw, const Vec3& scale, const DrawParams& params);
	void DrawShadow(const Vec3& pos, float radius);
	void DrawGround(const Vec3& center, float extent);
	void DrawWater(const Vec3& center, float sizeX, float sizeZ);
	void DrawSpores(const Vec3& center, const Vec3& field, const Vec3& color, float density, float size);

	void BeginUI();
	void EndUI();
	void DrawRectPx(float x, float y, float w, float h, const Vec3& color, float alpha);
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
		GLint sunDir = -1;
		GLint sunColor = -1;
		GLint skyColor = -1;
		GLint groundColor = -1;
		GLint fogColor = -1;
		GLint fogDensity = -1;
		GLint fogOrigin = -1;
		GLint saturation = -1;
		GLint time = -1;
		GLint camPos = -1;
	};

	void Initialize(int sizeX, int sizeY);
	bool ReadFile(const char* filename, std::string* target);
	void AddShader(GLuint program, const char* shaderText, GLenum shaderType);
	GLuint CompileShaders(const char* filenameVS, const char* filenameFS);
	void CreateVertexBufferObjects();
	void CacheLitLocations();
	void LoadModels();
	void GetGLPosition(float x, float y, float* newX, float* newY);

	void BindLit(const Mat4& model, const DrawParams& params, int mode);
	void DrawMesh(int id);
	void DrawOverlayQuad(int mode, float rx, float ry, float rw, float rh,
						 const Vec3& color, float alpha,
						 float vignette, float haze, const Vec3& hazeColor);

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
	std::vector<GpuMesh> meshes;

	Mat4     viewProj;
	Vec3     camPos;
	float    pixelsPerUnit = 1.0f;
	SceneEnv env;
	float    time = 0.0f;
};
