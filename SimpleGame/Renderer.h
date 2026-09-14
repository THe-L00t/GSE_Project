#pragma once

#include <string>
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

	void DrawBox(const Mat4& model, const Vec3& color, float emissive);
	void DrawBox(const Vec3& pos, const Vec3& size, float yaw, const Vec3& color, float emissive);
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
	void Initialize(int sizeX, int sizeY);
	bool ReadFile(const char* filename, std::string* target);
	void AddShader(GLuint program, const char* shaderText, GLenum shaderType);
	GLuint CompileShaders(const char* filenameVS, const char* filenameFS);
	void CreateVertexBufferObjects();
	void GetGLPosition(float x, float y, float* newX, float* newY);

	void BindLit(const Mat4& model, const Vec3& color, float emissive, int mode);
	void DrawOverlayQuad(int mode, float rx, float ry, float rw, float rh,
						 const Vec3& color, float alpha,
						 float vignette, float haze, const Vec3& hazeColor);

	bool initialized = false;

	unsigned int windowSizeX = 0;
	unsigned int windowSizeY = 0;

	GLuint vboRect = 0;
	GLuint solidRectShader = 0;

	GLuint vao = 0;
	GLuint vboBox = 0;      // unit cube, x/z in [-0.5,0.5], y in [0,1]
	GLuint vboQuad = 0;     // unit quad on the XZ plane
	GLuint vboScreen = 0;   // unit quad in [0,1]^2 for screen space
	GLuint vboSpores = 0;
	int    sporeCount = 0;

	GLuint litShader = 0;
	GLuint particleShader = 0;
	GLuint overlayShader = 0;

	Mat4     viewProj;
	Vec3     camPos;
	float    pixelsPerUnit = 1.0f;
	SceneEnv env;
	float    time = 0.0f;
};
