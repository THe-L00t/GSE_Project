#include "stdafx.h"
#include "Game.h"

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

	const float kSleepDuration = 2.2f;
	const float kSporeDrain = 4.0f;  // health per second once exposure is full

	const int kGuideMites = 3;

	const float kBushRegrow = 180.0f;   // seconds until a picked bush fruits again

	// Share of the bushes that fruit in each season; under the snow only a few hold on.
	const float kBushShare[SEASON_COUNT] = { 0.6f, 1.0f, 0.9f, 0.15f };
}

void Game::StartRoute()
{
	level = LEVEL_ROUTE;
	levelTimer = 0.0f;
	transitionTimer = -1.0f;
	sleepTimer = -1.0f;
	dayCardTimer = -1.0f;
	routeIntroHaze = 1.0f;
	sporeVisual = 0.55f;
	timeScale = 1.0f;
	tutorial = TUT_DONE;
	letterOpen = false;
	packOpen = false;
	survivalShown = true;
	dialogLines.clear();
	dialogIndex = 0;
	dialogNext = -1;
	prompt.clear();

	levelNode->Destroy();
	water = nullptr;
	letter = nullptr;
	villageExit = nullptr;
	grandmaNpc = nullptr;
	grandmaSleeper = nullptr;
	deer = nullptr;
	targetSleeper = nullptr;
	targetForage = nullptr;
	targetSpot = SPOT_NONE;

	// Chunks first and creatures last: the order they are drawn in.
	levelNode = worldNode->AddChild(new Actor(ACTOR_NODE));
	chunkGroup = levelNode->AddChild(new Actor(ACTOR_NODE));
	lantern = levelNode->AddChild(new LanternActor());
	lantern->SetPosition(Vec3(2.5f, 0.0f, -8.5f));
	safePoint = levelNode->AddChild(new Actor(ACTOR_SAFE_POINT));
	safePoint->SetPosition(Vec3(0.0f, 0.0f, -7.0f));
	itemGroup = levelNode->AddChild(new Actor(ACTOR_NODE));
	enemyGroup = levelNode->AddChild(new Actor(ACTOR_NODE));
	forageGroup = levelNode->AddChild(new Actor(ACTOR_NODE));

	reservoir = levelNode->AddChild(new WaterActor(kReservoirSizeX, kReservoirSizeZ));
	reservoir->SetPosition(Vec3(kReservoirCenter.x, 0.03f, kReservoirCenter.z));
	reservoirFound = false;
	townReached = false;
	townCardTimer = -1.0f;
	deerLeaveTimer = -1.0f;
	routeTime = 0.0f;
	warmthActive = false;

	// The finds the road is built around. They wait however long the player takes.
	if (!relicFound[RELIC_PEACHES])
	{
		ItemActor* can = DropItem(ITEM_CANNED_FOOD, Vec3(RoadCenter(kBusStopZ) - 3.7f, 0.0f, kBusStopZ), 0, 0, -1);
		can->landmark = true;
	}
	// Husks queue at the shelter, facing the road, for a bus that never came back.
	busQueueSeen = false;
	for (int i = 0; i < 3; ++i)
	{
		float z = kBusStopZ - 2.4f - 1.5f * (float)i;
		EnemyActor* husk = SpawnEnemy(ENEMY_HUSK, 1, Vec3(RoadCenter(z) - 4.6f, 0.0f, z), 0, kBusStopChunkZ, -1);
		husk->holdsPlace = true;
		husk->homeYaw = kPi * 0.5f;
		husk->SetYaw(husk->homeYaw);
	}

	if (!relicFound[RELIC_BADGE])
	{
		ItemActor* badge = DropItem(ITEM_RELIC, Vec3(RoadCenter(kTownGateZ) + 8.5f, 0.0f, kTownGateZ + 4.0f), 0, 0, -1);
		badge->relicId = RELIC_BADGE;
		badge->landmark = true;
	}

	uint64_t seed = kRouteSeed != 0 ? kRouteSeed : MixSeed((uint64_t)std::time(nullptr));
	routeMap.Reset(seed);
	chunkActors.clear();
	chunkStates.clear();
	popups.clear();
	wisps.clear();
	footprints.clear();

	player->SetPosition(safePoint->Position());
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
	routeIntroHaze = Maxf(routeIntroHaze - dt / 4.0f, 0.0f);

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
	UpdateLandmarks(dt);
	UpdateRouteForage();
	UpdateRouteDeer(dt);
	UpdateGuide();
	UpdateStory(dt);

	// Deeper, more overgrown chunks carry thicker spores. Nature Insight slows it; thirst speeds it up.
	const ChunkActor* here = EnsureChunk(playerChunkX, playerChunkZ);
	float rate = 0.0084f * (float)here->stage * NatureGuard(natureInsight) * (waterMeter <= 0.0f ? 1.6f : 1.0f);
	if (player->rollTimer > 0.0f) rate = 0.0f;

	// Rain washes the spores out of the air, and a gale scatters them.
	rate *= (1.0f - 0.7f * rainLevel) * (1.0f - 0.3f * windLevel);

	// Open water clears the air, as the reservoir did at home.
	if (reservoir->ShoreDistance(player->Position()) < reservoir->clearRange) rate = -0.075f;

	// The lantern clears the air and mends wounds.
	if (DistXZ(player->Position(), lantern->WorldPosition()) < lantern->zoneRadius && deathTimer < 0.0f)
	{
		rate = -0.08f;
		health = Minf(health + 3.0f * dt, MaxHealth(stats));
	}

	sporeExposure = Saturatef(sporeExposure + rate * dt);

	if (sporeExposure >= 1.0f && deathTimer < 0.0f)
	{
		health -= kSporeDrain * dt;
		if (health <= 0.0f) FallAsleep("포자가 온몸을 덮는다. 긴 잠에 빠져든다...");
	}
}

