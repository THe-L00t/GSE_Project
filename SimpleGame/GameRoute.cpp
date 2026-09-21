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

	const int kGuideMites = 3;
}

void Game::StartRoute()
{
	level = LEVEL_ROUTE;
	levelTimer = 0.0f;
	endingTimer = -1.0f;
	letterOpen = false;
	prompt.clear();

	levelNode->Destroy();
	levelNode = worldNode->AddChild(new Actor(ACTOR_NODE));
	water = nullptr;
	letter = nullptr;
	villageExit = nullptr;
	targetSleeper = nullptr;

	uint64_t seed = kRouteSeed != 0 ? kRouteSeed : MixSeed((uint64_t)std::time(nullptr));
	routeMap.Reset(seed);
	chunkStates.clear();
	enemies.clear();
	worldItems.clear();
	popups.clear();

	safePoint = Vec3(0.0f, 0.0f, -7.0f);
	lanternPos = Vec3(2.5f, 0.0f, -8.5f);

	player->SetPosition(safePoint);
	player->SetYaw(0.0f);
	player->rollTimer = 0.0f;
	player->rollAngle = 0.0f;
	camera->SetPosition(player->Position());
	health = MaxHealth(stats);
	deathTimer = -1.0f;
	sporeExposure = Minf(sporeExposure, 0.3f);

	if (!player->weapon) DropItem(ITEM_RUSTY_PIPE, Vec3(0.5f, 0.0f, -3.0f), 0, 0, -1);
	SetGuide(GUIDE_TAKE_PIPE);

	StreamChunks();
	UpdateChunkActivation();
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
		Vec3 pos = player->Position();
		ResolveRouteCollisions(pos, player->collider.radius);
		player->SetPosition(pos);
	}

	StreamChunks();
	UpdateChunkActivation();
	UpdateEnemies(dt);
	UpdateItems();
	UpdateCombatTimers(dt);
	UpdateGuide();

	// Deeper, more overgrown chunks carry thicker spores. Insight and dream fragments slow it.
	const Chunk& here = routeMap.Get(playerChunkX, playerChunkZ);
	float fragmentGuard = Maxf(1.0f - 0.18f * (float)fragments, 0.3f);
	float rate = 0.012f * (float)here.stage * fragmentGuard * SporeResistance(stats);
	if (player->rollTimer > 0.0f) rate = 0.0f;

	// The lantern clears the air and mends wounds.
	if (DistXZ(player->Position(), lanternPos) < kLanternRadius && deathTimer < 0.0f)
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

void Game::SetGuide(int next)
{
	guide = next;

	switch (next)
	{
	case GUIDE_TAKE_PIPE:
		ShowMessage("Something glints on the road ahead. Walk over it to pick it up.", 5.0f);
		break;

	case GUIDE_FIGHT_MITES:
		guideBaseline = kills[ENEMY_SPORE_MITE];
		SpawnEnemy(ENEMY_SPORE_MITE, 1, Vec3(-3.5f, 0.0f, 3.0f), 0, 0, -1);
		SpawnEnemy(ENEMY_SPORE_MITE, 1, Vec3(3.5f, 0.0f, 4.5f), 0, 0, -1);
		SpawnEnemy(ENEMY_SPORE_MITE, 1, Vec3(0.0f, 0.0f, 8.0f), 0, 0, -1);
		ShowMessage("Spore mites are stirring. Attack with J or the left mouse button.", 5.0f);
		break;

	case GUIDE_ASSIGN_STATS:
		guideBaseline = statConfirmations;
		ShowMessage("Level up: 3 stat points and full health. Press C to spend the points.", 6.0f);
		break;

	case GUIDE_FIGHT_BOAR:
		guideBaseline = kills[ENEMY_MOSS_BOAR];
		SpawnEnemy(ENEMY_MOSS_BOAR, 1, Vec3(RoadCenter(22.0f), 0.0f, 22.0f), 0, 1, -1);
		if (inventory[ITEM_HERB] == 0)
			DropItem(ITEM_HERB, player->Position() + Vec3(1.5f, 0.0f, 1.0f), 0, 0, -1);
		ShowMessage("A moss boar blocks the road south. When it glows red, roll through the charge.", 6.0f);
		break;

	case GUIDE_ASSIGN_AGAIN:
		guideBaseline = statConfirmations;
		ShowMessage("Level up again. Try putting these points somewhere new.", 5.0f);
		break;

	case GUIDE_EXPLORE:
		ShowMessage("The road opens. Every step grows new land from the ground behind you.", 6.0f);
		break;

	default:
		break;
	}
}

