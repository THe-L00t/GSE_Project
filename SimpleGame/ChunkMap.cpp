#include "stdafx.h"
#include "ChunkMap.h"

#include <cstdlib>

#include "Models.h"
#include "Random.h"
#include "Rpg.h"

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
		bool  modern;      // left by the old world: thins out as modernity falls
	};

	const PropKind kPropKinds[] =
	{
		{ MODEL_TREE, 1.6f, 0.25f, 0.85f, 1.30f, { 3.0f, 4.0f, 5.0f }, false },
		{ MODEL_PINE, 1.4f, 0.20f, 0.80f, 1.40f, { 1.0f, 3.0f, 4.0f }, false },
		{ MODEL_BUSH, 0.9f, 0.00f, 0.70f, 1.30f, { 3.0f, 3.0f, 3.0f }, false },
		{ MODEL_ROCK, 0.9f, 0.60f, 0.70f, 1.50f, { 2.0f, 2.0f, 2.0f }, false },
		{ MODEL_RUIN, 2.0f, 1.00f, 0.80f, 1.10f, { 2.0f, 1.0f, 0.0f }, true },
		{ MODEL_CAR,  2.2f, 1.30f, 1.00f, 1.00f, { 1.5f, 0.5f, 0.0f }, true },
		{ MODEL_POLE, 0.8f, 0.25f, 0.90f, 1.10f, { 1.5f, 0.8f, 0.2f }, true },
	};
	const int kPropKindCount = (int)(sizeof(kPropKinds) / sizeof(kPropKinds[0]));

	// The start of Route 32 keeps a clearing for the opening.
	const Vec3 kClearingCenter(0.0f, 0.0f, -3.0f);
	const float kClearingRadius = 9.0f;

	// A place a chunk's random layout must leave alone.
	struct KeepOut
	{
		Vec3  center;
		float radius = 0.0f;   // zero keeps nothing out
	};

	KeepOut LandmarkKeepOut(int cx, int cz)
	{
		KeepOut k;
		if (cx == 0 && cz == kBusStopChunkZ)
		{
			k.center = Vec3(RoadCenter(kBusStopZ) + 5.0f, 0.0f, kBusStopZ);
			k.radius = 5.0f;
		}
		else if (cx == kReservoirChunkX && cz == kReservoirChunkZ)
		{
			k.center = kReservoirCenter;
			k.radius = 8.5f;
		}
		else if (cx == 0 && cz == kTownChunkZ)
		{
			k.center = Vec3(RoadCenter(kTownGateZ), 0.0f, kTownGateZ + 3.0f);
			k.radius = 11.0f;
		}
		return k;
	}

	// Modernity: whole within three rings of the village, gone by ring twelve.
	float ModernityAt(int ring)
	{
		float m = 1.0f - (float)(ring - 3) / 9.0f;
		return m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m);
	}

	void AddFixedProp(Chunk& c, std::vector<float>& footprints, int model, const Vec3& pos, float yaw, float scale, float radius)
	{
		ChunkProp prop;
		prop.model = model;
		prop.pos = pos;
		prop.yaw = yaw;
		prop.scale = scale;
		prop.radius = radius;
		c.props.push_back(prop);
		footprints.push_back(radius > 0.0f ? radius + 1.0f : 1.0f);
	}

	void AddLandmarks(Chunk& c, std::vector<float>& footprints)
	{
		const float halfPi = kPi * 0.5f;

		if (c.cx == 0 && c.cz == kBusStopChunkZ)
		{
			// The bus shelter faces the road; a car died beside it long ago.
			AddFixedProp(c, footprints, MODEL_BUS_STOP, Vec3(RoadCenter(kBusStopZ) + 5.2f, 0.0f, kBusStopZ), -halfPi, 1.0f, 1.6f);
			AddFixedProp(c, footprints, MODEL_CAR, Vec3(RoadCenter(kBusStopZ + 6.0f) + 4.6f, 0.0f, kBusStopZ + 6.0f), 0.25f, 1.0f, 1.3f);
			AddFixedProp(c, footprints, MODEL_POLE, Vec3(RoadCenter(kBusStopZ - 5.0f) + 4.0f, 0.0f, kBusStopZ - 5.0f), 0.0f, 1.0f, 0.25f);
		}
		else if (c.cx == kReservoirChunkX && c.cz == kReservoirChunkZ)
		{
			// Reeds and stones around the hidden water; the water itself is placed by the level.
			AddFixedProp(c, footprints, MODEL_BUSH, kReservoirCenter + Vec3(-7.2f, 0.0f, 3.5f), 0.4f, 1.2f, 0.0f);
			AddFixedProp(c, footprints, MODEL_BUSH, kReservoirCenter + Vec3(6.8f, 0.0f, -4.0f), 1.3f, 1.0f, 0.0f);
			AddFixedProp(c, footprints, MODEL_ROCK, kReservoirCenter + Vec3(-6.5f, 0.0f, -4.4f), 2.0f, 1.1f, 0.6f);
			AddFixedProp(c, footprints, MODEL_PINE, kReservoirCenter + Vec3(7.5f, 0.0f, 5.5f), 0.0f, 1.3f, 0.25f);
		}
		else if (c.cx == 0 && c.cz == kTownChunkZ)
		{
			// Ginkgo Town's gate: two gold trees either side of the road and the old school wall.
			float road = RoadCenter(kTownGateZ);
			AddFixedProp(c, footprints, MODEL_GINKGO, Vec3(road - 6.5f, 0.0f, kTownGateZ), 0.0f, 1.0f, 0.35f);
			AddFixedProp(c, footprints, MODEL_GINKGO, Vec3(road + 6.5f, 0.0f, kTownGateZ + 1.0f), 1.2f, 1.1f, 0.35f);
			AddFixedProp(c, footprints, MODEL_RUIN, Vec3(road + 10.5f, 0.0f, kTownGateZ + 6.0f), halfPi, 1.1f, 1.0f);
			AddFixedProp(c, footprints, MODEL_SIGN, Vec3(road + 4.2f, 0.0f, kTownGateZ - 3.0f), 0.0f, 1.0f, 0.0f);
			AddFixedProp(c, footprints, MODEL_POLE, Vec3(road - 4.2f, 0.0f, kTownGateZ - 6.0f), 0.0f, 1.0f, 0.25f);
		}
	}

	struct GrowStep
	{
		int x;
		int z;
		int dir;
	};

	float KindWeight(const PropKind& kind, int stage, float modernity)
	{
		float scale = kind.modern ? modernity : 1.0f + 0.5f * (1.0f - modernity);
		return kind.weight[stage - 1] * scale;
	}

	const PropKind& PickPropKind(Rng& rng, int stage, float modernity)
	{
		float total = 0.0f;
		for (int i = 0; i < kPropKindCount; ++i)
			total += KindWeight(kPropKinds[i], stage, modernity);

		float roll = rng.Unit() * total;
		for (int i = 0; i < kPropKindCount; ++i)
		{
			roll -= KindWeight(kPropKinds[i], stage, modernity);
			if (roll < 0.0f) return kPropKinds[i];
		}
		return kPropKinds[0];
	}

	bool FindOpenSpot(Rng& rng, const Chunk& c, const std::vector<float>& footprints, float footprint,
					  bool avoidRoad, bool avoidClearing, Vec3& out)
	{
		Vec3 center = ChunkMap::ChunkCenter(c.cx, c.cz);
		KeepOut keep = LandmarkKeepOut(c.cx, c.cz);

		for (int attempt = 0; attempt < 8; ++attempt)
		{
			// Separate statements: argument evaluation order is unspecified, and the same seed
			// must give the same layout on every compiler and configuration.
			float offsetX = rng.Range(-0.5f, 0.5f);
			float offsetZ = rng.Range(-0.5f, 0.5f);
			Vec3 pos(center.x + offsetX * (kChunkSize - 2.0f), 0.0f, center.z + offsetZ * (kChunkSize - 2.0f));

			if (avoidRoad && fabsf(pos.x - RoadCenter(pos.z)) < 3.4f + footprint) continue;
			if (avoidClearing && DistXZ(pos, kClearingCenter) < kClearingRadius + footprint) continue;
			if (keep.radius > 0.0f && DistXZ(pos, keep.center) < keep.radius + footprint) continue;

			bool blocked = false;
			for (size_t j = 0; j < c.props.size() && !blocked; ++j)
				blocked = DistXZ(pos, c.props[j].pos) < footprint + footprints[j] + 0.6f;
			if (blocked) continue;

			out = pos;
			return true;
		}
		return false;
	}

	int32_t Quantise(float v)
	{
		return (int32_t)floorf(v * 100.0f);
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
			h = HashInt(h, Quantise(p.pos.x));
			h = HashInt(h, Quantise(p.pos.z));
			h = HashInt(h, Quantise(p.yaw));
			h = HashInt(h, Quantise(p.scale));
		}
		for (size_t i = 0; i < c.spawns.size(); ++i)
		{
			const ChunkSpawn& s = c.spawns[i];
			h = HashInt(h, s.enemyType);
			h = HashInt(h, s.level);
			h = HashInt(h, Quantise(s.pos.x));
			h = HashInt(h, Quantise(s.pos.z));
		}
		for (size_t i = 0; i < c.items.size(); ++i)
		{
			const ChunkItem& item = c.items[i];
			h = HashInt(h, item.itemType);
			h = HashInt(h, Quantise(item.pos.x));
			h = HashInt(h, Quantise(item.pos.z));
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

		int px = 0, pz = 0, dir = -1;
		bool hasParent = ParentOf(x, z, px, pz, dir);

		GrowStep step;
		step.x = x;
		step.z = z;
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
	c.modernity = ModernityAt(ring);

	std::vector<float> footprints;
	AddLandmarks(c, footprints);

	int count = 8 + rng.RangeInt(0, 6) + (c.stage - 1) * 3;
	for (int i = 0; i < count; ++i)
	{
		const PropKind& kind = PickPropKind(rng, c.stage, c.modernity);
		float scale = rng.Range(kind.minScale, kind.maxScale) * (c.stage == 3 ? 1.25f : 1.0f);
		float footprint = kind.spacing * scale;

		Vec3 pos;
		if (!FindOpenSpot(rng, c, footprints, footprint, true, origin, pos)) continue;

		ChunkProp prop;
		prop.model = kind.model;
		prop.pos = pos;
		prop.yaw = rng.Range(0.0f, 2.0f * kPi);
		prop.scale = scale;
		prop.radius = kind.collision * scale;
		c.props.push_back(prop);
		footprints.push_back(footprint);
	}

	// The opening chunk is scripted, and the first ring stays free of creatures so the guided
	// fights are not interrupted. Beyond that, creatures and finds are part of the seed.
	if (!origin)
	{
		int spawnCount = ring >= 2 ? rng.RangeInt(0, 1 + c.stage) : 0;
		for (int i = 0; i < spawnCount; ++i)
		{
			float roll = rng.Unit();
			int type = ENEMY_SPORE_MITE;
			if (ring >= 3 && roll < 0.25f) type = ENEMY_MOSS_BOAR;
			else if (roll < 0.55f) type = ENEMY_HUSK;

			ChunkSpawn spawn;
			if (!FindOpenSpot(rng, c, footprints, 1.2f, false, false, spawn.pos)) continue;
			spawn.enemyType = type;
			spawn.level = 1 + ring / 2 + (rng.Chance(0.3f) ? 1 : 0);
			c.spawns.push_back(spawn);
		}

		if (rng.Chance(0.55f))
		{
			// Relics grow rarer as the old world thins out.
			float roll = rng.Unit();
			ChunkItem item;
			item.itemType = roll < 0.6f ? ITEM_HERB : (roll < 1.0f - 0.1f * c.modernity ? ITEM_CLEAN_WATER : ITEM_RELIC);
			if (FindOpenSpot(rng, c, footprints, 0.6f, false, false, item.pos))
				c.items.push_back(item);
		}
	}

	c.hash = HashChunk(c);
	return c;
}
