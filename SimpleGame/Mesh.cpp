#include "stdafx.h"
#include "Mesh.h"

#include <cstdio>
#include <cstring>

#include "Random.h"

namespace
{
	const uint32_t kMeshCacheVersion = 1;
	const char kMeshMagic[4] = { 'S', 'G', 'M', 'D' };

	const int kCircleSegments = 16;
	const int kSphereStacks = 8;
	const int kSphereSlices = 16;

	struct MeshCacheHeader
	{
		char     magic[4];
		uint32_t version;
		uint64_t recipeHash;
		uint32_t vertexCount;
		uint32_t reserved;
	};

	float SafeScale(float s)
	{
		if (s >= 0.0f && s < 1e-4f) return 1e-4f;
		if (s < 0.0f && s > -1e-4f) return -1e-4f;
		return s;
	}

	Vec3 RotateEuler(const Vec3& v, const Vec3& r)
	{
		float cz = cosf(r.z), sz = sinf(r.z);
		Vec3 a(v.x * cz - v.y * sz, v.x * sz + v.y * cz, v.z);

		float cx = cosf(r.x), sx = sinf(r.x);
		Vec3 b(a.x, a.y * cx - a.z * sx, a.y * sx + a.z * cx);

		float cy = cosf(r.y), sy = sinf(r.y);
		return Vec3(b.x * cy + b.z * sy, b.y, -b.x * sy + b.z * cy);
	}

	Vec3 CirclePoint(int i, int segments)
	{
		float a = (float)i / (float)segments * 2.0f * kPi;
		return Vec3(cosf(a), 0.0f, sinf(a));
	}

	Vec3 SpherePoint(float theta, float phi)
	{
		return Vec3(sinf(theta) * cosf(phi), cosf(theta), sinf(theta) * sinf(phi));
	}

	uint64_t HashVec3(uint64_t h, const Vec3& v)
	{
		h = HashFloat(h, v.x);
		h = HashFloat(h, v.y);
		return HashFloat(h, v.z);
	}

	class PartWriter
	{
	public:
		PartWriter(const ShapePart& p, std::vector<MeshVertex>& out)
			: part(p)
			, vertices(out)
		{
		}

		void Triangle(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& na, const Vec3& nb, const Vec3& nc)
		{
			Emit(a, na);
			Emit(b, nb);
			Emit(c, nc);
		}

	private:
		void Emit(const Vec3& local, const Vec3& normal)
		{
			Vec3 scaled(local.x * part.size.x, local.y * part.size.y, local.z * part.size.z);
			Vec3 p = RotateEuler(scaled, part.rotation) + part.center;

			// Normals take the inverse scale so ellipsoids and stretched boxes shade correctly.
			Vec3 inverseScaled(normal.x / SafeScale(part.size.x), normal.y / SafeScale(part.size.y), normal.z / SafeScale(part.size.z));
			Vec3 n = Normalize(RotateEuler(inverseScaled, part.rotation));

			MeshVertex v;
			v.px = p.x;
			v.py = p.y;
			v.pz = p.z;
			v.nx = n.x;
			v.ny = n.y;
			v.nz = n.z;
			v.r = part.color.x;
			v.g = part.color.y;
			v.b = part.color.z;
			v.anim = (float)part.anim;
			vertices.push_back(v);
		}

		const ShapePart& part;
		std::vector<MeshVertex>& vertices;
	};

