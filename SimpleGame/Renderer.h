#pragma once

#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Dependencies\glew.h"
#include "Asset.h"
#include "Font.h"
#include "Math3D.h"

struct DrawParams
{
	Vec3  tint{ 1.0f, 1.0f, 1.0f };
	float emissive = 0.0f;
	float phase = 0.0f;       // offsets shader animation so neighbours do not move in lockstep
	float flash = 0.0f;
	float dissolve = 0.0f;    // [0,1] how much of the model has come apart into light
	float moss = 0.0f;        // [0,1] how far moss has grown over a Blender-made model
	const std::vector<Mat4>* skin = nullptr;   // an Animator's skinning matrices, for rigged models
};

struct GroundParams
{
	float stage = 0.0f;                                      // naturalisation: 0 bare village, 3 overgrown
	float neighborStage[4] = { 0.0f, 0.0f, 0.0f, 0.0f };     // -x, +x, -z, +z
	float chunkSize = 0.0f;                                  // zero turns neighbour blending off
	Vec3  dampCenter;
	float dampStrength = 0.0f;
	float road = 1.0f;                                       // how much of the painted road survives
	float city = 0.0f;                                       // 1 where the old town's streets may show (Route 32)
};

// Season weights and live weather, read by Env.glsl as u_Season and u_Weather.
struct WeatherParams
{
	float summer = 0.0f;      // season weights; spring is what is left
	float autumn = 0.0f;
	float winter = 0.0f;
	float snowCover = 0.0f;
	float rain = 0.0f;
	float snow = 0.0f;
	float wind = 0.0f;
	float wetness = 0.0f;
	float driftX = 0.0f;      // wind travel integrated over time, in gale-seconds
	float driftZ = 0.0f;
};

// Full-screen moods, composited in Post.fs before tone mapping.
struct ScreenFx
{
	float dream = 0.0f;       // [0,1] a dream fragment playing: warm, faded, the edges melting
	float frost = 0.0f;       // [0,1] cold: a blue cast, frost creeping in from the edges
	float hunger = 0.0f;      // [0,1] hunger or thirst: colour drains, sight narrows
	Vec3  grade{ 1.0f, 1.0f, 1.0f };   // the region's colour cast
	float hurt = 0.0f;        // [0,1] red at the edges after a blow
	float tiltShift = 0.0f;   // [0,1] zoomed in: the view above and below the focus goes soft
	Vec3  focus;              // world point the tilt-shift keeps sharp
	float memo = 0.0f;        // [0,1] grandmother's map note: the scene sketched on paper
	float recall = 0.0f;      // [0,1] a memory: sepia and dim corners
	float ripple = 0.0f;      // (0,1) how far the level-up wave has spread from rippleAt; 0 none
	Vec3  rippleAt;
	Vec3  heat[2];            // lantern flames the air shimmers over
	float heatStrength[2] = { 0.0f, 0.0f };
};

// Graphics options; ApplySettings rebuilds only what a change touches.
struct RenderSettings
{
	float renderScale = 1.0f;      // scene resolution relative to the window
	int   msaa = 4;                // 0, 2, 4 or 8 samples
	int   shadowSize = 2048;       // 0 turns sun shadows off
	int   volumetric = 1;          // light shafts: 0 off, 1 low, 2 high
	float particles = 1.0f;        // share of spores, rain, snow and leaves drawn
	bool  ssao = true;             // corners and contacts darkened from the depth buffer
	bool  vsync = true;
};

// GPU milliseconds per stage, read back a couple of frames late so the queries never stall.
struct GpuTimes
{
	float shadow = 0.0f;
	float scene = 0.0f;
	float post = 0.0f;
	float ui = 0.0f;
};

class Renderer
{
public:
	Renderer(int sizeX, int sizeY);
	~Renderer();

	bool IsInitialized();

	void ApplySettings(const RenderSettings& next);
	const RenderSettings& Settings() const { return settings; }
	bool ShadowsEnabled() const { return shadowReady; }

