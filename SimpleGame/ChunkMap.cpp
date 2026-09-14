#include "stdafx.h"
#include "ChunkMap.h"

#include <cstdlib>

#include "Models.h"
#include "Random.h"

namespace
{
	enum Direction
	{
		DIR_POS_X,
		DIR_NEG_X,
		DIR_POS_Z,
		DIR_NEG_Z
	};

	struct PropKind
	{
		int   model;
		float spacing;     // footprint that keeps props apart, at scale 1
		float collision;   // collision radius at scale 1, zero for decoration
		float minScale;
		float maxScale;
		float weight[3];   // by naturalisation stage 1..3
	};

	const PropKind kPropKinds[] =
	{
		{ MODEL_TREE, 1.6f, 0.25f, 0.85f, 1.30f, { 3.0f, 4.0f, 5.0f } },
		{ MODEL_PINE, 1.4f, 0.20f, 0.80f, 1.40f, { 1.0f, 3.0f, 4.0f } },
		{ MODEL_BUSH, 0.9f, 0.00f, 0.70f, 1.30f, { 3.0f, 3.0f, 3.0f } },
		{ MODEL_ROCK, 0.9f, 0.60f, 0.70f, 1.50f, { 2.0f, 2.0f, 2.0f } },
		{ MODEL_RUIN, 2.0f, 1.00f, 0.80f, 1.10f, { 2.0f, 1.0f, 0.0f } },
		{ MODEL_CAR,  2.2f, 1.30f, 1.00f, 1.00f, { 1.5f, 0.5f, 0.0f } },
	};
	const int kPropKindCount = (int)(sizeof(kPropKinds) / sizeof(kPropKinds[0]));

	// The start of Route 32 keeps a clearing for the opening.
	const Vec3 kClearingCenter(0.0f, 0.0f, -3.0f);
	const float kClearingRadius = 9.0f;

	struct GrowStep
	{
		int x;
		int z;
		int dir;
	};

	const PropKind& PickPropKind(Rng& rng, int stage)
	{
		float total = 0.0f;
		for (int i = 0; i < kPropKindCount; ++i)
			total += kPropKinds[i].weight[stage - 1];

		float roll = rng.Unit() * total;
		for (int i = 0; i < kPropKindCount; ++i)
		{
			roll -= kPropKinds[i].weight[stage - 1];
			if (roll < 0.0f) return kPropKinds[i];
		}
		return kPropKinds[0];
	}

	uint64_t HashChunk(const Chunk& c)
	{
		uint64_t h = kFnvOffset;
		h = HashInt(h, c.cx);
		h = HashInt(h, c.cz);
		h = HashInt(h, c.stage);

		// Quantised, so the hash describes the layout rather than float noise.
		for (size_t i = 0; i < c.props.size(); ++i)
		{
			const ChunkProp& p = c.props[i];
			h = HashInt(h, p.model);
			h = HashInt(h, (int32_t)floorf(p.pos.x * 100.0f));
			h = HashInt(h, (int32_t)floorf(p.pos.z * 100.0f));
			h = HashInt(h, (int32_t)floorf(p.yaw * 100.0f));
			h = HashInt(h, (int32_t)floorf(p.scale * 100.0f));
		}
		return h;
	}
}

void ChunkMap::Reset(uint64_t seed)
{
	worldSeed = seed;
	chunks.clear();
}

void ChunkMap::ChunkCoords(const Vec3& pos, int& cx, int& cz)
{
	cx = (int)floorf(pos.x / kChunkSize + 0.5f);
	cz = (int)floorf(pos.z / kChunkSize + 0.5f);
}

Vec3 ChunkMap::ChunkCenter(int cx, int cz)
{
	return Vec3((float)cx * kChunkSize, 0.0f, (float)cz * kChunkSize);
}

uint64_t ChunkMap::Key(int cx, int cz)
{
	return ((uint64_t)(uint32_t)cx << 32) | (uint64_t)(uint32_t)cz;
}