	void AddBox(PartWriter& w)
	{
		const Vec3 normals[6] =
		{
			Vec3(1.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f),
			Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f),
			Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, 0.0f, -1.0f),
		};
		const Vec3 tangents[6] =
		{
			Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f),
			Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, 0.0f, 1.0f),
			Vec3(1.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f),
		};

		for (int f = 0; f < 6; ++f)
		{
			const Vec3& n = normals[f];
			Vec3 u = tangents[f] * 0.5f;
			Vec3 v = Cross(n, tangents[f]) * 0.5f;
			Vec3 c = n * 0.5f;

			Vec3 p00 = c - u - v;
			Vec3 p10 = c + u - v;
			Vec3 p11 = c + u + v;
			Vec3 p01 = c - u + v;
			w.Triangle(p00, p10, p11, n, n, n);
			w.Triangle(p00, p11, p01, n, n, n);
		}
	}

	void AddCylinder(PartWriter& w)
	{
		const Vec3 up(0.0f, 1.0f, 0.0f);
		const Vec3 down(0.0f, -1.0f, 0.0f);
		const Vec3 top(0.0f, 0.5f, 0.0f);
		const Vec3 bottom(0.0f, -0.5f, 0.0f);

		for (int i = 0; i < kCircleSegments; ++i)
		{
			Vec3 d0 = CirclePoint(i, kCircleSegments);
			Vec3 d1 = CirclePoint(i + 1, kCircleSegments);
			Vec3 b0 = d0 * 0.5f + bottom;
			Vec3 b1 = d1 * 0.5f + bottom;
			Vec3 t0 = d0 * 0.5f + top;
			Vec3 t1 = d1 * 0.5f + top;

			w.Triangle(b0, b1, t1, d0, d1, d1);
			w.Triangle(b0, t1, t0, d0, d1, d0);
			w.Triangle(top, t1, t0, up, up, up);
			w.Triangle(bottom, b0, b1, down, down, down);
		}
	}

	void AddCone(PartWriter& w)
	{
		const Vec3 down(0.0f, -1.0f, 0.0f);
		const Vec3 apex(0.0f, 0.5f, 0.0f);
		const Vec3 base(0.0f, -0.5f, 0.0f);

		for (int i = 0; i < kCircleSegments; ++i)
		{
			Vec3 d0 = CirclePoint(i, kCircleSegments);
			Vec3 d1 = CirclePoint(i + 1, kCircleSegments);
			Vec3 mid = Normalize(d0 + d1);
			Vec3 b0 = d0 * 0.5f + base;
			Vec3 b1 = d1 * 0.5f + base;

			// The side leans by radius / height = 0.5, which sets the normal's rise.
			Vec3 n0 = Normalize(Vec3(d0.x, 0.5f, d0.z));
			Vec3 n1 = Normalize(Vec3(d1.x, 0.5f, d1.z));
			Vec3 nm = Normalize(Vec3(mid.x, 0.5f, mid.z));

			w.Triangle(b0, b1, apex, n0, n1, nm);
			w.Triangle(base, b1, b0, down, down, down);
		}
	}

	void AddEllipsoid(PartWriter& w)
	{
		for (int s = 0; s < kSphereStacks; ++s)
		{
			float t0 = (float)s / (float)kSphereStacks * kPi;
			float t1 = (float)(s + 1) / (float)kSphereStacks * kPi;

			for (int i = 0; i < kSphereSlices; ++i)
			{
				float p0 = (float)i / (float)kSphereSlices * 2.0f * kPi;
				float p1 = (float)(i + 1) / (float)kSphereSlices * 2.0f * kPi;

				Vec3 n00 = SpherePoint(t0, p0);
				Vec3 n10 = SpherePoint(t1, p0);
				Vec3 n11 = SpherePoint(t1, p1);
				Vec3 n01 = SpherePoint(t0, p1);

				w.Triangle(n00 * 0.5f, n10 * 0.5f, n11 * 0.5f, n00, n10, n11);
				w.Triangle(n00 * 0.5f, n11 * 0.5f, n01 * 0.5f, n00, n11, n01);
			}
		}
	}

	void AddDisc(PartWriter& w)
	{
		const Vec3 up(0.0f, 1.0f, 0.0f);
		const Vec3 center(0.0f, 0.0f, 0.0f);

		for (int i = 0; i < kCircleSegments; ++i)
		{
			Vec3 e0 = CirclePoint(i, kCircleSegments) * 0.5f;
			Vec3 e1 = CirclePoint(i + 1, kCircleSegments) * 0.5f;
			w.Triangle(center, e1, e0, up, up, up);
		}
	}

	void AddPlane(PartWriter& w)
	{
		const Vec3 up(0.0f, 1.0f, 0.0f);
		Vec3 p00(-0.5f, 0.0f, -0.5f);
		Vec3 p10(0.5f, 0.0f, -0.5f);
		Vec3 p11(0.5f, 0.0f, 0.5f);
		Vec3 p01(-0.5f, 0.0f, 0.5f);
		w.Triangle(p00, p10, p11, up, up, up);
		w.Triangle(p00, p11, p01, up, up, up);
	}
}