void Game::SetGuide(int next)
{
	guide = next;

	switch (next)
	{
	case GUIDE_TAKE_PIPE:
		ShowMessage("길 앞에서 뭔가 반짝인다. 위로 걸어가면 줍는다.", 5.0f);
		break;

	case GUIDE_FIGHT_MITES:
		guideBaseline = kills[ENEMY_SPORE_MITE];
		SpawnEnemy(ENEMY_SPORE_MITE, 1, Vec3(-3.5f, 0.0f, 3.0f), 0, 0, -1);
		SpawnEnemy(ENEMY_SPORE_MITE, 1, Vec3(3.5f, 0.0f, 4.5f), 0, 0, -1);
		SpawnEnemy(ENEMY_SPORE_MITE, 1, Vec3(0.0f, 0.0f, 8.0f), 0, 0, -1);
		ShowMessage("포자 진드기들이 꿈틀거린다. J 또는 마우스 왼쪽 버튼으로 공격하라.", 5.0f);
		break;

	case GUIDE_ASSIGN_STATS:
		guideBaseline = statConfirmations;
		ShowMessage("레벨 업: 능력치 3점과 체력 회복. C를 눌러 점수를 써라.", 6.0f);
		break;

	case GUIDE_FIGHT_BOAR:
		guideBaseline = kills[ENEMY_MOSS_BOAR];
		SpawnEnemy(ENEMY_MOSS_BOAR, 1, Vec3(RoadCenter(22.0f), 0.0f, 22.0f), 0, 1, -1);
		if (inventory[ITEM_HERB] == 0)
			DropItem(ITEM_HERB, player->Position() + Vec3(1.5f, 0.0f, 1.0f), 0, 0, -1);
		ShowMessage("이끼 멧돼지가 남쪽 길을 막고 있다. 붉게 빛나면 구르기로 돌진을 피하라.", 6.0f);
		break;

	case GUIDE_ASSIGN_AGAIN:
		guideBaseline = statConfirmations;
		ShowMessage("또 레벨 업. 이번 점수는 다른 곳에 넣어 보라.", 5.0f);
		break;

	case GUIDE_BUS_STOP:
		ShowMessage("길 아래 버스 정류장 지붕 밑에 뭔가 있다.", 5.0f);
		break;

	case GUIDE_FIND_WATER:
		ShowMessage("물병이 오래가지 않는다. 할머니는 등불 사슴이 물 있는 곳을 안다고 했다.", 6.0f);
		break;

	case GUIDE_REACH_TOWN:
		ShowMessage("은행나무 읍내는 더 남쪽에 있다. 남은 길을 따라가라.", 5.0f);
		break;

	case GUIDE_EXPLORE:
		ShowMessage("길은 계속된다. 멀리 걸을수록 옛 세상은 덜 남아 있다.", 6.0f);
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
			SetGuide(stats.unspentPoints > 0 ? GUIDE_ASSIGN_AGAIN : GUIDE_BUS_STOP);
		break;

	case GUIDE_ASSIGN_AGAIN:
		if (statConfirmations > guideBaseline || stats.unspentPoints == 0) SetGuide(GUIDE_BUS_STOP);
		break;

	case GUIDE_BUS_STOP:
		if (relicFound[RELIC_PEACHES] || reservoirFound || townReached) SetGuide(GUIDE_FIND_WATER);
		break;

	case GUIDE_FIND_WATER:
		if (reservoirFound || townReached) SetGuide(GUIDE_REACH_TOWN);
		break;

	case GUIDE_REACH_TOWN:
		if (townReached) SetGuide(GUIDE_EXPLORE);
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
		sprintf_s(buf, size, "길에 떨어진 녹슨 파이프를 주워라.");
		break;
	case GUIDE_FIGHT_MITES:
		sprintf_s(buf, size, "포자 진드기를 쳐서 풀어 주어라 (%d/%d).  J 또는 클릭으로 공격.",
				  kills[ENEMY_SPORE_MITE] - guideBaseline, kGuideMites);
		break;
	case GUIDE_ASSIGN_STATS:
		sprintf_s(buf, size, "레벨 %d에 올랐다. C를 눌러 능력치 점수를 분배하라.", stats.level);
		break;
	case GUIDE_FIGHT_BOAR:
		sprintf_s(buf, size, "남쪽 길을 막은 이끼 멧돼지를 풀어 주어라. SPACE로 구르면 돌진을 피한다.");
		break;
	case GUIDE_ASSIGN_AGAIN:
		sprintf_s(buf, size, "레벨 %d. 새 점수를 써라 - 다른 능력치도 올려 보라.", stats.level);
		break;
	case GUIDE_BUS_STOP:
		sprintf_s(buf, size, "길 아래쪽 버스 정류장을 살펴라.");
		break;
	case GUIDE_FIND_WATER:
		if (timeOfDay < 0.22f || timeOfDay > 0.78f)
			sprintf_s(buf, size, "물을 찾아라. 등불 사슴이 나타나면 따라가라.");
		else
			sprintf_s(buf, size, "물을 찾아라. 등불 사슴은 어두워지면 나온다 (T로 시간 빠르게).");
		break;
	case GUIDE_REACH_TOWN:
		sprintf_s(buf, size, "길을 따라 남쪽의 은행나무 읍내에 닿아라.");
		break;
	default:
		sprintf_s(buf, size, "계속 걸어라. 땅은 사방으로 자라며 옛 세상을 잊어 간다.");
		break;
	}
}