// Every chunk but the origin has exactly one parent, one step closer to the origin along its
// larger axis. That fixes each chunk's seed no matter which way the player walked in.
bool ChunkMap::ParentOf(int cx, int cz, int& px, int& pz, int& dir)
{
	if (cx == 0 && cz == 0) return false;

	px = cx;
	pz = cz;
	if (abs(cx) >= abs(cz))
	{
		px = cx > 0 ? cx - 1 : cx + 1;
		dir = cx > 0 ? DIR_POS_X : DIR_NEG_X;
	}
	else
	{
		pz = cz > 0 ? cz - 1 : cz + 1;
		dir = cz > 0 ? DIR_POS_Z : DIR_NEG_Z;
	}
	return true;
}

const Chunk& ChunkMap::Get(int cx, int cz)
{
	std::unordered_map<uint64_t, Chunk>::iterator found = chunks.find(Key(cx, cz));
	if (found != chunks.end()) return found->second;

	// Walk toward the origin until a chunk that already exists, then grow back out.
	std::vector<GrowStep> path;
	uint64_t parentHash = 0;
	int x = cx;
	int z = cz;
	for (;;)
	{
		found = chunks.find(Key(x, z));
		if (found != chunks.end())
		{
			parentHash = found->second.hash;
			break;
		}

		GrowStep step;
		step.x = x;
		step.z = z;
		step.dir = -1;

		int px = 0, pz = 0, dir = -1;
		bool hasParent = ParentOf(x, z, px, pz, dir);
		step.dir = dir;
		path.push_back(step);

		if (!hasParent) break;
		x = px;
		z = pz;
	}

	for (int i = (int)path.size() - 1; i >= 0; --i)
	{
		const GrowStep& step = path[i];

		// The origin grows from the world seed. Everything else grows from its parent's hash,
		// salted with the direction so the four neighbours of one chunk differ.
		uint64_t seed = step.dir < 0
			? MixSeed(worldSeed)
			: MixSeed(parentHash ^ ((uint64_t)(step.dir + 1) * 0x9E3779B97F4A7C15ULL));

		Chunk chunk = Generate(step.x, step.z, seed, step.dir);
		parentHash = chunk.hash;
		chunks[Key(step.x, step.z)] = chunk;
	}

	return chunks[Key(cx, cz)];
}

Chunk ChunkMap::Generate(int cx, int cz, uint64_t seed, int parentDir) const
{
	Chunk c;
	c.cx = cx;
	c.cz = cz;
	c.parentDir = parentDir;
	c.seed = seed;

	Rng rng(seed);
	bool origin = (cx == 0 && cz == 0);

	// Naturalisation deepens with distance from where the road left the village.
	int ring = abs(cx) > abs(cz) ? abs(cx) : abs(cz);
	c.stage = 1 + ring / 3 + (rng.Chance(0.3f) ? 1 : 0);
	if (origin) c.stage = 1;
	if (c.stage > 3) c.stage = 3;

	Vec3 center = ChunkCenter(cx, cz);
	std::vector<float> footprints;

	int count = 8 + rng.RangeInt(0, 6) + (c.stage - 1) * 3;
	for (int i = 0; i < count; ++i)
	{
		const PropKind& kind = PickPropKind(rng, c.stage);
		float scale = rng.Range(kind.minScale, kind.maxScale) * (c.stage == 3 ? 1.25f : 1.0f);
		float footprint = kind.spacing * scale;

		for (int attempt = 0; attempt < 8; ++attempt)
		{
			Vec3 pos(center.x + rng.Range(-0.5f, 0.5f) * (kChunkSize - 2.0f), 0.0f,
					 center.z + rng.Range(-0.5f, 0.5f) * (kChunkSize - 2.0f));

			if (fabsf(pos.x - RoadCenter(pos.z)) < 3.4f + footprint) continue;
			if (origin && DistXZ(pos, kClearingCenter) < kClearingRadius + footprint) continue;

			bool blocked = false;
			for (size_t j = 0; j < c.props.size() && !blocked; ++j)
				blocked = DistXZ(pos, c.props[j].pos) < footprint + footprints[j] + 0.6f;
			if (blocked) continue;

			ChunkProp prop;
			prop.model = kind.model;
			prop.pos = pos;
			prop.yaw = rng.Range(0.0f, 2.0f * kPi);
			prop.scale = scale;
			prop.radius = kind.collision * scale;
			c.props.push_back(prop);
			footprints.push_back(footprint);
			break;
		}
	}

	c.hash = HashChunk(c);
	return c;
}