	// Call once per frame, before swapping buffers.
	void EndFrame();
	const GpuTimes& LastGpuTimes() const { return gpuTimes; }
	void Resize(int sizeX, int sizeY);
	int  GetWidth() const { return (int)windowSizeX; }
	int  GetHeight() const { return (int)windowSizeY; }

	// glDraw* calls since the last take.
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
	void SetWeather(const WeatherParams& params) { weather = params; }
	void SetScreenFx(const ScreenFx& fx) { screenFx = fx; }

	// Point lights for the lit shader; the first kMaxLights added each frame are kept.
	void ClearLights();
	void AddLight(const Vec3& pos, const Vec3& color, float radius);

	// The hand torch is a spotlight of its own, lit in Lit.fs and scattered in the light-shaft pass.
	void SetTorch(bool on, const Vec3& pos, const Vec3& dir);

	// Trees, roofs and cars in front of the player are cut open in a cylinder along the view, ray-marched
	// in Lit.fs. Strength 0 turns it off.
	void SetCutaway(const Vec3& playerPos, float strength);

	// The skeleton and clips of a rigged Blender model; null for anything else.
	const Skeleton* ModelSkeleton(int id) const;

	// Models and blob shadows are queued and drawn one call per mesh when the pass ends.
	void DrawModel(int id, const Mat4& model, const DrawParams& params);
	void DrawModel(int id, const Vec3& pos, float yaw, const Vec3& scale, const DrawParams& params);
	void DrawShadow(const Vec3& pos, float radius);
	// Flat on the ground. A ring width above zero leaves only a band of that share of the radius at the rim.
	void DrawDecal(const Vec3& pos, float yaw, float sizeX, float sizeZ, const Vec3& color, float strength, float ring = 0.0f);
	void DrawGround(const Vec3& center, float extent, const GroundParams& params);
	void DrawWater(const Vec3& center, float sizeX, float sizeZ);
	void DrawSpores(const Vec3& center, const Vec3& field, const Vec3& color, float densityScale, float size);
	void DrawAmbientMotes(const Vec3& center);   // fireflies by night, pollen by day

	// Lights the ground under the fireflies DrawAmbientMotes will draw around the same centre.
	// Call before the scene, after SetFrame and SetCamera.
	void PlaceFireflyLights(const Vec3& center);
	void DrawWeather(const Vec3& center);        // rain, snow and leaves from the SetWeather values

	void BeginUI();
	void EndUI();
	void DrawRectPx(float x, float y, float w, float h, const Vec3& color, float alpha);
	void DrawBarPx(float x, float y, float w, float h, float fill, const Vec3& low, const Vec3& high);
	void DrawAtmosphere(float vignette, float haze, const Vec3& hazeColor);
	void DrawFade(const Vec3& color, float alpha);

	// UTF-8 text with y on the baseline. Consecutive strings share one draw until another UI draw.
	void DrawTexts(int x, int y, const char* text, const Vec3& color, bool large);
	int  TextWidth(const char* text, bool large);

	// Kept from the course template.
	void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);

