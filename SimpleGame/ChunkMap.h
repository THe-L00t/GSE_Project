#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Math3D.h"

const float kChunkSize = 24.0f;

// Must match roadCenter() in Shaders/Lit.fs so props line up with the painted road.
inline float RoadCenter(float z) { return sinf(z * 0.05f) * 3.0f; }

struct ChunkProp
{
	int   model = 0;
	Vec3  pos;
	float yaw = 0.0f;
	float scale = 1.0f;
	float radius = 0.0f;     // collision radius; zero takes no part in collision
};

struct ChunkSpawn
{
	int  enemyType = 0;
	int  level = 1;
	Vec3 pos;
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
	std::vector<ChunkProp>  props;
	std::vector<ChunkSpawn> spawns;
	std::vector<ChunkItem>  items;
};

class ChunkMap
{
public:
	void Reset(uint64_t seed);

	// Generates the chunk, and any missing ancestors, on first use.
	const Chunk& Get(int cx, int cz);

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
