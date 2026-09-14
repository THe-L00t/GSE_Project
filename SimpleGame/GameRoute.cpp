#include "stdafx.h"
#include "Game.h"

#include <cstdio>
#include <ctime>

#include "Collision.h"
#include "Models.h"
#include "Random.h"

namespace
{
	// 0 grows a different Route 32 every run; any other value replays that world.
	const uint64_t kRouteSeed = 0;

	const int kStreamRadius = 2;   // chunks kept generated around the player
	const int kDrawRadius = 1;     // chunks drawn around the camera; covers the orthographic view
}

void Game::StartRoute()
{
	level = LEVEL_ROUTE;
	levelTimer = 0.0f;
	endingTimer = -1.0f;
	letterOpen = false;
	prompt.clear();

	uint64_t seed = kRouteSeed != 0 ? kRouteSeed : MixSeed((uint64_t)std::time(nullptr));
	routeMap.Reset(seed);

	playerPos = Vec3(0.0f, 0.0f, -6.0f);
	playerYaw = 0.0f;
	rollTimer = 0.0f;
	rollAngle = 0.0f;
	camTarget = playerPos;
	sporeExposure = Minf(sporeExposure, 0.3f);

	StreamChunks();
	ShowMessage("The road runs on. Walk any way you like; the land grows as you go.", 5.0f);
}

void Game::UpdateRoute(float dt, const bool* keys)
{
	UpdatePlayer(dt, keys);
	ResolveRouteCollisions();
	StreamChunks();

	// Deeper, more overgrown chunks carry thicker spores. Nature Insight still slows it.
	const Chunk& here = routeMap.Get(playerChunkX, playerChunkZ);
	float insight = Maxf(1.0f - 0.18f * (float)fragments, 0.3f);
	float rate = 0.012f * (float)here.stage * insight;
	if (rollTimer > 0.0f) rate = 0.0f;

	sporeExposure = Saturatef(sporeExposure + rate * dt);
}

void Game::StreamChunks()
{
	ChunkMap::ChunkCoords(playerPos, playerChunkX, playerChunkZ);

	for (int dz = -kStreamRadius; dz <= kStreamRadius; ++dz)
	{
		for (int dx = -kStreamRadius; dx <= kStreamRadius; ++dx)
			routeMap.Get(playerChunkX + dx, playerChunkZ + dz);
	}
}

void Game::ResolveRouteCollisions()
{
	int cx, cz;
	ChunkMap::ChunkCoords(playerPos, cx, cz);

	for (int dz = -1; dz <= 1; ++dz)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			const Chunk& c = routeMap.Get(cx + dx, cz + dz);
			for (size_t i = 0; i < c.props.size(); ++i)
			{
				if (c.props[i].radius > 0.0f)
					PushOutOfCircle(playerPos, kPlayerRadius, c.props[i].pos, c.props[i].radius);
			}
		}
	}
}

void Game::DrawRoute()
{
	int cx, cz;
	ChunkMap::ChunkCoords(camTarget, cx, cz);

	for (int dz = -kDrawRadius; dz <= kDrawRadius; ++dz)
	{
		for (int dx = -kDrawRadius; dx <= kDrawRadius; ++dx)
		{
			const Chunk& c = routeMap.Get(cx + dx, cz + dz);

			GroundParams ground;
			ground.stage = (float)c.stage;
			ground.neighborStage[0] = (float)routeMap.Get(c.cx - 1, c.cz).stage;
			ground.neighborStage[1] = (float)routeMap.Get(c.cx + 1, c.cz).stage;
			ground.neighborStage[2] = (float)routeMap.Get(c.cx, c.cz - 1).stage;
			ground.neighborStage[3] = (float)routeMap.Get(c.cx, c.cz + 1).stage;
			ground.chunkSize = kChunkSize;

			// A hair of overlap hides cracks between neighbouring ground quads.
			renderer->DrawGround(ChunkMap::ChunkCenter(c.cx, c.cz), kChunkSize + 0.02f, ground);

			for (size_t i = 0; i < c.props.size(); ++i)
			{
				const ChunkProp& p = c.props[i];
				DrawParams params;
				params.phase = p.pos.x * 0.37f + p.pos.z * 0.21f;
				renderer->DrawModel(p.model, p.pos, p.yaw, Vec3(p.scale, p.scale, p.scale), params);
			}
		}
	}
}

void Game::DrawRouteHud()
{
	const int w = renderer->GetWidth();

	DrawObjective("Walk Route 32. The land grows in every direction.");

	const Chunk& here = routeMap.Get(playerChunkX, playerChunkZ);
	char lines[4][96];
	sprintf_s(lines[0], sizeof(lines[0]), "Seed  %016llX", (unsigned long long)routeMap.WorldSeed());
	sprintf_s(lines[1], sizeof(lines[1]), "Chunk (%d, %d)  stage %d", here.cx, here.cz, here.stage);
	sprintf_s(lines[2], sizeof(lines[2]), "Chunk hash  %016llX", (unsigned long long)here.hash);
	sprintf_s(lines[3], sizeof(lines[3]), "Chunks grown  %d", routeMap.GeneratedCount());

	int boxW = 0;
	for (int i = 0; i < 4; ++i)
	{
		int tw = renderer->TextWidth(lines[i], false);
		if (tw > boxW) boxW = tw;
	}

	renderer->DrawRectPx((float)(w - boxW - 46), 18.0f, (float)(boxW + 28), 96.0f, kHudPanel, 0.38f);
	for (int i = 0; i < 4; ++i)
		renderer->DrawTexts(w - boxW - 32, 40 + i * 20, lines[i], i == 0 ? kHudInk : kHudDim, false);

	DrawCommonHud("WASD move    SPACE roll    T time    ESC quit");
	DrawTitleCard("ROUTE 32", "the road beyond the village");
}