ModelRecipe& ModelRecipe::Add(int shape, const Vec3& center, const Vec3& size, const Vec3& color, int anim)
{
	ShapePart part;
	part.shape = shape;
	part.center = center;
	part.size = size;
	part.color = color;
	part.anim = anim;
	parts.push_back(part);
	return *this;
}

ModelRecipe& ModelRecipe::Rotated(const Vec3& radians)
{
	if (!parts.empty()) parts.back().rotation = radians;
	return *this;
}

uint64_t ModelRecipe::Hash() const
{
	// Tessellation settings are part of the hash so changing them invalidates old caches.
	uint64_t h = kFnvOffset;
	h = HashInt(h, (int32_t)kMeshCacheVersion);
	h = HashInt(h, kCircleSegments);
	h = HashInt(h, kSphereStacks);
	h = HashInt(h, kSphereSlices);

	for (size_t i = 0; i < parts.size(); ++i)
	{
		const ShapePart& p = parts[i];
		h = HashInt(h, p.shape);
		h = HashVec3(h, p.center);
		h = HashVec3(h, p.size);
		h = HashVec3(h, p.rotation);
		h = HashVec3(h, p.color);
		h = HashInt(h, p.anim);
	}
	return h;
}

MeshData BuildMesh(const ModelRecipe& recipe)
{
	MeshData mesh;
	const std::vector<ShapePart>& parts = recipe.Parts();

	for (size_t i = 0; i < parts.size(); ++i)
	{
		PartWriter writer(parts[i], mesh.vertices);
		switch (parts[i].shape)
		{
		case SHAPE_BOX:       AddBox(writer); break;
		case SHAPE_CYLINDER:  AddCylinder(writer); break;
		case SHAPE_ELLIPSOID: AddEllipsoid(writer); break;
		case SHAPE_CONE:      AddCone(writer); break;
		case SHAPE_DISC:      AddDisc(writer); break;
		case SHAPE_PLANE:     AddPlane(writer); break;
		default:              AddBox(writer); break;
		}
	}
	return mesh;
}

bool SaveMeshCache(const char* path, uint64_t recipeHash, const MeshData& mesh)
{
	FILE* file = nullptr;
	if (fopen_s(&file, path, "wb") != 0 || file == nullptr) return false;

	MeshCacheHeader header;
	memcpy(header.magic, kMeshMagic, sizeof(header.magic));
	header.version = kMeshCacheVersion;
	header.recipeHash = recipeHash;
	header.vertexCount = (uint32_t)mesh.vertices.size();
	header.reserved = 0;

	bool ok = fwrite(&header, sizeof(header), 1, file) == 1;
	if (ok && !mesh.vertices.empty())
		ok = fwrite(&mesh.vertices[0], sizeof(MeshVertex), mesh.vertices.size(), file) == mesh.vertices.size();

	fclose(file);
	return ok;
}

bool LoadMeshCache(const char* path, uint64_t recipeHash, MeshData& mesh)
{
	FILE* file = nullptr;
	if (fopen_s(&file, path, "rb") != 0 || file == nullptr) return false;

	MeshCacheHeader header;
	bool ok = fread(&header, sizeof(header), 1, file) == 1
		&& memcmp(header.magic, kMeshMagic, sizeof(header.magic)) == 0
		&& header.version == kMeshCacheVersion
		&& header.recipeHash == recipeHash
		&& header.vertexCount > 0
		&& header.vertexCount < 4000000;

	if (ok)
	{
		mesh.vertices.resize(header.vertexCount);
		ok = fread(&mesh.vertices[0], sizeof(MeshVertex), header.vertexCount, file) == header.vertexCount;
	}

	fclose(file);
	if (!ok) mesh.vertices.clear();
	return ok;
}
