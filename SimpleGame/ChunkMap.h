#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Math3D.h"

const float kChunkSize = 24.0f;

// Must match roadCenter() in Shaders/Lit.fs so props line up with the painted road.
inline float RoadCenter(float z) { return sinf(z * 0.05f) * 3.0f; }

// The old town around the start of Route 32, which Lit.fs paints to the same measures. Cross streets run
// along the chunk borders across the road: the one after an even chunk row is wide, with pavements; the
// rest are alleys.
const float kMainRoadHalf = 3.6f;    // the road's asphalt either side of its centre, in town
const float kMainWalk = 2.4f;        // its pavement beyond that
const float kStreetHalf = 2.6f;      // a wide cross street's asphalt
const float kStreetWalk = 1.2f;
const float kAlleyHalf = 1.6f;

// 1 where the town still stands, fading out by four chunks from the start. Must match urbanAt() in Lit.fs.
inline float UrbanAt(float x, float z)
{
	float d = fabsf(x) > fabsf(z) ? fabsf(x) : fabsf(z);
	float t = (d - 30.0f) / 60.0f;
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	return 1.0f - t * t * (3.0f - 2.0f * t);
}

// Whether the cross street on the border after chunk row cz is a wide one.
inline bool WideStreetAfter(int cz) { return (cz & 1) == 0; }

// How far from that street's centre line the lots begin.
inline float StreetSetback(int cz) { return WideStreetAfter(cz) ? kStreetHalf + kStreetWalk : kAlleyHalf; }

// Fixed places on Route 32 (ContentDesign.md 3.2). The chunk map keeps its random props clear of them.
const int   kBusStopChunkZ = 2;
const float kBusStopZ = 48.0f;
const int   kReservoirChunkX = 1;
const int   kReservoirChunkZ = 4;
const Vec3  kReservoirCenter(20.0f, 0.0f, 96.0f);
const float kReservoirSizeX = 12.0f;
const float kReservoirSizeZ = 9.0f;
const int   kTownChunkZ = 6;
const float kTownGateZ = 140.0f;

// The lone tree at the south end of grandmother's map, where her road gives out. West of the road, so it
// stands behind the road from the quarter-view camera instead of in front of it.
const int   kOldTreeChunkZ = 10;
const float kOldTreeZ = 240.0f;
inline Vec3 OldTreePos() { return Vec3(RoadCenter(kOldTreeZ) - 6.5f, 0.0f, kOldTreeZ); }

struct ChunkProp
{
	int   model = 0;
	Vec3  pos;
	float yaw = 0.0f;
	float scale = 1.0f;
	float radius = 0.0f;     // collision radius; zero takes no part in collision
	float halfX = 0.0f;      // a collision box instead, along the prop's own axes, when not zero
	float halfZ = 0.0f;
};

struct ChunkSpawn
{
	int  enemyType = 0;
	int  level = 1;
	Vec3 pos;
	int  swarm = -1;      // mites crowding a car: index into Chunk::swarms
	int  guardOf = -1;    // a boar asleep beside a sleeper: index into Chunk::sleepers
};

// A berry bush that can be picked and grows its fruit back. Whether it fruits in a season is
// decided at play time from `bearing`, so the seed keeps the same bushes all year.
struct ChunkBush
{
	Vec3  pos;
	float yaw = 0.0f;
	bool  pale = false;     // the berries that twist your belly
	float bearing = 0.0f;   // [0,1); fruits while below the season's share
};

// Someone who lay down by the road and did not wake. Drawn as one of the chunk's props.
struct ChunkSleeper
{
	Vec3  pos;
	float yaw = 0.0f;
};

struct ChunkItem
{
	int  itemType = 0;
	Vec3 pos;
};

struct Chunk
{
	int      cx = 0;
	int      cz = 0;
	int      parentDir = -1; // direction from the parent chunk, -1 at the origin
	uint64_t seed = 0;
	uint64_t hash = 0;       // hash of the generated content; seeds the chunks grown from here
	int      stage = 1;      // naturalisation stage 1..3
	float    modernity = 1.0f; // 1 near the village, 0 far out: how much of the old world is left
	float    urban = 0.0f;     // how much of the old town stands at the chunk's centre
	bool     rest = false;     // no creature nests here: a quiet stretch between their nests
	std::vector<ChunkProp>  props;
	std::vector<Vec3>       lanterns;  // stone lanterns still burning at a rest
	std::vector<ChunkSpawn> spawns;
	std::vector<ChunkItem>  items;
	std::vector<Vec3>       swarms;    // the cars the mites gather on
	std::vector<ChunkSleeper> sleepers;
	std::vector<ChunkBush>  bushes;
};

class ChunkMap
{
public:
	void Reset(uint64_t seed);

	// Generates the chunk, and any missing ancestors, on first use.
	const Chunk& Get(int cx, int cz);
	// The chunk if it has been generated, without generating it.
	const Chunk* Find(int cx, int cz) const;

	uint64_t WorldSeed() const { return worldSeed; }
	int      GeneratedCount() const { return (int)chunks.size(); }

	static uint64_t Key(int cx, int cz);
	static void ChunkCoords(const Vec3& pos, int& cx, int& cz);
	static Vec3 ChunkCenter(int cx, int cz);

private:
	static bool ParentOf(int cx, int cz, int& px, int& pz, int& dir);
	Chunk Generate(int cx, int cz, uint64_t seed, int parentDir) const;

	uint64_t worldSeed = 0;
	std::unordered_map<uint64_t, Chunk> chunks;
};
