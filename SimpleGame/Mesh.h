#pragma once

#include <cstdint>
#include <vector>

#include "Math3D.h"

enum ShapeType
{
	SHAPE_BOX,
	SHAPE_CYLINDER,
	SHAPE_ELLIPSOID,
	SHAPE_CONE,
	SHAPE_DISC,
	SHAPE_PLANE
};

// Bit flags animated in Shaders/Lit.vs.
enum AnimFlag
{
	ANIM_NONE = 0,
	ANIM_BREATHE = 1,
	ANIM_SWAY = 2,
	ANIM_PULSE = 4,
	ANIM_HOVER = 8,
	ANIM_SPIN = 16,
	ANIM_BOB = 32     // walk bob driven by DrawParams::phase
};

struct MeshVertex
{
	float px, py, pz;
	float nx, ny, nz;
	float r, g, b;
	float anim;
};

// Unit shapes span [-0.5, 0.5]. Rotation applies roll (z), pitch (x), then yaw (y).
struct ShapePart
{
	int  shape = SHAPE_BOX;
	Vec3 center;
	Vec3 size{ 1.0f, 1.0f, 1.0f };
	Vec3 rotation;
	Vec3 color{ 1.0f, 1.0f, 1.0f };
	int  anim = ANIM_NONE;
};

class ModelRecipe
{
public:
	ModelRecipe& Add(int shape, const Vec3& center, const Vec3& size, const Vec3& color, int anim = ANIM_NONE);
	ModelRecipe& Rotated(const Vec3& radians);

	const std::vector<ShapePart>& Parts() const { return parts; }
	uint64_t Hash() const;

private:
	std::vector<ShapePart> parts;
};

struct MeshData
{
	std::vector<MeshVertex> vertices;
};

MeshData BuildMesh(const ModelRecipe& recipe);
bool SaveMeshCache(const char* path, uint64_t recipeHash, const MeshData& mesh);
bool LoadMeshCache(const char* path, uint64_t recipeHash, MeshData& mesh);