void Game::StreamChunks()
{
	ChunkMap::ChunkCoords(player->Position(), playerChunkX, playerChunkZ);

	for (int dz = -kStreamRadius; dz <= kStreamRadius; ++dz)
	{
		for (int dx = -kStreamRadius; dx <= kStreamRadius; ++dx)
			EnsureChunk(playerChunkX + dx, playerChunkZ + dz);
	}

	// Chunk actors nothing looked up last update go; the map rebuilds them when they are needed again.
	// Creatures far from the player keep the chunks around them alive.
	for (std::unordered_map<uint64_t, ChunkActor*>::iterator it = chunkActors.begin(); it != chunkActors.end();)
	{
		ChunkActor* chunk = it->second;
		if (chunk->lastTick < tick - 1)
		{
			chunk->Destroy();
			it = chunkActors.erase(it);
		}
		else
		{
			++it;
		}
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
	if (state.swarmCleared.size() != chunk.swarms.size()) state.swarmCleared.assign(chunk.swarms.size(), false);
	if (state.bushRegrowAt.size() != chunk.bushes.size()) state.bushRegrowAt.assign(chunk.bushes.size(), 0.0f);

	for (size_t i = 0; i < chunk.bushes.size(); ++i)
	{
		const ChunkBush& b = chunk.bushes[i];
		ForageActor* f = forageGroup->AddChild(new ForageActor(b.pale ? FORAGE_PALE_BERRY : FORAGE_RED_BERRY));
		f->SetPosition(b.pos);
		f->SetYaw(b.yaw);
		f->SetScale(Vec3(1.1f, 1.1f, 1.1f));
		f->phase = b.pos.x * 0.37f + b.pos.z * 0.21f;
		f->chunkX = cx;
		f->chunkZ = cz;
		f->spawnIndex = (int)i;
		f->bearing = b.bearing;
	}

	for (size_t i = 0; i < chunk.spawns.size(); ++i)
	{
		if (state.respawnAt[i] > time) continue;
		const ChunkSpawn& spawn = chunk.spawns[i];
		EnemyActor* e = SpawnEnemy(spawn.enemyType, spawn.level, spawn.pos, cx, cz, (int)i);
		e->swarm = spawn.swarm;
		if (spawn.guardOf >= 0)
		{
			// The watcher lies still beside the one it keeps, facing them.
			const Vec3& keep = chunk.sleepers[spawn.guardOf].pos;
			e->guarding = true;
			e->guardPos = keep;
			e->holdsPlace = true;
			e->homeYaw = atan2f(keep.x - spawn.pos.x, keep.z - spawn.pos.z);
			e->SetYaw(e->homeYaw);
		}
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

	std::vector<EnemyActor*> enemies = LiveEnemies();
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		EnemyActor* e = enemies[i];
		if (e->spawnIndex >= 0 && e->chunkX == state.cx && e->chunkZ == state.cz) e->Destroy();
	}

	std::vector<ItemActor*> items = LiveItems();
	for (size_t i = 0; i < items.size(); ++i)
	{
		ItemActor* item = items[i];
		if (item->spawnIndex >= 0 && item->chunkX == state.cx && item->chunkZ == state.cz) item->Destroy();
	}

	std::vector<ForageActor*> bushes;
	SceneGraph::Collect(forageGroup, ACTOR_FORAGE, bushes);
	for (size_t i = 0; i < bushes.size(); ++i)
	{
		if (bushes[i]->chunkX == state.cx && bushes[i]->chunkZ == state.cz) bushes[i]->Destroy();
	}
}

ChunkActor* Game::EnsureChunk(int cx, int cz)
{
	uint64_t key = ChunkMap::Key(cx, cz);
	std::unordered_map<uint64_t, ChunkActor*>::iterator found = chunkActors.find(key);
	if (found != chunkActors.end())
	{
		found->second->lastTick = tick;
		return found->second;
	}

	ChunkActor* chunk = chunkGroup->AddChild(new ChunkActor(routeMap.Get(cx, cz)));
	chunk->lastTick = tick;
	chunkActors[key] = chunk;
	ShareSleeperPose(*chunk);
	return chunk;
}

void Game::PrepareChunkView()
{
	for (std::unordered_map<uint64_t, ChunkActor*>::iterator it = chunkActors.begin(); it != chunkActors.end(); ++it)
		it->second->inView = false;

	int cx, cz;
	ChunkMap::ChunkCoords(camera->Position(), cx, cz);

	for (int dz = -kChunkViewRange; dz <= kChunkViewRange; ++dz)
	{
		for (int dx = -kChunkViewRange; dx <= kChunkViewRange; ++dx)
		{
			// Only a chunk that could show is built; then its own props decide.
			if (!ChunkInView(cx + dx, cz + dz, kChunkPropReach, kChunkPropTop)) continue;

			ChunkActor* c = EnsureChunk(cx + dx, cz + dz);
			c->inView = ChunkInView(c->cx, c->cz, c->reach, c->top);
			if (!c->inView || c->hasNeighborStages) continue;

			// The ground blends toward each neighbour's stage, so a chunk needs them before it is drawn.
			c->neighborStage[0] = (float)routeMap.Get(c->cx - 1, c->cz).stage;
			c->neighborStage[1] = (float)routeMap.Get(c->cx + 1, c->cz).stage;
			c->neighborStage[2] = (float)routeMap.Get(c->cx, c->cz - 1).stage;
			c->neighborStage[3] = (float)routeMap.Get(c->cx, c->cz + 1).stage;
			c->hasNeighborStages = true;
		}
	}
}

bool Game::ChunkInView(int cx, int cz, float reach, float top) const
{
	// The orthographic view shows a rectangle of ground, measured across the screen and away from the eye.
	// Anything standing up also shows from below the bottom edge, as far as its height lifts it on screen.
	float yaw = camera->Yaw();
	Vec3 away(-sinf(yaw), 0.0f, -cosf(yaw));
	Vec3 across(cosf(yaw), 0.0f, -sinf(yaw));
	float aspect = (float)renderer->GetWidth() / Maxf((float)renderer->GetHeight(), 1.0f);
	float hh = camera->orthoHeight * 0.5f;
	float halfAcross = hh * aspect + 1.0f;
	float farEdge = hh / sinf(camera->elevation) + 1.0f;
	float nearEdge = -(hh / sinf(camera->elevation) + top / tanf(camera->elevation) + 1.0f);

	// The chunk square, grown by its props' spread, against that rectangle: separating axes are the
	// rectangle's two and the world's two.
	Vec3 c = ChunkMap::ChunkCenter(cx, cz) - camera->Position();
	float half = kChunkSize * 0.5f + reach;

	float onAcross = c.x * across.x + c.z * across.z;
	float onAway = c.x * away.x + c.z * away.z;
	float spanAcross = half * (fabsf(across.x) + fabsf(across.z));
	float spanAway = half * (fabsf(away.x) + fabsf(away.z));
	if (onAcross + spanAcross < -halfAcross || onAcross - spanAcross > halfAcross) return false;
	if (onAway + spanAway < nearEdge || onAway - spanAway > farEdge) return false;

	float mid = (farEdge + nearEdge) * 0.5f;
	float halfAway = (farEdge - nearEdge) * 0.5f;
	float spanX = halfAcross * fabsf(across.x) + halfAway * fabsf(away.x);
	float spanZ = halfAcross * fabsf(across.z) + halfAway * fabsf(away.z);
	if (fabsf(c.x - away.x * mid) > half + spanX) return false;
	if (fabsf(c.z - away.z * mid) > half + spanZ) return false;
	return true;
}

void Game::ResolveRouteCollisions(Vec3& pos, float radius)
{
	int cx, cz;
	ChunkMap::ChunkCoords(pos, cx, cz);

	for (int dz = -1; dz <= 1; ++dz)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			const ChunkActor* c = EnsureChunk(cx + dx, cz + dz);
			for (size_t i = 0; i < c->ChildCount(); ++i)
			{
				const Actor* prop = c->Child(i);
				if (!prop->IsDestroyed() && prop->collider.shape == COLLIDER_CIRCLE)
					PushOutOfCircle(pos, radius, prop->WorldPosition(), prop->collider.radius);
			}
		}
	}

	PushOutOfCircle(pos, radius, lantern->WorldPosition(), lantern->collider.radius);
	PushOutOfBox(pos, radius, reservoir->WorldPosition(), reservoir->collider.halfX, reservoir->collider.halfZ);
}

