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

struct GroundParams
{
	float stage = 0.0f;                                      // naturalisation: 0 bare village, 3 overgrown
	float neighborStage[4] = { 0.0f, 0.0f, 0.0f, 0.0f };     // -x, +x, -z, +z
	float chunkSize = 0.0f;                                  // zero turns neighbour blending off
	Vec3  dampCenter;
	float dampStrength = 0.0f;
	float road = 1.0f;                                       // how much of the painted road survives
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

	// glDraw* calls since the last take; bitmap text is not counted.
	int  TakeDrawCalls();

	// Projects a world position to pixels, top-left origin. False when it is behind the camera.
	bool WorldToScreen(const Vec3& pos, float& sx, float& sy) const;

	// Sun, palette, fog and spore density are evaluated in Shaders/Env.glsl from these values.
	// The scene renders into an HDR multisampled target; EndScene resolves it through
	// bloom and tone mapping to the window, after which the UI draws on top.
	void BeginFrame();
	void EndScene();

	// Between these, DrawModel writes depth into the sun's shadow map and every other draw is skipped.
	// Call after SetFrame, since the sun follows the time of day.
	void BeginShadowPass(const Vec3& focus);
	void EndShadowPass();
	void SetCamera(const Mat4& view, const Mat4& proj, const Vec3& eye, float pxPerUnit);
	void SetFrame(float dayTime, float exposure, const Vec3& fogCenter, float seconds);

	// Point lights for the lit shader; the first kMaxLights added each frame are kept.
	void ClearLights();
	void AddLight(const Vec3& pos, const Vec3& color, float radius);

	void DrawModel(int id, const Mat4& model, const DrawParams& params);
	void DrawModel(int id, const Vec3& pos, float yaw, const Vec3& scale, const DrawParams& params);
	void DrawShadow(const Vec3& pos, float radius);
	void DrawGround(const Vec3& center, float extent, const GroundParams& params);
	void DrawWater(const Vec3& center, float sizeX, float sizeZ);
	void DrawSpores(const Vec3& center, const Vec3& field, const Vec3& color, float densityScale, float size);
	void DrawAmbientMotes(const Vec3& center);   // fireflies by night, pollen by day

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
		GLint stage = -1;
		GLint neighborStage = -1;
		GLint chunkCenter = -1;
		GLint chunkSize = -1;
		GLint damp = -1;
		GLint road = -1;
		GLint lightCount = -1;
		GLint lightPos = -1;
		GLint lightColor = -1;
		GLint shadowMap = -1;
		GLint lightViewProj = -1;
		GLint shadowOn = -1;
	};

	struct ShadowLocations
	{
		GLint viewProj = -1;
		GLint model = -1;
		GLint normalMat = -1;
		GLint phase = -1;
		GLint time = -1;
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
		GLint style = -1;
		GLint seedAttrib = -1;
		GLint randAttrib = -1;
	};

	struct PostLocations
	{
		GLint mode = -1;
		GLint source = -1;
		GLint bloom = -1;
		GLint texel = -1;
		GLint direction = -1;
		GLint bloomStrength = -1;
		GLint sporeExposure = -1;
		GLint time = -1;
		GLint positionAttrib = -1;
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
	void DrawMotes(int style, int first, int count, const Vec3& center, const Vec3& field, const Vec3& color, float densityScale, float size);
	void DrawPixelQuad(float x, float y, float w, float h, const OverlayParams& params);
	void DrawOverlayQuad(float rx, float ry, float rw, float rh, const OverlayParams& params);

	void CreateTargets(int sizeX, int sizeY);
	void DeleteTargets();
	void BindSceneTarget();
	void CreateShadowMap();
	void DrawPostPass(GLuint target, int width, int height, int mode, GLuint source, float sourceWidth, float sourceHeight);

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
	GLuint postShader = 0;

	// Scene targets. sceneTex is the resolved HDR image; msFbo exists only when multisampling is available.
	GLuint msFbo = 0;
	GLuint msColor = 0;
	GLuint msDepth = 0;
	int    samples = 0;
	GLuint sceneFbo = 0;
	GLuint sceneTex = 0;
	GLuint sceneDepth = 0;
	GLuint bloomFbo[2] = { 0, 0 };
	GLuint bloomTex[2] = { 0, 0 };
	int    bloomWidth = 0;
	int    bloomHeight = 0;
	int    targetWidth = 0;
	int    targetHeight = 0;
	bool   postReady = false;

	static const int kShadowSize = 2048;
	GLuint shadowShader = 0;
	GLuint shadowFbo = 0;
	GLuint shadowTex = 0;
	bool   shadowReady = false;
	bool   shadowPass = false;     // inside BeginShadowPass / EndShadowPass
	bool   shadowActive = false;   // this frame's shadow map is valid
	Mat4   lightViewProj;

	LitLocations         lit;
	ShadowLocations      shadow;
	OverlayLocations     overlay;
	ParticleLocations    particle;
	PostLocations        post;
	std::vector<GpuMesh> meshes;

	Mat4  viewProj;
	Vec3  camPos;
	Vec3  fogOrigin;
	float pixelsPerUnit = 1.0f;
	float timeOfDay = 0.0f;
	float sporeExposure = 0.0f;
	float time = 0.0f;

	static const int kMaxLights = 8;   // matches kMaxLights in Lit.fs
	float lightPos[kMaxLights * 4] = {};
	float lightColor[kMaxLights * 3] = {};
	int   lightCount = 0;
	bool  lightsDirty = true;

	int drawCalls = 0;
};
