#include "stdafx.h"
#include "Game.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "Collision.h"
#include "Models.h"
#include "Random.h"

namespace
{
	// 0 grows a different Route 32 every run; any other value replays that world.
	const uint64_t kRouteSeed = 0;

	const int kStreamRadius = 2;   // chunks kept generated around the player
	const int kSimRadius = 1;      // chunks whose creatures and finds are live
	const int kDrawRadius = 1;     // chunks drawn around the camera; covers the orthographic view

	const float kLanternRadius = 4.5f;
	const float kSleepDuration = 2.2f;
	const float kSporeDrain = 4.0f;  // health per second once exposure is full
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
	chunkStates.clear();
	enemies.clear();
	worldItems.clear();
	popups.clear();

	safePoint = Vec3(0.0f, 0.0f, -7.0f);
	lanternPos = Vec3(2.5f, 0.0f, -8.5f);

	playerPos = safePoint;
	playerYaw = 0.0f;
	rollTimer = 0.0f;
	rollAngle = 0.0f;
	camTarget = playerPos;
	health = MaxHealth(stats);
	deathTimer = -1.0f;
	sporeExposure = Minf(sporeExposure, 0.3f);

	if (!hasWeapon) DropItem(ITEM_RUSTY_PIPE, Vec3(0.5f, 0.0f, -3.0f), 0, 0, -1);

	StreamChunks();
	UpdateChunkActivation();
	ShowMessage("The road runs on. Walk any way you like; the land grows as you go.", 5.0f);
}

void Game::UpdateRoute(float dt, const bool* keys)
{
	if (deathTimer >= 0.0f)
	{
		deathTimer += dt;
		if (deathTimer > kSleepDuration) WakeAtSafePoint();
	}
	else
	{
		UpdatePlayer(dt, keys);
		ResolveRouteCollisions(playerPos, kPlayerRadius);
	}

	StreamChunks();
	UpdateChunkActivation();
	UpdateEnemies(dt);
	UpdateItems();
	UpdateCombatTimers(dt);

	// Deeper, more overgrown chunks carry thicker spores. Insight and dream fragments slow it.
	const Chunk& here = routeMap.Get(playerChunkX, playerChunkZ);
	float fragmentGuard = Maxf(1.0f - 0.18f * (float)fragments, 0.3f);
	float rate = 0.012f * (float)here.stage * fragmentGuard * SporeResistance(stats);
	if (rollTimer > 0.0f) rate = 0.0f;

	// The lantern clears the air and mends wounds.
	if (DistXZ(playerPos, lanternPos) < kLanternRadius && deathTimer < 0.0f)
	{
		rate = -0.08f;
		health = Minf(health + 3.0f * dt, MaxHealth(stats));
	}

	sporeExposure = Saturatef(sporeExposure + rate * dt);

	if (sporeExposure >= 1.0f && deathTimer < 0.0f)
	{
		health -= kSporeDrain * dt;
		if (health <= 0.0f) FallAsleep("The spores close over you. You sink into a long sleep...");
	}
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

void Game::UpdateChunkActivation()
{
	// A chunk drops out of the simulation a ring beyond the live area; its living creatures
	// and untaken finds return when it comes back.
	for (std::unordered_map<uint64_t, ChunkState>::iterator it = chunkStates.begin(); it != chunkStates.end(); ++it)
	{
		ChunkState& state = it->second;
		if (!state.active) continue;
		if (abs(state.cx - playerChunkX) > kSimRadius + 1 || abs(state.cz - playerChunkZ) > kSimRadius + 1)
			DeactivateChunk(state);
	}

	for (int dz = -kSimRadius; dz <= kSimRadius; ++dz)
	{
		for (int dx = -kSimRadius; dx <= kSimRadius; ++dx)
			ActivateChunk(playerChunkX + dx, playerChunkZ + dz);
	}
}

void Game::ActivateChunk(int cx, int cz)
{
	const Chunk& chunk = routeMap.Get(cx, cz);
	ChunkState& state = chunkStates[ChunkMap::Key(cx, cz)];
	if (state.active) return;

	state.active = true;
	state.cx = cx;
	state.cz = cz;
	if (state.respawnAt.size() != chunk.spawns.size()) state.respawnAt.assign(chunk.spawns.size(), 0.0f);
	if (state.itemTaken.size() != chunk.items.size()) state.itemTaken.assign(chunk.items.size(), false);

	for (size_t i = 0; i < chunk.spawns.size(); ++i)
	{
		if (state.respawnAt[i] > time) continue;
		SpawnEnemy(chunk.spawns[i].enemyType, chunk.spawns[i].level, chunk.spawns[i].pos, cx, cz, (int)i);
	}

	for (size_t i = 0; i < chunk.items.size(); ++i)
	{
		if (state.itemTaken[i]) continue;
		DropItem(chunk.items[i].itemType, chunk.items[i].pos, cx, cz, (int)i);
	}
}

void Game::DeactivateChunk(ChunkState& state)
{
	state.active = false;
	int cx = state.cx;
	int cz = state.cz;

	enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
		[cx, cz](const Enemy& e) { return e.spawnIndex >= 0 && e.chunkX == cx && e.chunkZ == cz; }), enemies.end());

	worldItems.erase(std::remove_if(worldItems.begin(), worldItems.end(),
		[cx, cz](const WorldItem& item) { return item.spawnIndex >= 0 && item.chunkX == cx && item.chunkZ == cz; }), worldItems.end());
}

void Game::ResolveRouteCollisions(Vec3& pos, float radius)
{
	int cx, cz;
	ChunkMap::ChunkCoords(pos, cx, cz);

	for (int dz = -1; dz <= 1; ++dz)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			const Chunk& c = routeMap.Get(cx + dx, cz + dz);
			for (size_t i = 0; i < c.props.size(); ++i)
			{
				if (c.props[i].radius > 0.0f)
					PushOutOfCircle(pos, radius, c.props[i].pos, c.props[i].radius);
			}
		}
	}

	PushOutOfCircle(pos, radius, lanternPos, 0.5f);
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

	DrawParams lantern;
	lantern.emissive = 0.4f;
	renderer->DrawShadow(lanternPos, 0.6f);
	renderer->DrawModel(MODEL_LANTERN, lanternPos, 0.3f, Vec3(1.0f, 1.0f, 1.0f), lantern);

	DrawItems();
	DrawEnemies();
}

void Game::DrawRouteHud()
{
	const int w = renderer->GetWidth();

	DrawObjective("Walk Route 32. Fight, gather, and grow stronger.");

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

	DrawCombatHud();
	DrawPopups();
	DrawCommonHud("WASD move   SPACE roll   J/Click attack   Q herb   R water   C stats   ESC quit");
	DrawTitleCard("ROUTE 32", "the road beyond the village");
	DrawStatPanel();
}