void Game::UpdateLandmarks(float dt)
{
	const Vec3& pos = player->Position();

	if (!busQueueSeen && DistXZ(pos, Vec3(RoadCenter(kBusStopZ) - 4.6f, 0.0f, kBusStopZ - 4.0f)) < 14.0f)
	{
		busQueueSeen = true;
		ShowMessage("정류장 앞에 허물들이 줄을 서 있다. 오지 않는 버스를 아직 기다리는 것처럼.", 5.0f);
		MarkMemo(MEMO_BUS_STOP);
	}

	if (!reservoirFound && reservoir->ShoreDistance(pos) < 3.0f)
	{
		reservoirFound = true;
		AddInsight(1, "땅이 움푹 꺼진 곳에 물이 숨어 있었다.");
		MarkMemo(MEMO_RESERVOIR);
	}

	if (!townReached && pos.z > kTownGateZ && fabsf(pos.x - RoadCenter(pos.z)) < 16.0f)
	{
		townReached = true;
		townCardTimer = 0.0f;
		ShowMessage("은행나무 읍내에 닿았다. 원한다면, 길은 계속된다.", 6.0f);
		MarkMemo(MEMO_TOWN);
	}

	if (townCardTimer >= 0.0f)
	{
		townCardTimer += dt;
		if (townCardTimer > 8.0f) townCardTimer = -1.0f;
	}

	prompt.clear();
	restIndex = -1;
	if (deathTimer >= 0.0f) return;
	if (NearLanternFire())
	{
		char buf[128];
		sprintf_s(buf, sizeof(buf), "[E]  석등 불에 고기 굽기 (%d)", inventory[ITEM_MEAT]);
		prompt = buf;
	}
	else if (reservoir->ShoreDistance(pos) < 1.8f)
	{
		prompt = "[E]  물 마시고 물병 채우기";
	}
	else if (FindRouteSleeper())
	{
		prompt = "[E]  잠든 이 곁에서 쉬기";
	}
}

