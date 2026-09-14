#pragma once

#include "Math3D.h"

// Pushes a circle of the given radius out of an axis-aligned box on the XZ plane.
inline void PushOutOfBox(Vec3& pos, float radius, const Vec3& center, float halfX, float halfZ)
{
	float hx = halfX + radius;
	float hz = halfZ + radius;
	float dx = pos.x - center.x;
	float dz = pos.z - center.z;
	if (fabsf(dx) >= hx || fabsf(dz) >= hz) return;

	float penX = hx - fabsf(dx);
	float penZ = hz - fabsf(dz);
	if (penX < penZ)
		pos.x += (dx < 0.0f ? -penX : penX);
	else
		pos.z += (dz < 0.0f ? -penZ : penZ);
}

// Pushes a circle out of another circle on the XZ plane.
inline void PushOutOfCircle(Vec3& pos, float radius, const Vec3& center, float otherRadius)
{
	float dx = pos.x - center.x;
	float dz = pos.z - center.z;
	float minDist = radius + otherRadius;
	float distSq = dx * dx + dz * dz;
	if (distSq >= minDist * minDist) return;

	float dist = sqrtf(distSq);
	if (dist < 1e-4f)
	{
		pos.x += minDist;
		return;
	}

	float push = (minDist - dist) / dist;
	pos.x += dx * push;
	pos.z += dz * push;
}