private:
	// A recipe mesh is a flat list of MeshVertex; a Blender-made one is indexed AssetVertex with three textures.
	struct GpuMesh
	{
		GLuint vbo = 0;
		GLuint ebo = 0;
		int    count = 0;        // vertices for a recipe mesh, indices for an asset
		bool   asset = false;
		bool   index32 = false;
		bool   skinned = false;
		GLuint textures[3] = { 0, 0, 0 };   // albedo, normal, mask
		float  specular = 0.0f;
		float  emissive = 1.0f;
	};

	// Per-instance attributes at locations 4-10 in Lit.vs.
	struct Instance
	{
		float model[16];
		float tint[3];
		float emissive;
		float phase;
		float flash;
		float dissolve;
		float cutaway;    // 1 for models that open up when they hide the player
		float boneBase;   // first of this instance's matrices in the bone buffer, -1 when not skinned
		float moss;
		float spare[2];
	};

	struct LitLocations
	{
		GLint viewProj = -1;
		GLint model = -1;
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
		GLint city = -1;
		GLint lightCount = -1;
		GLint lightPos = -1;
		GLint lightColor = -1;
		GLint shadowMap = -1;
		GLint lightViewProj = -1;
		GLint shadowOn = -1;
		GLint season = -1;
		GLint weather = -1;
		GLint torchPos = -1;
		GLint torchDir = -1;
		GLint glowCount = -1;
		GLint glowPos = -1;
		GLint cutaway = -1;
		GLint material = -1;
		GLint groundTex = -1;
	};

	struct ShadowLocations
	{
		GLint viewProj = -1;
		GLint model = -1;
		GLint time = -1;
	};

	// Texture units the lit and shadow shaders read beyond the ones the passes already use.
	enum
	{
		kUnitAlbedo = 5,
		kUnitNormal = 6,
		kUnitMask = 7,
		kUnitGroundAlbedo = 8,
		kUnitGroundNormal = 9,
		kUnitGroundHeight = 10,
		kUnitBones = 11
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
		GLint season = -1;
		GLint weather = -1;
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
		GLint season = -1;
		GLint weather = -1;
		GLint windDrift = -1;
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
		GLint volume = -1;
		GLint volumeOn = -1;
		GLint dream = -1;
		GLint frost = -1;
		GLint hunger = -1;
		GLint grade = -1;
		GLint hurt = -1;
		GLint focus = -1;
		GLint tiltShift = -1;
		GLint memo = -1;
		GLint recall = -1;
		GLint ripple = -1;
		GLint heat = -1;
		GLint depth = -1;
		GLint ao = -1;
		GLint positionAttrib = -1;
	};

	struct VolumeLocations
	{
		GLint invViewProj = -1;
		GLint lightViewProj = -1;
		GLint shadowOn = -1;
		GLint steps = -1;
		GLint time = -1;
		GLint timeOfDay = -1;
		GLint sporeExposure = -1;
		GLint season = -1;
		GLint weather = -1;
		GLint torchPos = -1;
		GLint torchDir = -1;
		GLint positionAttrib = -1;
	};

	struct TextVertex
	{
		float x, y;
		float u, v;
		float r, g, b;
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
	bool LoadAssetModel(int id);
	void LoadGroundTextures();
	void GetGLPosition(float x, float y, float* newX, float* newY);
	bool ProjectUv(const Vec3& pos, float& u, float& v) const;   // texture coordinates, bottom-left origin

	void BindLit(const Mat4& model, int mode);
	void BindMesh(int id);
	void DrawMesh(int id);
	void FlushModels();
	void DrawInstances(int mesh, int start, int count);
	void DrawMotes(int style, int first, int count, const Vec3& center, const Vec3& field, const Vec3& color, float densityScale, float size);
	void UploadWeather(GLint seasonLoc, GLint weatherLoc) const;
	void DrawPixelQuad(float x, float y, float w, float h, const OverlayParams& params);
	void DrawOverlayQuad(float rx, float ry, float rw, float rh, const OverlayParams& params);
	void FlushText();

	void CreateTargets(int sizeX, int sizeY);
	void DeleteTargets();
	void BindSceneTarget();
	void CreateShadowMap();
	void DeleteShadowMap();
	void CreateNoiseTexture();
	void DrawPostPass(GLuint target, int width, int height, int mode, GLuint source, float sourceWidth, float sourceHeight);
	bool VolumeOn() const;
	bool AoOn() const;
	void DrawVolumePass();

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
	GLuint volumeShader = 0;
	GLuint textShader = 0;

	// Small and large faces; each queue holds glyph quads waiting for FlushText.
	Font   fonts[2];
	std::vector<TextVertex> textQueue[2];
	GLuint vboText = 0;
	GLint  textScreen = -1;
	GLint  textAtlas = -1;

	// Scene targets. sceneTex is the resolved HDR image; msFbo exists only when multisampling is available.
	GLuint msFbo = 0;
	GLuint msColor = 0;
	GLuint msDepth = 0;
	int    samples = 0;
	GLuint sceneFbo = 0;
	GLuint sceneTex = 0;
	GLuint sceneDepth = 0;      // a depth texture, read by the light-shaft pass
	GLuint bloomFbo[2] = { 0, 0 };
	GLuint bloomTex[2] = { 0, 0 };
	int    bloomWidth = 0;
	int    bloomHeight = 0;
	int    targetWidth = 0;
	int    targetHeight = 0;
	bool   postReady = false;

	RenderSettings settings;
	int    shadowSize = 0;         // size of the shadow map that exists now
	GLuint shadowShader = 0;
	GLuint shadowFbo = 0;
	GLuint shadowTex = 0;
	bool   shadowReady = false;
	bool   shadowPass = false;     // inside BeginShadowPass / EndShadowPass
	bool   shadowActive = false;   // this frame's shadow map is valid
	GLuint noiseTex = 0;
	Mat4   lightViewProj;

	LitLocations         lit;
	ShadowLocations      shadow;
	OverlayLocations     overlay;
	ParticleLocations    particle;
	PostLocations        post;
	VolumeLocations      volume;
	std::vector<GpuMesh> meshes;
	std::vector<Skeleton> skeletons;   // per model; empty for unrigged ones

	// Skinning matrices for this pass, three rows of four floats per bone, read by Lit.vs from a buffer texture.
	std::vector<float> boneData;
	GLuint boneBuffer = 0;
	GLuint boneTex = 0;
	GLsizeiptr boneCapacity = 0;

	// Ground layers made in Blender: grass, moss, dirt, concrete, forest floor.
	GLuint groundAlbedo = 0;
	GLuint groundNormal = 0;
	GLuint groundHeight = 0;

	// DrawModel and DrawShadow only queue; FlushModels draws each mesh once for all its instances.
	std::vector<std::vector<Instance>> modelQueue;
	std::vector<Instance> discQueue;
	std::vector<Instance> instanceData;
	std::vector<int>      instanceFirst;
	GLsizeiptr            instanceCapacity = 0;
	GLuint vboInstances = 0;

	Mat4  viewProj;
	float depthRange = 1.0f;   // world units across the depth buffer; the view is orthographic, so depth is linear
	Vec3  camPos;
	Vec3  fogOrigin;
	float pixelsPerUnit = 1.0f;
	float timeOfDay = 0.0f;
	float sporeExposure = 0.0f;
	float time = 0.0f;
	WeatherParams weather;
	ScreenFx screenFx;
	bool  torchOn = false;
	Vec3  torchPos;
	Vec3  torchDir{ 0.0f, 0.0f, 1.0f };
	Vec3  cutawayPos;
	float cutawayStrength = 0.0f;

	// Quarter-resolution light shafts, beside the bloom buffers; [1] is the blur scratch.
	GLuint volumeFbo[2] = { 0, 0 };
	GLuint volumeTex[2] = { 0, 0 };
	bool   volumeReady = false;

	static const int kMaxLights = 8;   // matches kMaxLights in Lit.fs
	float lightPos[kMaxLights * 4] = {};
	float lightColor[kMaxLights * 3] = {};
	int   lightCount = 0;
	bool  lightsDirty = true;

	// Fireflies: the first kFireflyCount seeds of the spore buffer, and the few lighting the ground this frame.
	static const int kFireflyCount = 260;
	static const int kMaxGlows = 12;   // matches kMaxGlows in Lit.fs
	std::vector<float> fireflySeeds;
	float glowPos[kMaxGlows * 4] = {};
	int   glowCount = 0;
	bool  glowsDirty = true;

	int drawCalls = 0;

	// Timestamps at frame start, after shadows, after the scene, after post and after the UI.
	enum GpuMark { MARK_BEGIN, MARK_SHADOW, MARK_SCENE, MARK_POST, MARK_UI, MARK_COUNT };
	static const int kQueryFrames = 3;
	void MarkGpu(int mark);
	GLuint   timeQueries[kQueryFrames][MARK_COUNT] = {};
	bool     queryPending[kQueryFrames] = {};
	int      queryFrame = 0;
	GpuTimes gpuTimes;
};