void Game::UpdateRouteForage()
{
	targetForage = nullptr;

	std::vector<ForageActor*> bushes;
	SceneGraph::Collect(forageGroup, ACTOR_FORAGE, bushes);

	float best = kInteractRange;
	for (size_t i = 0; i < bushes.size(); ++i)
	{
		ForageActor& f = *bushes[i];
		const ChunkState& state = chunkStates[ChunkMap::Key(f.chunkX, f.chunkZ)];
		bool regrown = f.spawnIndex < (int)state.bushRegrowAt.size() && state.bushRegrowAt[f.spawnIndex] <= time;
		bool fruiting = regrown && f.bearing < kBushShare[season];

		f.picked = !fruiting;
		f.model = !fruiting ? MODEL_BUSH : (f.forage == FORAGE_PALE_BERRY ? MODEL_BERRY_PALE : MODEL_BERRY_RED);

		float d = DistXZ(player->Position(), f.Position());
		if (fruiting && d < best)
		{
			best = d;
			targetForage = &f;
		}
	}

	// The lantern and the water speak first; a bush only offers itself when nothing else does.
	if (targetForage && deathTimer < 0.0f && prompt.empty())
		prompt = targetForage->forage == FORAGE_PALE_BERRY ? "[E]  허연 열매 살펴보기" : "[E]  빨간 열매 따기";
	else
		targetForage = nullptr;
}

