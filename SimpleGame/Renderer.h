#pragma once

#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Dependencies\glew.h"
#include "Math3D.h"

// Lighting and atmosphere for one frame. Driven by the time of day.
struct SceneEnv
{
	Vec3  sunDir;       // direction TOWARD the sun
	Vec3  sunColor;
	Vec3  skyColor;     // ambient from above
	Vec3  groundColor;  // ambient bounce
	Vec3  fogColor;
	Vec3  fogOrigin;    // fog distance is measured from here (the camera target)
	float fogDensity;
	float saturation;

	SceneEnv()
		: sunDir(0.4f, 0.8f, 0.3f)
		, sunColor(0.9f, 0.9f, 0.85f)
		, skyColor(0.30f, 0.35f, 0.38f)
		, groundColor(0.10f, 0.12f, 0.10f)
		, fogColor(0.55f, 0.60f, 0.62f)
		, fogOrigin(0.0f, 0.0f, 0.0f)
		, fogDensity(0.030f)
		, saturation(0.80f)
	{
	}
};

class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized();
	void Resize(int windowSizeX, int windowSizeY);
	int  GetWidth() const { return (int)m_WindowSizeX; }
	int  GetHeight() const { return (int)m_WindowSizeY; }

	// ---- frame setup ----
	void BeginFrame(const Vec3& clearColor);
	void SetCamera(const Mat4& view, const Mat4& proj, const Vec3& camPos, float pixelsPerUnit);
	void SetEnv(const SceneEnv& env, float time);

	// ---- world ----
	void DrawBox(const Mat4& model, const Vec3& color, float emissive);
	void DrawBox(const Vec3& pos, const Vec3& size, float yaw, const Vec3& color, float emissive);
	void DrawGround(const Vec3& center, float extent);
	void DrawWater(const Vec3& center, float sizeX, float sizeZ);
	void DrawSpores(const Vec3& center, const Vec3& field, const Vec3& color, float density, float size);

	// ---- screen space ----
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
	void Initialize(int windowSizeX, int windowSizeY);
	bool ReadFile(const char* filename, std::string* target);
	void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType);
	GLuint CompileShaders(const char* filenameVS, const char* filenameFS);
	void CreateVertexBufferObjects();
	void GetGLPosition(float x, float y, float* newX, float* newY);

	void BindLit(const Mat4& model, const Vec3& color, float emissive, int mode);
	void DrawOverlayQuad(int mode, float rx, float ry, float rw, float rh,
						 const Vec3& color, float alpha,
						 float vignette, float haze, const Vec3& hazeColor);

	bool m_Initialized = false;

	unsigned int m_WindowSizeX = 0;
	unsigned int m_WindowSizeY = 0;

	// Template geometry and shader.
	GLuint m_VBORect = 0;
	GLuint m_SolidRectShader = 0;

	// Prototype geometry.
	GLuint m_VAO = 0;
	GLuint m_VBOBox = 0;      // unit cube, x/z in [-0.5,0.5], y in [0,1]
	GLuint m_VBOQuad = 0;     // unit quad on the XZ plane
	GLuint m_VBOScreen = 0;   // unit quad in [0,1]^2 for screen space
	GLuint m_VBOSpores = 0;
	int    m_SporeCount = 0;

	GLuint m_LitShader = 0;
	GLuint m_ParticleShader = 0;
	GLuint m_OverlayShader = 0;

	// Per-frame state.
	Mat4  m_ViewProj;
	Vec3  m_CamPos;
	float m_PixelsPerUnit = 1.0f;
	SceneEnv m_Env;
	float m_Time = 0.0f;
};
