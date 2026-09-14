#pragma once

#include <cstdint>
#include <cstddef>

const uint64_t kFnvOffset = 14695981039346656037ULL;
const uint64_t kFnvPrime = 1099511628211ULL;

inline uint64_t HashBytes(uint64_t h, const void* data, size_t size)
{
	const unsigned char* p = static_cast<const unsigned char*>(data);
	for (size_t i = 0; i < size; ++i)
	{
		h ^= p[i];
		h *= kFnvPrime;
	}
	return h;
}

inline uint64_t HashInt(uint64_t h, int32_t v) { return HashBytes(h, &v, sizeof(v)); }
inline uint64_t HashU64(uint64_t h, uint64_t v) { return HashBytes(h, &v, sizeof(v)); }
inline uint64_t HashFloat(uint64_t h, float v) { return HashBytes(h, &v, sizeof(v)); }

// SplitMix64 finalizer: spreads neighbouring inputs across the whole range.
inline uint64_t MixSeed(uint64_t x)
{
	x += 0x9E3779B97F4A7C15ULL;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
	return x ^ (x >> 31);
}

class Rng
{
public:
	explicit Rng(uint64_t seed) : state(seed) {}

	uint64_t Next()
	{
		state += 0x9E3779B97F4A7C15ULL;
		uint64_t z = state;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		return z ^ (z >> 31);
	}

	float Unit() { return (float)(Next() >> 40) / 16777216.0f; }
	float Range(float lo, float hi) { return lo + (hi - lo) * Unit(); }
	int   RangeInt(int lo, int hiInclusive) { return lo + (int)(Next() % (uint64_t)(hiInclusive - lo + 1)); }
	bool  Chance(float p) { return Unit() < p; }

private:
	uint64_t state;
};