void Game::PickRouteBush(ForageActor& bush)
{
	if (bush.forage == FORAGE_PALE_BERRY)
	{
		ShowMessage("허연 열매다. 할머니 말대로 배가 뒤틀릴 테니 그냥 둔다.", 3.0f);
		return;
	}

	int room = kBagCapacity - BagCount();
	if (room <= 0)
	{
		ShowMessage("배낭이 가득 찼다. 먼저 먹거나 마시거나 써서 비워라.", 2.5f);
		return;
	}

	int take = room < 2 ? room : 2;
	inventory[ITEM_BERRIES] += take;
	PlayPlayerAction("pickup");
	NoteFirstFind(ITEM_BERRIES);
	ChunkState& state = chunkStates[ChunkMap::Key(bush.chunkX, bush.chunkZ)];
	if (bush.spawnIndex < (int)state.bushRegrowAt.size()) state.bushRegrowAt[bush.spawnIndex] = time + kBushRegrow;

	char buf[256];
	sprintf_s(buf, sizeof(buf), "빨간 열매를 %d줌 땄다. 덤불은 몇 분 지나면 다시 열린다.", take);
	ShowMessage(buf, 3.0f);
}

bool Game::NearLanternFire() const
{
	return inventory[ITEM_MEAT] > 0 && DistXZ(player->Position(), lantern->WorldPosition()) < 2.4f;
}

