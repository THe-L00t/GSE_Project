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

	// What left a prop behind: the land itself, the old world anywhere, or the old world only out past the town.
	enum PropEra
	{
		ERA_WILD,
		ERA_OLD,
		ERA_OUTSKIRTS
	};

	struct PropKind
	{
		int   model;
		float spacing;     // footprint that keeps props apart, at scale 1
		float collision;   // collision radius at scale 1, zero for decoration
		float halfX;       // or a collision box at scale 1, along the model's own axes
		float halfZ;
		float minScale;
		float maxScale;
		float weight[3];   // by naturalisation stage 1..3
		int   era;
	};

	const PropKind kPropKinds[] =
	{
		{ MODEL_TREE, 1.6f, 0.25f, 0.0f, 0.0f, 0.85f, 1.30f, { 3.0f, 4.0f, 5.0f }, ERA_WILD },
		{ MODEL_PINE, 1.4f, 0.20f, 0.0f, 0.0f, 0.80f, 1.40f, { 1.0f, 3.0f, 4.0f }, ERA_WILD },
		{ MODEL_BUSH, 0.9f, 0.00f, 0.0f, 0.0f, 0.70f, 1.30f, { 3.0f, 3.0f, 3.0f }, ERA_WILD },
		{ MODEL_ROCK, 0.9f, 0.60f, 0.0f, 0.0f, 0.70f, 1.50f, { 2.0f, 2.0f, 2.0f }, ERA_WILD },
		{ MODEL_RUIN, 2.0f, 1.00f, 0.0f, 0.0f, 0.80f, 1.10f, { 2.0f, 1.0f, 0.0f }, ERA_OLD },
		{ MODEL_CAR,  2.2f, 1.30f, 0.0f, 0.0f, 1.00f, 1.00f, { 1.5f, 0.5f, 0.0f }, ERA_OLD },
		{ MODEL_POLE, 0.8f, 0.25f, 0.0f, 0.0f, 0.90f, 1.10f, { 1.5f, 0.8f, 0.2f }, ERA_OLD },
		{ MODEL_TILLER, 1.6f, 0.70f, 0.0f, 0.0f, 0.95f, 1.05f, { 0.8f, 0.3f, 0.0f }, ERA_OUTSKIRTS },
		{ MODEL_JANGDOK, 1.3f, 0.75f, 0.0f, 0.0f, 0.85f, 1.10f, { 0.5f, 0.2f, 0.0f }, ERA_OUTSKIRTS },
		{ MODEL_GATE, 2.2f, 0.40f, 0.0f, 0.0f, 0.95f, 1.00f, { 0.4f, 0.2f, 0.0f }, ERA_OUTSKIRTS },
		{ MODEL_GREENHOUSE, 3.4f, 0.00f, 0.0f, 0.0f, 0.90f, 1.05f, { 0.4f, 0.15f, 0.0f }, ERA_OUTSKIRTS },
		// Where the town has gone: the last of its houses, and street furniture that outlived the streets.
		{ MODEL_HOUSE_SITE, 4.6f, 0.00f, 3.5f, 2.75f, 0.95f, 1.05f, { 1.2f, 0.8f, 0.3f }, ERA_OUTSKIRTS },
		{ MODEL_EMPTY_HOUSE, 4.6f, 0.00f, 3.5f, 2.75f, 0.95f, 1.05f, { 0.7f, 0.4f, 0.1f }, ERA_OUTSKIRTS },
		{ MODEL_BLOCK_WALL, 2.2f, 0.00f, 2.0f, 0.15f, 0.95f, 1.05f, { 1.0f, 0.7f, 0.3f }, ERA_OUTSKIRTS },
		{ MODEL_STREETLAMP, 0.9f, 0.20f, 0.0f, 0.0f, 0.95f, 1.05f, { 0.8f, 0.5f, 0.2f }, ERA_OUTSKIRTS },
		{ MODEL_ROAD_SIGN, 1.6f, 0.00f, 1.1f, 0.2f, 0.95f, 1.05f, { 0.2f, 0.15f, 0.05f }, ERA_OUTSKIRTS },
	};
	const int kPropKindCount = (int)(sizeof(kPropKinds) / sizeof(kPropKinds[0]));

	// The start of Route 32 keeps a clearing for the opening. The town's buildings may come closer.
	const Vec3 kClearingCenter(0.0f, 0.0f, -3.0f);
	const float kClearingRadius = 9.0f;
	const float kLotClearingRadius = 5.5f;

	// The ground a placed thing keeps to itself: a circle, or a box turned by yaw. A box keeps the cosine and
	// sine of its turn: placing one chunk tests thousands of pairs, and the trigonometry was most of the cost.
	struct Footprint
	{
		Vec3  center;
		float radius = 0.0f;
		float halfX = 0.0f;
		float halfZ = 0.0f;
		float cosYaw = 1.0f;
		float sinYaw = 0.0f;

		bool IsBox() const { return halfX > 0.0f; }
	};

	Footprint Circle(const Vec3& center, float radius)
	{
		Footprint f;
		f.center = center;
		f.radius = radius;
		return f;
	}

	Footprint Box(const Vec3& center, float yaw, float halfX, float halfZ)
	{
		Footprint f;
		f.center = center;
		f.cosYaw = cosf(yaw);
		f.sinYaw = sinf(yaw);
		f.halfX = halfX;
		f.halfZ = halfZ;
		return f;
	}

	// Distance from a point to a box footprint, zero inside it.
	float BoxDistance(const Footprint& box, const Vec3& p)
	{
		float c = box.cosYaw;
		float s = box.sinYaw;
		float dx = p.x - box.center.x;
		float dz = p.z - box.center.z;
		float ex = Maxf(fabsf(dx * c - dz * s) - box.halfX, 0.0f);
		float ez = Maxf(fabsf(dx * s + dz * c) - box.halfZ, 0.0f);
		return sqrtf(ex * ex + ez * ez);
	}

	// Half the length of a box's shadow on a unit axis.
	float BoxExtent(const Footprint& box, float ax, float az)
	{
		float c = box.cosYaw;
		float s = box.sinYaw;
		return box.halfX * fabsf(c * ax - s * az) + box.halfZ * fabsf(s * ax + c * az);
	}

	bool Overlaps(const Footprint& a, const Footprint& b, float gap)
	{
		if (!a.IsBox() && !b.IsBox()) return DistXZ(a.center, b.center) < a.radius + b.radius + gap;
		if (!a.IsBox()) return BoxDistance(b, a.center) < a.radius + gap;
		if (!b.IsBox()) return BoxDistance(a, b.center) < b.radius + gap;

		// Two boxes are apart when an edge direction of either one separates them.
		const Footprint* boxes[2] = { &a, &b };
		for (int i = 0; i < 2; ++i)
		{
			float c = boxes[i]->cosYaw;
			float s = boxes[i]->sinYaw;
			const float axes[2][2] = { { c, -s }, { s, c } };
			for (int k = 0; k < 2; ++k)
			{
				float ax = axes[k][0];
				float az = axes[k][1];
				float d = fabsf((b.center.x - a.center.x) * ax + (b.center.z - a.center.z) * az);
				if (d > BoxExtent(a, ax, az) + BoxExtent(b, ax, az) + gap) return false;
			}
		}
		return true;
	}

	// Within reach of a cross street's line, pavements included.
	bool AtCrossStreet(float z, float margin)
	{
		int row = (int)floorf((z - kChunkSize * 0.5f) / kChunkSize + 0.5f);
		return fabsf(z - ((float)row * kChunkSize + kChunkSize * 0.5f)) < StreetSetback(row) + margin;
	}

	// On the road or, where the town still stands, on its streets and pavements.
	bool OnStreet(const Vec3& p, float margin)
	{
		float dx = fabsf(p.x - RoadCenter(p.z));
		if (dx < 3.4f + margin) return true;
		if (UrbanAt(p.x, p.z) < 0.25f) return false;
		return dx < kMainRoadHalf + kMainWalk + margin || AtCrossStreet(p.z, margin);
	}

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
			k.center = Vec3(RoadCenter(kBusStopZ) - 5.0f, 0.0f, kBusStopZ);
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
		else if (cx == 0 && cz == kOldTreeChunkZ)
		{
			k.center = OldTreePos();
			k.radius = 6.0f;
		}
		return k;
	}

	int RingOf(int cx, int cz)
	{
		return abs(cx) > abs(cz) ? abs(cx) : abs(cz);
	}

	// Every third chunk on a lattice is left to rest: no creature nests there. Each of the others touches one
	// along an edge, so a quiet stretch is never more than a chunk away. The world seed shifts the lattice.
	bool IsRestChunk(int cx, int cz, uint64_t worldSeed)
	{
		if (RingOf(cx, cz) < 2 || (cx == 0 && cz == kBusStopChunkZ)) return false;
		int v = (cx + 2 * cz + (int)(worldSeed % 3)) % 3;
		return v == 0;
	}

	// Modernity: whole within three rings of the village, gone by ring twelve.
	float ModernityAt(int ring)
	{
		float m = 1.0f - (float)(ring - 3) / 9.0f;
		return m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m);
	}

	// Clear of everything placed so far, and of the chunk's landmark.
	bool Fits(const Chunk& c, const std::vector<Footprint>& footprints, const Footprint& f, float gap)
	{
		KeepOut keep = LandmarkKeepOut(c.cx, c.cz);
		if (keep.radius > 0.0f && Overlaps(f, Circle(keep.center, keep.radius), 0.0f)) return false;
		for (size_t j = 0; j < footprints.size(); ++j)
		{
			if (Overlaps(f, footprints[j], gap)) return false;
		}
		return true;
	}

	void AddFixedProp(Chunk& c, std::vector<Footprint>& footprints, int model, const Vec3& pos, float yaw, float scale, float radius)
	{
		ChunkProp prop;
		prop.model = model;
		prop.pos = pos;
		prop.yaw = yaw;
		prop.scale = scale;
		prop.radius = radius;
		c.props.push_back(prop);
		footprints.push_back(Circle(pos, radius > 0.0f ? radius + 1.0f : 1.0f));
	}

	// A building or a wall: it collides as a box, and keeps a box of ground a little larger.
	void AddBoxProp(Chunk& c, std::vector<Footprint>& footprints, int model, const Vec3& pos, float yaw, float scale, float halfX, float halfZ)
	{
		ChunkProp prop;
		prop.model = model;
		prop.pos = pos;
		prop.yaw = yaw;
		prop.scale = scale;
		prop.halfX = halfX * scale;
		prop.halfZ = halfZ * scale;
		c.props.push_back(prop);
		footprints.push_back(Box(pos, yaw, prop.halfX + 0.3f, prop.halfZ + 0.3f));
	}

	void AddLandmarks(Chunk& c, std::vector<Footprint>& footprints)
	{
		const float halfPi = kPi * 0.5f;

		if (c.cx == 0 && c.cz == kBusStopChunkZ)
		{
			// The bus shelter faces the road; a car died beside it long ago. It stands on the west side so
			// its open front, and whatever waits in it, faces the quarter-view camera.
			AddFixedProp(c, footprints, MODEL_BUS_STOP, BusStopPos(), halfPi, 1.0f, 1.6f);
			AddFixedProp(c, footprints, MODEL_CAR, Vec3(RoadCenter(kBusStopZ + 6.0f) - 4.6f, 0.0f, kBusStopZ + 6.0f), -0.25f, 1.0f, 1.3f);
			AddFixedProp(c, footprints, MODEL_POLE, Vec3(RoadCenter(kBusStopZ - 5.0f) - 4.0f, 0.0f, kBusStopZ - 5.0f), 0.0f, 1.0f, 0.25f);
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
		else if (c.cx == 0 && c.cz == kOldTreeChunkZ)
		{
			// Where the road gives out: one old tree, larger than any around it.
			AddFixedProp(c, footprints, MODEL_TREE, OldTreePos(), 0.4f, 2.1f, 0.55f);
		}
	}

	struct GrowStep
	{
		int x;
		int z;
		int dir;
	};

	float KindWeight(const PropKind& kind, int stage, float modernity, float urban)
	{
		// The land grows back as the old world thins out; the town leaves about only what a town would.
		float scale = (0.55f + 0.95f * (1.0f - modernity)) * (1.0f - 0.5f * urban);
		if (kind.era == ERA_OLD) scale = modernity;
		else if (kind.era == ERA_OUTSKIRTS) scale = modernity * (1.0f - urban);
		return kind.weight[stage - 1] * scale;
	}

	const PropKind& PickPropKind(Rng& rng, int stage, float modernity, float urban)
	{
		float total = 0.0f;
		for (int i = 0; i < kPropKindCount; ++i)
			total += KindWeight(kPropKinds[i], stage, modernity, urban);

		float roll = rng.Unit() * total;
		for (int i = 0; i < kPropKindCount; ++i)
		{
			roll -= KindWeight(kPropKinds[i], stage, modernity, urban);
			if (roll < 0.0f) return kPropKinds[i];
		}
		return kPropKinds[0];
	}

	bool FindOpenSpot(Rng& rng, const Chunk& c, const std::vector<Footprint>& footprints, float footprint,
					  bool avoidRoad, bool avoidClearing, Vec3& out)
	{
		Vec3 center = ChunkMap::ChunkCenter(c.cx, c.cz);

		for (int attempt = 0; attempt < 8; ++attempt)
		{
			// Separate statements: argument evaluation order is unspecified, and the same seed
			// must give the same layout on every compiler and configuration.
			float offsetX = rng.Range(-0.5f, 0.5f);
			float offsetZ = rng.Range(-0.5f, 0.5f);
			Vec3 pos(center.x + offsetX * (kChunkSize - 2.0f), 0.0f, center.z + offsetZ * (kChunkSize - 2.0f));

			if (avoidRoad && OnStreet(pos, footprint)) continue;
			if (avoidClearing && DistXZ(pos, kClearingCenter) < kClearingRadius + footprint) continue;
			if (!Fits(c, footprints, Circle(pos, footprint), 0.6f)) continue;

			out = pos;
			return true;
		}
		return false;
	}

	// An open spot within spread of a point, on either axis.
	bool FindSpotNear(Rng& rng, const Chunk& c, const std::vector<Footprint>& footprints, const Vec3& around, float spread,
					  float footprint, Vec3& out)
	{
		for (int attempt = 0; attempt < 8; ++attempt)
		{
			float offsetX = rng.Range(-spread, spread);
			float offsetZ = rng.Range(-spread, spread);
			Vec3 pos(around.x + offsetX, 0.0f, around.z + offsetZ);
			if (!Fits(c, footprints, Circle(pos, footprint), 0.6f)) continue;

			out = pos;
			return true;
		}
		return false;
	}

	// A stone lantern still burning at a rest: at the roadside when the road runs through, else somewhere open.
	// Nothing else grows or is left within a few steps of it.
	void AddRestLantern(Chunk& c, std::vector<Footprint>& footprints, Rng& rng)
	{
		Vec3 center = ChunkMap::ChunkCenter(c.cx, c.cz);
		for (int attempt = 0; attempt < 12; ++attempt)
		{
			Vec3 pos;
			if (c.cx == 0)
			{
				float z = center.z + rng.Range(-8.0f, 8.0f);
				float side = rng.Chance(0.5f) ? 1.0f : -1.0f;
				float out = rng.Range(4.6f, 6.4f);
				pos = Vec3(RoadCenter(z) + side * out, 0.0f, z);
				if (UrbanAt(pos.x, pos.z) >= 0.25f && AtCrossStreet(z, 0.8f)) continue;
			}
			else
			{
				float offsetX = rng.Range(-6.0f, 6.0f);
				float offsetZ = rng.Range(-6.0f, 6.0f);
				pos = Vec3(center.x + offsetX, 0.0f, center.z + offsetZ);
				if (OnStreet(pos, 1.0f)) continue;
			}
			if (!Fits(c, footprints, Circle(pos, 1.0f), 0.3f)) continue;

			c.lanterns.push_back(pos);
			footprints.push_back(Circle(pos, 3.0f));
			return;
		}
	}

	// Creatures keep to one nest a chunk, well inside it, so the ground between nests is left quiet. The car
	// nearest the middle draws the mites, one swarm however many cars stand about; what else lives here gathers
	// at the same place, or, with no swarm, somewhere in the middle of the chunk.
	void AddNest(Chunk& c, const std::vector<Footprint>& footprints, Rng& rng, int ring)
	{
		Vec3 center = ChunkMap::ChunkCenter(c.cx, c.cz);
		Vec3 nest;
		bool hasNest = false;

		int car = -1;
		float carDist = 1e9f;
		for (size_t j = 0; j < c.props.size(); ++j)
		{
			if (c.props[j].model != MODEL_CAR) continue;
			float d = DistXZ(c.props[j].pos, center);
			if (d < carDist)
			{
				carDist = d;
				car = (int)j;
			}
		}
		if (car >= 0 && rng.Chance(0.7f))
		{
			const Vec3 at = c.props[car].pos;
			int swarm = (int)c.swarms.size();
			c.swarms.push_back(at);
			int mites = 2 + rng.RangeInt(0, 1);
			for (int k = 0; k < mites; ++k)
			{
				float angle = (float)k * 2.1f + rng.Range(0.0f, 1.0f);
				ChunkSpawn spawn;
				spawn.enemyType = ENEMY_SPORE_MITE;
				spawn.level = 1 + ring / 2;
				spawn.pos = at + Vec3(sinf(angle) * 2.2f, 0.0f, cosf(angle) * 2.2f);
				spawn.swarm = swarm;
				c.spawns.push_back(spawn);
			}
			nest = at;
			hasNest = true;
		}

		// A third of the chunks rest, so the nests are fuller than creatures once were spread.
		int spawnCount = rng.RangeInt(1, 2 + c.stage);
		if (!hasNest) hasNest = FindSpotNear(rng, c, footprints, center, kChunkSize * 0.25f, 1.2f, nest);
		if (!hasNest) return;

		for (int i = 0; i < spawnCount; ++i)
		{
			float roll = rng.Unit();
			int type = ENEMY_SPORE_MITE;
			if (ring >= 3 && roll < 0.25f) type = ENEMY_MOSS_BOAR;
			else if (roll < 0.55f) type = ENEMY_HUSK;

			ChunkSpawn spawn;
			if (!FindSpotNear(rng, c, footprints, nest, 4.5f, 0.8f, spawn.pos)) continue;
			spawn.enemyType = type;
			spawn.level = 1 + ring / 2 + (rng.Chance(0.3f) ? 1 : 0);
			c.spawns.push_back(spawn);
		}
	}

	// What a town lot can hold. Sizes run along the street (width) and back from it (depth).
	enum LotUse
	{
		LOT_OFFICE,
		LOT_VILLA,
		LOT_SHOP,
		LOT_STORE,
		LOT_COLLAPSED,
		LOT_EMPTY_HOUSE,
		LOT_HOUSE_SITE,
		LOT_WALL,
		LOT_PARKING,
		LOT_VACANT,
		LOT_USE_COUNT
	};

	struct LotKind
	{
		int   model;   // -1 for a lot without a building
		float width;
		float depth;
	};

	const LotKind kLotKinds[LOT_USE_COUNT] =
	{
		{ MODEL_OFFICE_4F, 10.0f, 8.0f },
		{ MODEL_VILLA_3F, 9.0f, 8.0f },
		{ MODEL_SHOP_2F, 8.0f, 7.0f },
		{ MODEL_STORE_1F, 8.0f, 6.0f },
		{ MODEL_COLLAPSED_BUILDING, 9.0f, 8.0f },
		{ MODEL_EMPTY_HOUSE, 7.0f, 5.5f },
		{ MODEL_HOUSE_SITE, 7.0f, 5.5f },
		{ MODEL_BLOCK_WALL, 4.2f, 2.0f },
		{ -1, 6.0f, 6.0f },
		{ -1, 6.0f, 6.0f },
	};

	// A row whose fronts face the camera may stand tall; a row that turns its back on it stays low, so it never
	// hides the street behind it. Away from its middle the town has more ruins and empty lots, then only
	// house sites and walls.
	float LotWeight(int use, bool facing, float urban)
	{
		float loss = 1.0f - urban;
		if (urban < 0.35f)
		{
			switch (use)
			{
			case LOT_COLLAPSED:   return facing && urban > 0.2f ? 0.5f : 0.0f;
			case LOT_EMPTY_HOUSE: return 1.0f;
			case LOT_HOUSE_SITE:  return 1.4f;
			case LOT_WALL:        return 1.0f;
			case LOT_VACANT:      return 1.6f;
			default:              return 0.0f;
			}
		}
		if (facing)
		{
			switch (use)
			{
			case LOT_OFFICE:    return urban > 0.75f ? 1.4f : 0.0f;
			case LOT_VILLA:     return 1.8f;
			case LOT_SHOP:      return 2.4f;
			case LOT_STORE:     return 0.6f;
			case LOT_COLLAPSED: return 0.45f + 1.4f * loss;
			case LOT_PARKING:   return 0.25f;
			case LOT_VACANT:    return 0.2f + 1.0f * loss;
			default:            return 0.0f;
			}
		}
		switch (use)
		{
		case LOT_STORE:   return 2.4f;
		case LOT_WALL:    return 0.4f + 0.6f * loss;
		case LOT_PARKING: return 0.8f;
		case LOT_VACANT:  return 0.4f + 1.0f * loss;
		default:          return 0.0f;
		}
	}

	// One row of lots along a street: the fronts on a line from `from` along `along`, the lots set back toward
	// `back`. Along the road the line follows its bends.
	struct LotRow
	{
		Vec3  from;
		Vec3  along;
		Vec3  back;
		float length;
		bool  facing;
		bool  followsRoad;
	};

	Vec3 RowPoint(const LotRow& row, float s)
	{
		Vec3 p = row.from + row.along * s;
		if (row.followsRoad) p.x = RoadCenter(p.z) + row.back.x * (kMainRoadHalf + kMainWalk);
		return p;
	}

	void PlaceLot(Chunk& c, std::vector<Footprint>& footprints, Rng& rng, const LotRow& row, float s, int use, bool origin)
	{
		const LotKind& kind = kLotKinds[use];
		Vec3 front = RowPoint(row, s);
		Vec3 back = row.back;
		if (row.followsRoad)
		{
			// Square to the road where it bends.
			float slope = RoadCenter(front.z + 0.5f) - RoadCenter(front.z - 0.5f);
			float len = sqrtf(1.0f + slope * slope);
			back = Vec3(row.back.x / len, 0.0f, -row.back.x * slope / len);
		}

		Vec3 center = front + back * (kind.depth * 0.5f);
		float yaw = atan2f(-back.x, -back.z);   // the front faces the street
		Vec3 side(cosf(yaw), 0.0f, -sinf(yaw));
		Footprint lot = Box(center, yaw, kind.width * 0.5f, kind.depth * 0.5f);
		if (origin && Overlaps(lot, Circle(kClearingCenter, kLotClearingRadius), 0.0f)) return;
		if (!Fits(c, footprints, lot, 0.2f)) return;
		footprints.push_back(Box(center, yaw, kind.width * 0.5f + 0.3f, kind.depth * 0.5f + 0.3f));

		switch (use)
		{
		case LOT_PARKING:
		{
			// Cars left nose in, a little askew.
			int cars = rng.Chance(0.5f) ? 2 : 1;
			for (int i = 0; i < cars; ++i)
			{
				float offset = cars == 1 ? 0.0f : (i == 0 ? -1.5f : 1.5f);
				float turn = rng.Chance(0.5f) ? kPi : 0.0f;
				AddFixedProp(c, footprints, MODEL_CAR, center + side * offset, yaw + turn + rng.Range(-0.2f, 0.2f), 1.0f, 1.3f);
			}
			break;
		}

		case LOT_VACANT:
		{
			// An empty lot gone to weeds: bushes, a young tree, now and then a broken wall.
			int count = rng.RangeInt(1, 3);
			for (int i = 0; i < count; ++i)
			{
				float ox = rng.Range(-2.0f, 2.0f);
				float oz = rng.Range(-2.0f, 2.0f);
				float roll = rng.Unit();
				Vec3 at = center + side * ox + back * oz;
				float turn = rng.Range(0.0f, 2.0f * kPi);
				float scale = rng.Range(0.8f, 1.2f);
				if (i == 0 && roll > 0.85f)
					AddFixedProp(c, footprints, MODEL_RUIN, center, yaw + (turn - kPi) * 0.1f, 0.9f, 0.9f);
				else if (roll < 0.5f)
					AddFixedProp(c, footprints, MODEL_BUSH, at, turn, scale, 0.0f);
				else
					AddFixedProp(c, footprints, MODEL_TREE, at, turn, scale * 0.75f, 0.2f);
			}
			break;
		}

		case LOT_WALL:
		{
			// A garden wall along the pavement, whatever it kept gone.
			AddBoxProp(c, footprints, MODEL_BLOCK_WALL, front + back * 0.4f, yaw, 1.0f, 2.0f, 0.15f);
			if (rng.Chance(0.6f))
				AddFixedProp(c, footprints, MODEL_BUSH, front + back * 1.5f + side * rng.Range(-1.2f, 1.2f), rng.Range(0.0f, 2.0f * kPi), 1.0f, 0.0f);
			break;
		}

		default:
			AddBoxProp(c, footprints, kind.model, center, yaw, 1.0f, kind.width * 0.5f, kind.depth * 0.5f);
			break;
		}
	}

	void FillRow(Chunk& c, std::vector<Footprint>& footprints, Rng& rng, const LotRow& row, bool origin)
	{
		float s = rng.Range(0.0f, 1.0f);
		while (s < row.length - 3.0f)
		{
			float room = row.length - s;
			Vec3 probe = RowPoint(row, s + Minf(room, 8.0f) * 0.5f);
			float urban = UrbanAt(probe.x, probe.z);
			if (urban < 0.08f)
			{
				s += 6.0f;
				continue;
			}

			float total = 0.0f;
			for (int i = 0; i < LOT_USE_COUNT; ++i)
			{
				if (kLotKinds[i].width <= room) total += LotWeight(i, row.facing, urban);
			}
			if (total <= 0.0f) break;

			float roll = rng.Unit() * total;
			int use = -1;
			for (int i = 0; i < LOT_USE_COUNT && use < 0; ++i)
			{
				float w = kLotKinds[i].width <= room ? LotWeight(i, row.facing, urban) : 0.0f;
				if (w <= 0.0f) continue;
				roll -= w;
				if (roll < 0.0f || i == LOT_USE_COUNT - 1) use = i;
			}
			if (use < 0) break;

			const LotKind& kind = kLotKinds[use];
			PlaceLot(c, footprints, rng, row, s + kind.width * 0.5f, use, origin);
			s += kind.width + rng.Range(0.3f, 1.2f);
		}
	}

	// What stood along the road: lamps on the pavements, signals at the wide crossings, cars stopped in the
	// lanes. Out of town a lamp or a stretch of guardrail is all that is left.
	void AddRoadFurniture(Chunk& c, std::vector<Footprint>& footprints, Rng& rng, bool origin)
	{
		const float halfPi = kPi * 0.5f;
		float zLow = (float)c.cz * kChunkSize - kChunkSize * 0.5f;
		float zHigh = zLow + kChunkSize;
		float urban = UrbanAt(0.0f, (zLow + zHigh) * 0.5f);

		// The way to Ginkgo Town, on the west pavement where it faces the road and the eye.
		if (c.cz == 1)
		{
			float z = 30.0f;
			Vec3 at(RoadCenter(z) - kMainRoadHalf - 1.4f, 0.0f, z);
			if (Fits(c, footprints, Box(at, halfPi, 1.3f, 0.2f), 0.1f))
				AddBoxProp(c, footprints, MODEL_ROAD_SIGN, at, halfPi, 1.0f, 1.1f, 0.2f);
		}

		// Signals on the far corners of a wide crossing, their arms out over the lanes.
		if (WideStreetAfter(c.cz) && UrbanAt(0.0f, zHigh) > 0.3f && rng.Chance(0.85f))
		{
			float z = zHigh - kStreetHalf - 4.2f;
			Vec3 at(RoadCenter(z) + 5.0f, 0.0f, z);
			if (Fits(c, footprints, Circle(at, 0.3f), 0.1f))
				AddFixedProp(c, footprints, MODEL_TRAFFIC_LIGHT, at, kPi, 1.0f, 0.28f);
		}
		if (WideStreetAfter(c.cz - 1) && UrbanAt(0.0f, zLow) > 0.3f && rng.Chance(0.85f))
		{
			float z = zLow + kStreetHalf + 4.2f;
			Vec3 at(RoadCenter(z) - 5.0f, 0.0f, z);
			if (Fits(c, footprints, Circle(at, 0.3f), 0.1f))
				AddFixedProp(c, footprints, MODEL_TRAFFIC_LIGHT, at, 0.0f, 1.0f, 0.28f);
		}

		// Lamps on alternate sides, their heads out over the kerb; where one is missing, a street tree.
		for (int i = 0; i < 2; ++i)
		{
			float z = zLow + 6.0f + 12.0f * (float)i;
			float side = ((c.cz + i) & 1) ? -1.0f : 1.0f;
			Vec3 at(RoadCenter(z) + side * 5.0f, 0.0f, z);
			float here = UrbanAt(at.x, at.z);
			float chance = here > 0.12f ? (here > 0.5f ? 0.85f : 0.35f + 0.5f * here) : 0.3f * c.modernity;
			bool lamp = rng.Chance(chance);
			bool tree = !lamp && here > 0.3f && rng.Chance(0.35f);
			if ((!lamp && !tree) || !Fits(c, footprints, Circle(at, 0.25f), 0.1f)) continue;

			if (lamp)
				AddFixedProp(c, footprints, MODEL_STREETLAMP, at, side > 0.0f ? -halfPi : halfPi, rng.Range(0.95f, 1.05f), 0.2f);
			else
				AddFixedProp(c, footprints, MODEL_TREE, at, rng.Range(0.0f, 2.0f * kPi), rng.Range(0.7f, 0.9f), 0.2f);
		}

		// Cars stopped where their drivers fell asleep, mostly in their own lane. The opening stays clear.
		int cars = (urban > 0.2f && !origin) ? rng.RangeInt(0, urban > 0.5f ? 2 : 1) : 0;
		for (int i = 0; i < cars; ++i)
		{
			float z = rng.Range(zLow + 6.5f, zHigh - 6.5f);
			bool south = rng.Chance(0.5f);
			float turn = rng.Range(-0.35f, 0.35f);
			Vec3 at(RoadCenter(z) + (south ? -1.8f : 1.8f), 0.0f, z);
			if (Fits(c, footprints, Circle(at, 1.6f), 0.3f))
				AddFixedProp(c, footprints, MODEL_CAR, at, (south ? 0.0f : kPi) + turn, 1.0f, 1.3f);
		}

		// Where the town has thinned, guardrail along the verge, its rail running with the road and facing it.
		if (urban < 0.5f && c.modernity > 0.15f)
		{
			for (int sideIndex = 0; sideIndex < 2; ++sideIndex)
			{
				if (!rng.Chance(0.35f * c.modernity)) continue;
				float side = sideIndex == 0 ? -1.0f : 1.0f;
				int count = rng.RangeInt(2, 4);
				float z = rng.Range(zLow + 1.0f, zHigh - 1.0f - 4.0f * (float)count);
				for (int k = 0; k < count; ++k, z += 4.0f)
				{
					float mid = z + 2.0f;
					float slope = RoadCenter(mid + 0.5f) - RoadCenter(mid - 0.5f);
					float yaw = side > 0.0f ? atan2f(-1.0f, slope) : atan2f(1.0f, -slope);
					Vec3 at(RoadCenter(mid) + side * 3.9f, 0.0f, mid);
					if (UrbanAt(at.x, at.z) > 0.15f && AtCrossStreet(mid, 2.0f)) continue;
					if (Fits(c, footprints, Box(at, yaw, 2.0f, 0.25f), 0.1f))
						AddBoxProp(c, footprints, MODEL_GUARDRAIL, at, yaw, 1.0f, 2.0f, 0.23f);
				}
			}
		}
	}

	// The old town's share of a chunk: rows of lots along the road and the cross streets, kept inside the
	// chunk so neighbours never build over each other, then what stood in the streets.
	void BuildTown(Chunk& c, std::vector<Footprint>& footprints, Rng& rng, bool origin)
	{
		float zLow = (float)c.cz * kChunkSize - kChunkSize * 0.5f;
		float zHigh = zLow + kChunkSize;
		float lowSet = StreetSetback(c.cz - 1);
		float highSet = StreetSetback(c.cz);

		// The corner of the chunk nearest the start is where any of the town would still stand.
		float nearX = Clampf(0.0f, (float)c.cx * kChunkSize - kChunkSize * 0.5f, (float)c.cx * kChunkSize + kChunkSize * 0.5f);
		float nearZ = Clampf(0.0f, zLow, zHigh);
		bool town = UrbanAt(nearX, nearZ) > 0.08f;

		if (c.cx == 0)
		{
			// Along the road the west row faces the camera; the east row turns its back to it.
			if (town)
			{
				float from = zLow + lowSet + 0.4f;
				float length = zHigh - highSet - 0.4f - from;
				LotRow west = { Vec3(0.0f, 0.0f, from), Vec3(0.0f, 0.0f, 1.0f), Vec3(-1.0f, 0.0f, 0.0f), length, true, true };
				LotRow east = { Vec3(0.0f, 0.0f, from), Vec3(0.0f, 0.0f, 1.0f), Vec3(1.0f, 0.0f, 0.0f), length, false, true };
				FillRow(c, footprints, rng, west, origin);
				FillRow(c, footprints, rng, east, origin);
			}
			AddRoadFurniture(c, footprints, rng, origin);
			return;
		}
		if (!town) return;

		// Along the cross streets, clear of the road's rows: the row behind the next street faces the camera,
		// the row before the last one turns its back.
		float xLow = (float)c.cx * kChunkSize - kChunkSize * 0.5f + 0.5f;
		float xHigh = xLow + kChunkSize - 1.0f;
		if (c.cx == 1) xLow = 18.5f;
		if (c.cx == -1) xHigh = -18.5f;
		LotRow facing = { Vec3(xLow, 0.0f, zHigh - highSet), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), xHigh - xLow, true, false };
		LotRow backed = { Vec3(xLow, 0.0f, zLow + lowSet), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), xHigh - xLow, false, false };
		FillRow(c, footprints, rng, facing, origin);
		FillRow(c, footprints, rng, backed, origin);

		// A car or two left at the kerb of a wide street.
		for (int i = 0; i < 2; ++i)
		{
			bool high = i == 1;
			if (!WideStreetAfter(high ? c.cz : c.cz - 1)) continue;
			float edge = high ? zHigh : zLow;
			float x = rng.Range(xLow + 2.5f, xHigh - 2.5f);
			if (UrbanAt(x, edge) < 0.3f || !rng.Chance(0.5f)) continue;
			Vec3 at(x, 0.0f, edge + (high ? -1.4f : 1.4f));
			float turn = rng.Range(-0.15f, 0.15f);
			if (Fits(c, footprints, Circle(at, 1.6f), 0.2f))
				AddFixedProp(c, footprints, MODEL_CAR, at, (high ? -kPi : kPi) * 0.5f + turn, 1.0f, 1.3f);
		}
	}

	// Where a family lived out past the town: what is left of the house, facing the road if there is one,
	// the jars by the house, a gate where the yard met the road, walls down its sides, a thing they used.
	void AddHomestead(Chunk& c, std::vector<Footprint>& footprints, Rng& rng)
	{
		Vec3 middle = ChunkMap::ChunkCenter(c.cx, c.cz);
		for (int attempt = 0; attempt < 6; ++attempt)
		{
			float z = middle.z + rng.Range(-6.0f, 6.0f);
			float side = rng.Chance(0.5f) ? -1.0f : 1.0f;
			float offset = rng.Range(10.0f, 12.5f);
			float spread = rng.Range(-6.0f, 6.0f);
			float yaw = (float)rng.RangeInt(0, 3) * kPi * 0.5f + rng.Range(-0.12f, 0.12f);
			float x = middle.x + spread;
			if (c.cx == 0)
			{
				x = RoadCenter(z) + side * offset;
				yaw = (side > 0.0f ? -kPi : kPi) * 0.5f + rng.Range(-0.12f, 0.12f);
			}

			Vec3 at(x, 0.0f, z);
			Vec3 front(sinf(yaw), 0.0f, cosf(yaw));
			Vec3 right(cosf(yaw), 0.0f, -sinf(yaw));
			Footprint yard = Box(at + front * 1.5f, yaw, 5.5f, 4.5f);
			if (OnStreet(at + front * 6.0f, 0.3f) || !Fits(c, footprints, yard, 0.3f)) continue;
			footprints.push_back(yard);

			AddBoxProp(c, footprints, rng.Chance(0.6f) ? MODEL_HOUSE_SITE : MODEL_EMPTY_HOUSE, at, yaw, 1.0f, 3.5f, 2.75f);

			float jarSide = rng.Chance(0.5f) ? -1.0f : 1.0f;
			AddFixedProp(c, footprints, MODEL_JANGDOK, at + right * (jarSide * 2.4f) + front * 4.0f, yaw + rng.Range(-0.3f, 0.3f), rng.Range(0.9f, 1.05f), 0.75f);
			if (rng.Chance(0.6f))
				AddFixedProp(c, footprints, MODEL_GATE, at + front * 6.2f + right * rng.Range(-1.5f, 1.5f), yaw, 1.0f, 0.4f);
			if (rng.Chance(0.7f))
			{
				AddBoxProp(c, footprints, MODEL_BLOCK_WALL, at + right * 5.2f + front * 3.0f, yaw + kPi * 0.5f, 1.0f, 2.0f, 0.15f);
				if (rng.Chance(0.5f))
					AddBoxProp(c, footprints, MODEL_BLOCK_WALL, at - right * 5.2f + front * 3.0f, yaw - kPi * 0.5f, 1.0f, 2.0f, 0.15f);
			}

			const int kept[3] = { MODEL_WELL, MODEL_PYEONGSANG, MODEL_TILLER };
			int pick = rng.RangeInt(0, 2);
			float turn = rng.Range(-0.5f, 0.5f);
			AddFixedProp(c, footprints, kept[pick], at - right * (jarSide * 2.2f) + front * 4.4f, yaw + turn, 1.0f, pick == 0 ? 0.9f : 0.8f);
			return;
		}
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
			h = HashInt(h, s.swarm);
			h = HashInt(h, s.guardOf);
		}
		for (size_t i = 0; i < c.items.size(); ++i)
		{
			const ChunkItem& item = c.items[i];
			h = HashInt(h, item.itemType);
			h = HashInt(h, Quantise(item.pos.x));
			h = HashInt(h, Quantise(item.pos.z));
		}
		for (size_t i = 0; i < c.bushes.size(); ++i)
		{
			const ChunkBush& b = c.bushes[i];
			h = HashInt(h, b.pale ? 1 : 0);
			h = HashInt(h, Quantise(b.pos.x));
			h = HashInt(h, Quantise(b.pos.z));
		}
		h = HashInt(h, c.rest ? 1 : 0);
		for (size_t i = 0; i < c.lanterns.size(); ++i)
		{
			h = HashInt(h, Quantise(c.lanterns[i].x));
			h = HashInt(h, Quantise(c.lanterns[i].z));
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

const Chunk* ChunkMap::Find(int cx, int cz) const
{
	std::unordered_map<uint64_t, Chunk>::const_iterator found = chunks.find(Key(cx, cz));
	return found != chunks.end() ? &found->second : nullptr;
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

	// Naturalisation deepens with distance from where the road left the village; the town keeps its first stage.
	int ring = abs(cx) > abs(cz) ? abs(cx) : abs(cz);
	c.stage = 1 + ring / 3 + (ring > 3 && rng.Chance(0.3f) ? 1 : 0);
	if (origin) c.stage = 1;
	if (c.stage > 3) c.stage = 3;
	c.modernity = ModernityAt(ring);
	Vec3 middle = ChunkCenter(cx, cz);
	c.urban = UrbanAt(middle.x, middle.z);

	std::vector<Footprint> footprints;
	AddLandmarks(c, footprints);

	// The town stands around the start of the road; past it, now and then, what is left of a homestead.
	BuildTown(c, footprints, rng, origin);
	if (!origin && ring >= 3 && c.urban < 0.35f && rng.Chance(0.55f * c.modernity))
		AddHomestead(c, footprints, rng);

	// Some rests keep a lantern lit, more of them where the old world is nearer.
	c.rest = IsRestChunk(cx, cz, worldSeed);
	if (c.rest && rng.Chance(0.35f + 0.4f * c.modernity)) AddRestLantern(c, footprints, rng);

	// Then whatever the land and the old world scattered about; the town leaves little room for it.
	int count = (int)((float)(8 + rng.RangeInt(0, 6) + (c.stage - 1) * 3) * (1.0f - 0.6f * c.urban) + 0.5f);
	for (int i = 0; i < count; ++i)
	{
		const PropKind& kind = PickPropKind(rng, c.stage, c.modernity, c.urban);
		float scale = rng.Range(kind.minScale, kind.maxScale) * (c.stage == 3 && kind.era == ERA_WILD ? 1.25f : 1.0f);
		float footprint = kind.spacing * scale;

		Vec3 pos;
		if (!FindOpenSpot(rng, c, footprints, footprint, true, origin, pos)) continue;

		float yaw = rng.Range(0.0f, 2.0f * kPi);
		if (kind.halfX > 0.0f)
		{
			AddBoxProp(c, footprints, kind.model, pos, yaw, scale, kind.halfX, kind.halfZ);
			continue;
		}

		ChunkProp prop;
		prop.model = kind.model;
		prop.pos = pos;
		prop.yaw = yaw;
		prop.scale = scale;
		prop.radius = kind.collision * scale;
		c.props.push_back(prop);
		footprints.push_back(Circle(pos, footprint));
	}

	// The opening chunk is scripted, and the first ring stays free of creatures so the guided
	// fights are not interrupted. Beyond that, creatures and finds are part of the seed.
	if (!origin)
	{
		// Mites gather on what the old world left full of memory: an abandoned car draws a swarm.
		if (ring >= 2 && !c.rest) AddNest(c, footprints, rng, ring);

		if (ring >= 2)
		{
			// Now and then a sleeper by the road, and, away from a rest, a boar asleep at their side keeping watch.
			ChunkSleeper sleeper;
			if (rng.Chance(0.3f) && FindOpenSpot(rng, c, footprints, 1.6f, true, false, sleeper.pos))
			{
				sleeper.yaw = rng.Range(0.0f, 2.0f * kPi);
				int index = (int)c.sleepers.size();
				c.sleepers.push_back(sleeper);
				AddFixedProp(c, footprints, MODEL_SLEEPER, sleeper.pos, sleeper.yaw, 1.0f, 0.0f);

				if (!c.rest)
				{
					ChunkSpawn guard;
					guard.enemyType = ENEMY_MOSS_BOAR;
					guard.level = 1 + ring / 2;
					guard.pos = sleeper.pos + Vec3(sinf(sleeper.yaw + 1.6f) * 1.8f, 0.0f, cosf(sleeper.yaw + 1.6f) * 1.8f);
					guard.guardOf = index;
					c.spawns.push_back(guard);
				}
			}
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

	// Berry bushes last, so they never push the rest of the layout around. Every chunk has some:
	// the road must be able to feed whoever walks it.
	int bushCount = rng.RangeInt(1, 3);
	for (int i = 0; i < bushCount; ++i)
	{
		ChunkBush bush;
		if (!FindOpenSpot(rng, c, footprints, 0.9f, true, origin, bush.pos)) continue;
		bush.yaw = rng.Range(0.0f, 2.0f * kPi);
		bush.pale = rng.Chance(0.25f);
		bush.bearing = rng.Unit();
		c.bushes.push_back(bush);
	}

	c.hash = HashChunk(c);
	return c;
}
