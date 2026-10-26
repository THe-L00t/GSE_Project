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
// The reservoir is a pond shape (PondShore in Mesh.h) turned to run north-south. The drought has drawn it back
// from where it used to reach, leaving a band of cracked mud as wide as the distance it is noticed from.
const Vec3  kReservoirCenter(21.5f, 0.0f, 96.0f);
const float kReservoirSizeX = 12.8f;
const float kReservoirSizeZ = 10.0f;
const float kReservoirYaw = 1.95f;
const float kReservoirDry = 3.0f;
const int   kTownChunkZ = 6;
const float kTownGateZ = 140.0f;

// The shelter stands west of the road, so its open front faces the quarter-view camera.
inline Vec3 BusStopPos() { return Vec3(RoadCenter(kBusStopZ) - 5.2f, 0.0f, kBusStopZ); }
// The road at the town gate, between its two ginkgo trees.
inline Vec3 TownGatePos() { return Vec3(RoadCenter(kTownGateZ), 0.0f, kTownGateZ); }

// The lone tree at the south end of grandmother's map, where her road gives out. West of the road, so it
// stands behind the road from the quarter-view camera instead of in front of it.
const int   kOldTreeChunkZ = 10;
const float kOldTreeZ = 240.0f;
inline Vec3 OldTreePos() { return Vec3(RoadCenter(kOldTreeZ) - 6.5f, 0.0f, kOldTreeZ); }

// The school wall at the town gate, where Hayul's mother left a few words in chalk; read from the road side.
inline Vec3 TownWallPos() { return Vec3(RoadCenter(kTownGateZ) + 8.6f, 0.0f, kTownGateZ + 6.0f); }

// Past Ginkgo Town (ContentDesign.md 6). Where these lie changes with the world seed, but every world has them,
// all beyond the lone tree: the great tree with the clear air under it, the sleeping city round it, the pylon
// ridge from where it first shows, and the two who lie at the city's near edge.
const float kSanctuaryRadius = 30.0f;    // under the great tree the spores are all drawn in and the air is clear
const int   kCityChunks = 7;             // how far the sleeping city spreads round the tree

struct StorySites
{
	int  rootX = 0;
	int  rootZ = 0;
	int  ridgeX = 0;
	int  ridgeZ = 0;
	int  parentsX = 0;
	int  parentsZ = 0;
	Vec3 root;        // the great tree, at the centre of its chunk
	Vec3 ridge;       // the pylon
	Vec3 parents;     // where the two lie side by side
	Vec3 toTown;      // from the tree back toward Ginkgo Town, one unit long
};

// The home the parents began under the tree, on the side the camera looks from: four things to build, and the
// jars they left, which keep from the start.
enum HomeSpot
{
	HOME_SHELTER,
	HOME_WELL,
	HOME_GARDEN,
	HOME_LANTERN,
	HOME_STORE,
	HOME_SPOT_COUNT
};
const int kHomeProjects = HOME_STORE;

inline Vec3 HomeSpotPos(const StorySites& s, int spot)
{
	// Out from the trunk toward the camera (front), and across that (side).
	const float front[HOME_SPOT_COUNT] = { 13.0f, 9.0f, 16.0f, 14.0f, 10.0f };
	const float side[HOME_SPOT_COUNT] = { -6.0f, 9.0f, 10.0f, 4.0f, 1.0f };
	const float k = 0.70710678f;
	return s.root + Vec3((front[spot] + side[spot]) * k, 0.0f, (front[spot] - side[spot]) * k);
}

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

// Reeds standing in a few clumps along a pond's edge and a scatter of stones on the mud it left behind; the side
// facing openAngle (a world direction, radians from +x toward +z) stays open, so the water shows and can be
// reached from there. Nothing in it collides.
void ShoreDressing(const Vec3& center, float halfX, float halfZ, float yaw, float dry, float openAngle, uint64_t seed,
				   std::vector<ChunkProp>& out);

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
	int  relicId = -1;    // a relic kept for one place; -1 lets the game pick
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
	const StorySites& Sites() const { return sites; }

	static uint64_t Key(int cx, int cz);
	static void ChunkCoords(const Vec3& pos, int& cx, int& cz);
	static Vec3 ChunkCenter(int cx, int cz);

private:
	static bool ParentOf(int cx, int cz, int& px, int& pz, int& dir);
	Chunk Generate(int cx, int cz, uint64_t seed, int parentDir) const;

	uint64_t worldSeed = 0;
	StorySites sites;
	std::unordered_map<uint64_t, Chunk> chunks;
};