void Game::UpdateRouteDeer(float dt)
{
	bool night = timeOfDay < 0.22f || timeOfDay > 0.78f;

	if (!deer)
	{
		// Only while it is needed: after dark, with the water still unfound.
		if (guide != GUIDE_FIND_WATER || !night || reservoirFound || deathTimer >= 0.0f) return;

		Vec3 toWater(kReservoirCenter.x - player->Position().x, 0.0f, kReservoirCenter.z - player->Position().z);
		float len = Length(toWater);
		Vec3 dir = len > 0.01f ? toWater * (1.0f / len) : Vec3(0.0f, 0.0f, 1.0f);

		deer = levelNode->AddChild(new DeerActor());
		deer->SetPosition(player->Position() + dir * 6.0f);
		deer->SetYaw(atan2f(dir.x, dir.z));
		deer->goal = kReservoirCenter + Vec3(-kReservoirSizeX * 0.5f - 1.5f, 0.0f, 0.0f);
		deer->speed = 2.4f;
		deer->leading = true;
		deer->carriesGrandma = true;
		ShowMessage("등불 사슴이 어둠 속에서 걸어 나와 너를 기다린다. 뿔에 못 보던 따뜻한 불빛이 하나 늘어 있다.", 6.0f);
		return;
	}

	Vec3 pos = deer->Position();

	// Its work done, or the night over, it walks off and is gone.
	if (deerLeaveTimer < 0.0f && (reservoirFound || (!night && !deer->arrived)))
	{
		deerLeaveTimer = 0.0f;
		if (!reservoirFound) ShowMessage("날이 밝자 사슴은 사라졌다.", 3.0f);
	}
	if (deerLeaveTimer >= 0.0f)
	{
		deerLeaveTimer += dt;
		deer->SetPosition(pos + Vec3(1.8f * dt, 0.0f, 0.6f * dt));
		deer->SetYaw(atan2f(1.8f, 0.6f));
		deer->stepPhase += dt * 4.0f;
		if (deerLeaveTimer > 6.0f)
		{
			deer->Destroy();
			deer = nullptr;
			deerLeaveTimer = -1.0f;
		}
		return;
	}

	Vec3 toGoal(deer->goal.x - pos.x, 0.0f, deer->goal.z - pos.z);
	float goalDist = Length(toGoal);
	float playerDist = DistXZ(pos, player->Position());

	if (goalDist < 0.5f)
	{
		deer->arrived = true;
		return;
	}

	// Leading: it keeps just ahead, and when the player falls behind it stops and looks back over its
	// shoulder. Only a deer without that pose turns right round to wait.
	if (playerDist > 9.0f)
	{
		if (deer->anim.GetSkeleton() == nullptr || deer->anim.GetSkeleton()->FindClip("wait") < 0)
		{
			Vec3 toPlayer = player->Position() - pos;
			deer->SetYaw(atan2f(toPlayer.x, toPlayer.z));
		}
		return;
	}

	float step = Minf(deer->speed * dt, goalDist);
	deer->SetPosition(pos + toGoal * (step / goalDist));
	deer->SetYaw(atan2f(toGoal.x, toGoal.z));
	deer->stepPhase += dt * 5.0f;
}