void Game::UpdateGuide()
{
	// Each step also clears when its goal is already met, so no order of play can stall it.
	switch (guide)
	{
	case GUIDE_TAKE_PIPE:
		if (player->weapon) SetGuide(GUIDE_FIGHT_MITES);
		break;

	case GUIDE_FIGHT_MITES:
		if (kills[ENEMY_SPORE_MITE] - guideBaseline >= kGuideMites)
			SetGuide(stats.unspentPoints > 0 ? GUIDE_ASSIGN_STATS : GUIDE_FIGHT_BOAR);
		break;

	case GUIDE_ASSIGN_STATS:
		if (statConfirmations > guideBaseline || stats.unspentPoints == 0) SetGuide(GUIDE_FIGHT_BOAR);
		break;

	case GUIDE_FIGHT_BOAR:
		if (kills[ENEMY_MOSS_BOAR] - guideBaseline >= 1)
			SetGuide(stats.unspentPoints > 0 ? GUIDE_ASSIGN_AGAIN : GUIDE_EXPLORE);
		break;

	case GUIDE_ASSIGN_AGAIN:
		if (statConfirmations > guideBaseline || stats.unspentPoints == 0) SetGuide(GUIDE_EXPLORE);
		break;

	default:
		break;
	}
}

void Game::GuideText(char* buf, size_t size) const
{
	switch (guide)
	{
	case GUIDE_TAKE_PIPE:
		sprintf_s(buf, size, "Take the rusty pipe lying on the road.");
		break;
	case GUIDE_FIGHT_MITES:
		sprintf_s(buf, size, "Defeat the spore mites (%d/%d).  J or click to attack.",
				  kills[ENEMY_SPORE_MITE] - guideBaseline, kGuideMites);
		break;
	case GUIDE_ASSIGN_STATS:
		sprintf_s(buf, size, "You reached level %d. Press C and assign your stat points.", stats.level);
		break;
	case GUIDE_FIGHT_BOAR:
		sprintf_s(buf, size, "Defeat the moss boar to the south. SPACE rolls through its charge.");
		break;
	case GUIDE_ASSIGN_AGAIN:
		sprintf_s(buf, size, "Level %d. Spend the new points - try a different stat.", stats.level);
		break;
	default:
		sprintf_s(buf, size, "Walk Route 32. The land grows in every direction.");
		break;
	}
}

void Game::StreamChunks()
{
	ChunkMap::ChunkCoords(player->Position(), playerChunkX, playerChunkZ);

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
	ChunkMap::ChunkCoords(camera->Position(), cx, cz);

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

	char objective[128];
	GuideText(objective, sizeof(objective));
	DrawObjective(objective);

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

	DrawGuideCard();
	DrawCombatHud();
	DrawPopups();
	DrawCommonHud("WASD move   SPACE roll   J/Click attack   Q herb   R water   C stats   ESC quit");
	DrawTitleCard("ROUTE 32", "where the road teaches you to grow");
	DrawStatPanel();
}

void Game::DrawGuideCard()
{
	if (guide < GUIDE_ASSIGN_STATS || guide > GUIDE_ASSIGN_AGAIN) return;

	const int w = renderer->GetWidth();

	const char* lines[] =
	{
		"HOW YOU GROW",
		"Creatures and relics give XP.",
		"A full XP bar raises your level:",
		"  +3 stat points and full health.",
		"Each level asks for more XP.",
		"C opens stats; place points freely.",
	};
	const int lineCount = (int)(sizeof(lines) / sizeof(lines[0]));

	int boxW = 0;
	for (int i = 0; i < lineCount; ++i)
	{
		int tw = renderer->TextWidth(lines[i], false);
		if (tw > boxW) boxW = tw;
	}

	float x = (float)(w - boxW - 46);
	float y = 128.0f;
	renderer->DrawRectPx(x, y, (float)(boxW + 28), 24.0f + 20.0f * (float)lineCount, kHudPanel, 0.45f);
	for (int i = 0; i < lineCount; ++i)
		renderer->DrawTexts((int)x + 14, (int)y + 26 + i * 20, lines[i], i == 0 ? kHudAccent : kHudInk, false);
}
