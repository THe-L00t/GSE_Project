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
	SHAPE_PLANE,
	SHAPE_PRISM,      // triangular prism, ridge along x at the top, for pitched roofs
	SHAPE_POND        // flat oval with a wandering edge: the water's outline, PondRadius around its middle
};

// How far the water's edge reaches at an angle around its middle, as a share of the oval's radius there.
// Three slow waves bend the oval into a shore. Must match PondRadius in Shaders/Lit.fs and Shaders/Map.fs.
inline float PondRadius(float angle)
{
	return 1.0f + 0.10f * sinf(2.0f * angle + 0.8f) + 0.07f * sinf(3.0f * angle + 2.1f) + 0.04f * sinf(5.0f * angle + 4.0f);
}

inline float PondRadiusSlope(float angle)
{
	return 0.20f * cosf(2.0f * angle + 0.8f) + 0.21f * cosf(3.0f * angle + 2.1f) + 0.20f * cosf(5.0f * angle + 4.0f);
}

// Signed distance in metres from the edge of a pond reaching halfX by halfZ and turned by yaw (as MatRotateY
// turns it), for a point dx, dz from its middle: negative over the water. Measured along the line from the
// middle and tilted by the edge's slope, it stays within a few tenths of the true distance near the edge.
// Must match PondShore in Shaders/Lit.fs.
inline float PondShore(float dx, float dz, float halfX, float halfZ, float yaw)
{
	float c = cosf(yaw), s = sinf(yaw);
	float lx = c * dx - s * dz;
	float lz = s * dx + c * dz;
	float qx = lx / halfX, qz = lz / halfZ;
	float rho = sqrtf(qx * qx + qz * qz);
	if (rho < 1e-4f) return -(halfX < halfZ ? halfX : halfZ);
	float a = atan2f(qz, qx);
	float r = PondRadius(a);
	float slope = PondRadiusSlope(a) / r;
	float across = sqrtf(lx * lx + lz * lz) / rho;
	return across * (rho - r) / sqrtf(1.0f + slope * slope);
}

// The point on the edge of that pond in a world direction from its middle, then offset further out along it.
inline Vec3 PondEdge(const Vec3& center, float halfX, float halfZ, float yaw, float angle, float offset)
{
	float c = cosf(yaw), s = sinf(yaw);
	float wx = cosf(angle), wz = sinf(angle);
	float lx = c * wx - s * wz;
	float lz = s * wx + c * wz;
	float a = atan2f(lz / halfZ, lx / halfX);
	float r = PondRadius(a);
	float ex = halfX * r * cosf(a), ez = halfZ * r * sinf(a);
	Vec3 edge(center.x + c * ex + s * ez, center.y, center.z - s * ex + c * ez);
	return edge + Vec3(wx, 0.0f, wz) * offset;
}

// Bit flags animated in Shaders/Lit.vs.
enum AnimFlag
{
	ANIM_NONE = 0,
	ANIM_BREATHE = 1,
	ANIM_SWAY = 2,
	ANIM_PULSE = 4,
	ANIM_HOVER = 8,
	ANIM_SPIN = 16,
	ANIM_BOB = 32,    // walk bob driven by DrawParams::phase
	ANIM_FOLIAGE = 64 // leaves: they turn in autumn and pale in winter (Blender-made models only)
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