void Game::TryRouteInteract()
{
	if (DialogOpen())
	{
		AdvanceDialog();
		return;
	}
	if (deathTimer >= 0.0f) return;

	if (targetForage)
	{
		PickRouteBush(*targetForage);
		return;
	}

	// The lantern's flame roasts whatever raw meat is in the bag, all at once.
	if (NearLanternFire())
	{
		int count = inventory[ITEM_MEAT];
		inventory[ITEM_COOKED_MEAT] += count;
		inventory[ITEM_MEAT] = 0;

		char buf[256];
		sprintf_s(buf, sizeof(buf), "석등 불에 고기 %d덩이를 굽는다. 기름 냄새가 안개 속으로 번진다.", count);
		ShowMessage(buf, 3.5f);
		return;
	}

	if (restIndex >= 0)
	{
		RestBySleeper();
		return;
	}

	if (reservoir->ShoreDistance(player->Position()) >= 1.8f) return;

	waterMeter = 100.0f;
	int room = kBagCapacity - BagCount();
	int fill = room < 3 ? room : 3;
	if (fill < 0) fill = 0;
	inventory[ITEM_CLEAN_WATER] += fill;

	char buf[256];
	sprintf_s(buf, sizeof(buf), "물을 실컷 마시고 물병 %d개를 채웠다.", fill);
	ShowMessage(buf, 3.0f);
}

void Game::DrawRouteHud()
{
	const int w = renderer->GetWidth();

	char objective[256];
	GuideText(objective, sizeof(objective));
	DrawObjective(objective);

	const ChunkActor* here = EnsureChunk(playerChunkX, playerChunkZ);
	char lines[5][128];
	sprintf_s(lines[0], sizeof(lines[0]), "시드  %016llX", (unsigned long long)routeMap.WorldSeed());
	sprintf_s(lines[1], sizeof(lines[1]), "청크 (%d, %d)  단계 %d", here->cx, here->cz, here->stage);
	sprintf_s(lines[2], sizeof(lines[2]), "청크 해시  %016llX", (unsigned long long)here->hash);
	sprintf_s(lines[3], sizeof(lines[3]), "생성된 청크  %d", routeMap.GeneratedCount());
	sprintf_s(lines[4], sizeof(lines[4]), "남은 옛 세상  %d%%", (int)(here->modernity * 100.0f + 0.5f));

	int boxW = 0;
	for (int i = 0; i < 5; ++i)
	{
		int tw = renderer->TextWidth(lines[i], false);
		if (tw > boxW) boxW = tw;
	}

	renderer->DrawRectPx((float)(w - boxW - 46), 18.0f, (float)(boxW + 28), 116.0f, kHudPanel, 0.38f);
	for (int i = 0; i < 5; ++i)
		renderer->DrawTexts(w - boxW - 32, 40 + i * 20, lines[i], i == 0 ? kHudInk : kHudDim, false);

	DrawGuideCard();
	DrawCombatHud();
	DrawPopups();
	DrawSurvivalHud();
	DrawCommonHud("WASD 이동  SPACE 구르기  J 공격  Q 약초  R 마시기  F 먹기  L 손전등  I 배낭  B 도감  C 능력치  E 사용  F4 옵션");
	DrawTitleCard("32번 국도", "처음 나선 바깥");
	DrawTitleCardAt("은행나무 읍내", "첫 길의 끝", townCardTimer);
	DrawDialog();
	DrawStatPanel();
	DrawBagPanel();
	DrawJournalPanel();
}

void Game::DrawGuideCard()
{
	if (guide < GUIDE_ASSIGN_STATS || guide > GUIDE_ASSIGN_AGAIN) return;

	const int w = renderer->GetWidth();

	const char* lines[] =
	{
		"성장하는 법",
		"짐승과 유물은 경험치를 준다.",
		"경험치가 가득 차면 레벨이 오른다:",
		"  능력치 +3점, 체력 완전 회복.",
		"레벨마다 필요한 경험치가 늘어난다.",
		"C로 능력치 창을 열고 자유롭게 분배하라.",
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
