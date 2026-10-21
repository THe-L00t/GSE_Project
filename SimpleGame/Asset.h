#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Math3D.h"

// One vertex of a Blender-made model as stored in Assets/Models/*.gsm (Art/gse/export.py writes it).
#pragma pack(push, 1)
struct AssetVertex
{
	float    px, py, pz;
	int16_t  normal[4];
	int16_t  tangent[4];   // w carries the bitangent's sign
	float    u, v;
	uint8_t  color[4];     // rgb tint; a is where moss settles first
	uint8_t  joints[4];
	uint8_t  weights[4];
	uint32_t anim;         // AnimFlag bits
};
#pragma pack(pop)
static_assert(sizeof(AssetVertex) == 52, "AssetVertex must match Art/gse/export.py");

struct Quat
{
	float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
};

struct BonePose
{
	Vec3 t;
	Quat q;
};

struct AnimClip
{
	std::string name;
	float fps = 30.0f;
	int   frameCount = 0;
	bool  loop = true;
	std::vector<BonePose> frames;   // frameCount rows of one pose per bone

	float Duration() const { return frameCount > 1 ? (float)(frameCount - 1) / fps : 0.0f; }
};

struct Skeleton
{
	std::vector<std::string> names;
	std::vector<int>  parents;       // parents come before their children
	std::vector<Mat4> inverseBind;
	std::vector<AnimClip> clips;

	int BoneCount() const { return (int)names.size(); }
	int FindBone(const char* name) const;
	int FindClip(const char* name) const;
};

struct AssetMesh
{
	std::vector<AssetVertex> vertices;
	std::vector<uint32_t> indices;
	Skeleton skeleton;
	std::string textures[3];   // albedo, normal, mask (R occlusion, G roughness, B glow)
	float specular = 0.25f;
	float emissive = 1.0f;
	Vec3  boundsMin;
	Vec3  boundsMax;
};

bool LoadAssetMesh(const std::string& path, AssetMesh& mesh);
bool LoadAssetClips(const std::string& path, Skeleton& skeleton);

// Plays a skeleton's clips and fades from the clip before for a moment, so changes never snap.
class Animator
{
public:
	void Bind(const Skeleton* s);
	bool Bound() const { return skeleton != nullptr; }
	const Skeleton* GetSkeleton() const { return skeleton; }

	// Keeps going if the clip is already playing, unless asked to start it over.
	void Play(const char* clip, float fade = 0.15f, bool restart = false);
	void Update(float dt);

	// Pins the playing clip to an outside timer, [0, 1] across the clip: a roll or a swing in step with the game.
	// dt still runs the fade from the clip before.
	void SetNormalizedTime(float t, float dt);
	float NormalizedTime() const;
	bool  Finished() const;
	bool  IsPlaying(const char* clip) const;
	void  SetSpeed(float s) { speed = s; }
	void  SetTime(float seconds);

	// Model-space skinning matrices, one per bone.
	const std::vector<Mat4>& SkinMatrices() const { return skin; }
	// Model-space transform of a bone, to hang things on it.
	Mat4 BoneMatrix(int bone) const;

private:
	void Sample(int clip, float t, std::vector<BonePose>& out) const;
	void AdvanceFade(float dt);
	void Evaluate();

	const Skeleton* skeleton = nullptr;
	int   current = -1;
	float time = 0.0f;
	int   previous = -1;
	float previousTime = 0.0f;
	float fade = 0.0f;
	float fadeLength = 0.0f;
	float speed = 1.0f;
	std::vector<BonePose> poseA;
	std::vector<BonePose> poseB;
	std::vector<BonePose> frozen;   // the pose on screen when a change cut into a fade
	std::vector<Mat4> global;
	std::vector<Mat4> skin;
};
