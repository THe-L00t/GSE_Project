#include "stdafx.h"
#include "Game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "Collision.h"
#include "Models.h"

namespace
{
	const float kBareHandReach = 1.5f;
	const float kHurtTime = 0.6f;
	const float kSleepDuration = 2.2f;
	const float kPickupRange = 1.1f;
	const float kPopupTime = 0.9f;
	const float kRespawnDelay = 90.0f;
	const float kHerbHeal = 0.4f;        // fraction of max health
	const float kLoreRange = 9.0f;
	const float kGuardRange = 3.5f;      // how close to a sleeper wakes the boar beside them
	const float kLeashRange = 22.0f;     // a chase gives up this far from home
	const float kLeashReturn = 16.0f;    // and a creature walking home takes notice again only this close to it

	// Seconds the world freezes on a landed blow. Time-based: one frame at a high frame rate is too short to see.
	const float kHitStop = 0.05f;
	const float kKillStop = 0.09f;

	const Vec3 kXpColor(0.75f, 0.95f, 0.55f);
	const Vec3 kHurtColor(1.0f, 0.45f, 0.40f);
	const Vec3 kHealColor(0.55f, 0.95f, 0.55f);
	const Vec3 kHitColor(1.0f, 0.92f, 0.70f);

	Vec3 FlatDirection(const Vec3& v)
	{
		Vec3 d(v.x, 0.0f, v.z);
		float len = Length(d);
		return len > 1e-4f ? d * (1.0f / len) : Vec3(0.0f, 0.0f, 1.0f);
	}

	void MoveToward(Vec3& pos, float& yaw, const Vec3& target, float speed, float dt)
	{
		Vec3 to(target.x - pos.x, 0.0f, target.z - pos.z);
		float dist = Length(to);
		if (dist < 0.05f) return;

		float step = Minf(speed * dt, dist);
		pos = pos + to * (step / dist);
		yaw = atan2f(to.x, to.z);
	}
}

std::vector<EnemyActor*> Game::LiveEnemies() const
{
	std::vector<EnemyActor*> list;
	if (enemyGroup) SceneGraph::Collect(enemyGroup, ACTOR_ENEMY, list);
	return list;
}

std::vector<ItemActor*> Game::LiveItems() const
{
	std::vector<ItemActor*> list;
	if (itemGroup) SceneGraph::Collect(itemGroup, ACTOR_ITEM, list);
	return list;
}

void Game::Attack()
{
	if (level != LEVEL_ROUTE || statPanelOpen || deathTimer >= 0.0f) return;
	if (player->attackTimer > 0.0f || player->rollTimer > 0.0f) return;

	player->attackTimer = AttackCooldown(stats);
	player->swingTimer = kSwingTime;

	Vec3 playerPos = player->Position();
	Vec3 facing(sinf(player->Yaw()), 0.0f, cosf(player->Yaw()));
	float power = AttackPower(stats, player->weapon ? player->weapon->power : 0.0f) * (foodMeter <= 0.0f ? 0.8f : 1.0f);
	float reach = player->weapon ? player->weapon->reach : kBareHandReach;

	std::vector<EnemyActor*> enemies = LiveEnemies();
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		EnemyActor& e = *enemies[i];
		if (!e.alive) continue;

		Vec3 to(e.Position().x - playerPos.x, 0.0f, e.Position().z - playerPos.z);
		float dist = Length(to);
		if (dist > reach + e.collider.radius) continue;

		// A wide arc in front; point-blank enemies count wherever they stand.
		if (dist > 0.6f && Dot(to * (1.0f / dist), facing) < 0.35f) continue;

		DamageEnemy(e, power * combatRng.Range(0.9f, 1.1f), to);
	}
}

void Game::DamageEnemy(EnemyActor& e, float amount, const Vec3& push)
{
	if (!e.alive) return;

	const EnemyInfo& info = GetEnemyInfo(e.type);
	e.health -= amount;
	e.flash = 1.0f;
	e.provoked = true;
	hitStop = Maxf(hitStop, kHitStop);
	e.knockback = FlatDirection(push) * (e.type == ENEMY_MOSS_BOAR ? 2.5f : 6.0f);

	char text[16];
	sprintf_s(text, sizeof(text), "%d", (int)(amount + 0.5f));
	AddPopup(e.Position() + Vec3(0.0f, 1.2f + info.radius, 0.0f), text, kHitColor);

	// Light creatures lose their windup when struck; the boar's charge cannot be stopped.
	if (e.type != ENEMY_MOSS_BOAR && e.state == ENEMY_WINDUP)
	{
		e.state = ENEMY_RECOVER;
		e.stateTimer = 0.35f;
	}
	else if (e.state == ENEMY_IDLE)
	{
		e.state = ENEMY_CHASE;
	}

	if (e.health <= 0.0f) KillEnemy(e);
}

void Game::KillEnemy(EnemyActor& e)
{
	const EnemyInfo& info = GetEnemyInfo(e.type);
	e.alive = false;
	e.state = ENEMY_DYING;
	e.stateTimer = kDyingTime;
	++kills[e.type];
	hitStop = Maxf(hitStop, kKillStop);

	// What the creature was holding is let go as light. Its own generator keeps the drop rolls unchanged.
	int motes = e.type == ENEMY_MOSS_BOAR ? 16 : 9;
	for (int i = 0; i < motes; ++i)
	{
		float a = fxRng.Range(0.0f, 2.0f * kPi);
		float r = fxRng.Range(0.0f, info.radius);
		Wisp w;
		w.pos = e.Position() + Vec3(cosf(a) * r, fxRng.Range(0.2f, 1.0f), sinf(a) * r);
		w.vel = Vec3(cosf(a) * 0.4f, fxRng.Range(1.2f, 2.2f), sinf(a) * 0.4f);
		w.maxLife = fxRng.Range(1.1f, 1.9f);
		w.life = w.maxLife;
		wisps.push_back(w);
	}

	if (e.spawnIndex >= 0)
	{
		ChunkState& state = chunkStates[ChunkMap::Key(e.chunkX, e.chunkZ)];
		if (e.spawnIndex < (int)state.respawnAt.size())
			state.respawnAt[e.spawnIndex] = time + kRespawnDelay;
	}

	// The last mite off a car lays open what it was crowding: something left inside.
	if (e.swarm >= 0 && e.spawnIndex >= 0)
	{
		ChunkState& state = chunkStates[ChunkMap::Key(e.chunkX, e.chunkZ)];
		bool others = false;
		std::vector<EnemyActor*> enemies = LiveEnemies();
		for (size_t i = 0; i < enemies.size() && !others; ++i)
		{
			const EnemyActor& o = *enemies[i];
			others = o.alive && o.swarm == e.swarm && o.chunkX == e.chunkX && o.chunkZ == e.chunkZ;
		}

		if (!others && e.swarm < (int)state.swarmCleared.size() && !state.swarmCleared[e.swarm])
		{
			state.swarmCleared[e.swarm] = true;
			const Vec3& car = routeMap.Get(e.chunkX, e.chunkZ).swarms[e.swarm];
			// Someone's road food was still in the glovebox.
			float roll = combatRng.Unit();
			int type = roll < 0.30f ? ITEM_RELIC : (roll < 0.55f ? ITEM_CANNED_FOOD : (roll < 0.80f ? ITEM_CLEAN_WATER : ITEM_HERB));
			DropItem(type, car + Vec3(0.0f, 0.0f, -1.7f), 0, 0, -1);
			ShowMessage("진드기를 다 치우자 차 안의 물건이 보인다.", 3.5f);
		}
	}

	float find = FindScale(stats);
	if (combatRng.Chance(info.herbChance * find)) DropItem(ITEM_HERB, e.Position() + Vec3(0.4f, 0.0f, 0.2f), 0, 0, -1);
	if (combatRng.Chance(info.waterChance * find)) DropItem(ITEM_CLEAN_WATER, e.Position() + Vec3(-0.4f, 0.0f, 0.1f), 0, 0, -1);
	if (e.type == ENEMY_MOSS_BOAR) DropItem(ITEM_MEAT, e.Position() + Vec3(-0.2f, 0.0f, -0.5f), 0, 0, -1);
	bool relic = combatRng.Chance(info.relicChance * find);
	if (relic) DropItem(ITEM_RELIC, e.Position() + Vec3(0.0f, 0.0f, -0.4f), 0, 0, -1);

	// Past the town what they held is worth keeping for a home: boards, wire, a sheet of tin.
	if (townReached && combatRng.Chance((e.type == ENEMY_HUSK ? 0.35f : 0.15f) * find))
		DropItem(ITEM_SCRAP, e.Position() + Vec3(0.4f, 0.0f, -0.3f), 0, 0, -1);

	// Striking one down does not kill anything: it lets go of what the spores were holding.
	AddPopup(e.Position() + Vec3(0.0f, 1.6f + info.radius, 0.0f), "풀려났다", kXpColor);
	if (messageTimer <= 0.5f)
	{
		if (e.type == ENEMY_HUSK)
			ShowMessage(relic ? "허물이 주저앉는다. 안주머니에서 누군가의 물건이 떨어진다."
							  : "허물이 주저앉는다. 빈 옷만 남는다.", 3.0f);
		else if (e.type == ENEMY_MOSS_BOAR)
			ShowMessage("멧돼지가 이끼 속으로 무너진다. 지키던 잠이 풀리고, 고기 한 덩이가 남는다.", 3.5f);
		else if (relic)
			ShowMessage("진드기가 터지며 붙잡혀 있던 것이 풀려난다.", 3.0f);
	}

	GrantXp(EnemyXpAt(info, e.level));
}

void Game::DamagePlayer(float amount, const Vec3& push)
{
	// The roll is the dodge: nothing lands while it plays.
	if (player->rollTimer > 0.0f || player->hurtTimer > 0.0f || deathTimer >= 0.0f) return;

	health -= amount;
	player->hurtTimer = kHurtTime;
	player->flash = 1.0f;
	player->SetPosition(player->Position() + FlatDirection(push) * 0.6f);
	shake = 1.0f;
	hurtFlash = 1.0f;

	char text[16];
	sprintf_s(text, sizeof(text), "-%d", (int)(amount + 0.5f));
	AddPopup(player->Position() + Vec3(0.0f, 2.0f, 0.0f), text, kHurtColor);

	if (health <= 0.0f) FallAsleep("긴 잠에 빠져든다...");
}

void Game::FallAsleep(const char* reason)
{
	health = 0.0f;
	deathTimer = 0.0f;
	player->swingTimer = 0.0f;
	ShowMessage(reason, kSleepDuration + 1.0f);
}

void Game::WakeAtSafePoint()
{
	deathTimer = -1.0f;
	health = MaxHealth(stats);
	sporeExposure = 0.2f;
	player->SetPosition(safePoint->Position());
	camera->SetPosition(player->Position());
	player->rollTimer = 0.0f;
	player->rollAngle = 0.0f;
	player->hurtTimer = 1.5f;

	std::vector<EnemyActor*> enemies = LiveEnemies();
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		if (!enemies[i]->alive) continue;
		enemies[i]->state = ENEMY_IDLE;
		enemies[i]->stateTimer = 0.0f;
	}

	// Where the long sleep ends moves on with the story.
	if (rootReached) ShowMessage("큰 나무 그늘, 집 앞에서 눈을 뜬다. 잃은 것은 시간뿐이다.", 4.0f);
	else if (ridgeClimbed) ShowMessage("송전탑 밑에서 눈을 뜬다. 잃은 것은 시간뿐이다.", 4.0f);
	else if (townWallRead) ShowMessage("학교 담벼락 밑에서 눈을 뜬다. 잃은 것은 시간뿐이다.", 4.0f);
	else ShowMessage("석등 곁에서 눈을 뜬다. 잃은 것은 시간뿐이다.", 4.0f);
}

void Game::UpdateEnemies(float dt)
{
	sheltered = (lantern && NearLantern(player->Position(), lantern->zoneRadius)) || InSanctuary(player->Position());

	std::vector<EnemyActor*> enemies = LiveEnemies();
	for (size_t i = 0; i < enemies.size(); ++i)
		UpdateEnemy(*enemies[i], dt);

	// Each kind of creature is explained once, the first time it comes close.
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		const EnemyActor& e = *enemies[i];
		if (!e.alive || enemySeen[e.type]) continue;
		if (DistXZ(e.Position(), player->Position()) > kLoreRange) continue;

		enemySeen[e.type] = true;
		ShowMessage(GetEnemyInfo(e.type).lore, 5.5f);
	}

	// Keep creatures from stacking into one another, then out of whatever that shoved them into: otherwise two
	// against a wall push each other in and are pushed back out every frame, shaking on the spot.
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		if (!enemies[i]->alive) continue;
		Vec3 pos = enemies[i]->Position();
		Vec3 before = pos;
		for (size_t j = i + 1; j < enemies.size(); ++j)
		{
			if (!enemies[j]->alive) continue;
			PushOutOfCircle(pos, enemies[i]->collider.radius, enemies[j]->Position(), enemies[j]->collider.radius);
		}
		if (DistXZ(pos, before) < 1e-5f) continue;
		ResolveRouteCollisions(pos, enemies[i]->collider.radius);
		KeepOutOfLight(pos, enemies[i]->collider.radius);
		enemies[i]->SetPosition(pos);
	}

	for (size_t i = 0; i < enemies.size(); ++i)
	{
		if (!enemies[i]->alive && enemies[i]->stateTimer <= 0.0f) enemies[i]->Destroy();
	}
}

void Game::UpdateEnemy(EnemyActor& e, float dt)
{
	const EnemyInfo& info = GetEnemyInfo(e.type);
	Vec3 pos = e.Position();
	float yaw = e.Yaw();

	e.flash = Maxf(e.flash - dt * 4.0f, 0.0f);
	pos = pos + e.knockback * dt;
	e.knockback = e.knockback * expf(-8.0f * dt);

	if (!e.alive)
	{
		e.stateTimer -= dt;
		e.SetPosition(pos);
		return;
	}

	Vec3 toPlayer(player->Position().x - pos.x, 0.0f, player->Position().z - pos.z);
	float dist = Length(toPlayer);
	Vec3 toPlayerDir = FlatDirection(toPlayer);
	bool playerAwake = deathTimer < 0.0f;
	float speed = info.speed * (1.0f + 0.05f * (float)(e.level - 1));

	switch (e.state)
	{
	case ENEMY_IDLE:
		e.stateTimer -= dt;
		if (e.holdsPlace)
		{
			// Back to its place, then still again, facing the way it always faced.
			MoveToward(pos, yaw, e.home, speed * 0.5f, dt);
			if (DistXZ(pos, e.home) < 0.1f) yaw = e.homeYaw;
		}
		else
		{
			if (e.stateTimer <= 0.0f)
			{
				e.moveTarget = e.home + Vec3(combatRng.Range(-4.0f, 4.0f), 0.0f, combatRng.Range(-4.0f, 4.0f));
				e.stateTimer = combatRng.Range(2.0f, 4.5f);
			}
			// Far from home it walks back with purpose rather than strolling.
			float pace = DistXZ(pos, e.home) > kLeashReturn ? 0.7f : 0.35f;
			MoveToward(pos, yaw, e.moveTarget, speed * pace, dt);
		}
		// One that gave up a chase at the edge of its ground does not turn round at once: started again at the
		// same edge, it would flip between chasing and going home every frame and shake on the spot.
		if (playerAwake && !sheltered && DistXZ(pos, e.home) < kLeashReturn &&
			(dist < info.aggroRange || (e.guarding && DistXZ(player->Position(), e.guardPos) < kGuardRange)))
		{
			e.state = ENEMY_CHASE;
			e.provoked = true;
			if (e.guarding) ShowMessage("잠든 이 곁의 멧돼지가 깨어난다! 지키던 잠에 다가온 것을 쫓아낸다.", 3.5f);
		}
		break;

	case ENEMY_CHASE:
		// A provoked creature chases well past its notice range; the boar is only ever provoked.
		// None follows into a lantern's light.
		if (!playerAwake || sheltered || dist > Maxf(info.aggroRange * 1.8f, e.provoked ? 16.0f : 0.0f) || DistXZ(pos, e.home) > kLeashRange)
		{
			e.provoked = false;
			e.state = ENEMY_IDLE;
			e.moveTarget = e.home;
			e.stateTimer = 3.0f;
		}
		else if (dist < info.attackRange)
		{
			e.state = ENEMY_WINDUP;
			e.stateTimer = info.windup;
			e.strikeDir = toPlayerDir;
		}
		else
		{
			MoveToward(pos, yaw, player->Position(), speed, dt);
		}
		break;

	case ENEMY_WINDUP:
		// The boar commits to its line when the windup starts; smaller creatures keep tracking.
		if (e.type != ENEMY_MOSS_BOAR) e.strikeDir = toPlayerDir;
		yaw = atan2f(e.strikeDir.x, e.strikeDir.z);
		e.stateTimer -= dt;
		if (e.stateTimer <= 0.0f)
		{
			e.state = ENEMY_STRIKE;
			e.stateTimer = info.lunge > 0.0f ? 0.4f : 0.15f;
			e.strikeHit = false;
		}
		break;

	case ENEMY_STRIKE:
		if (info.lunge > 0.0f) pos = pos + e.strikeDir * (info.lunge * dt);

		if (!e.strikeHit && playerAwake)
		{
			bool hit = (info.lunge > 0.0f)
				? (dist < e.collider.radius + player->collider.radius + 0.25f)
				: (dist < info.attackRange + 0.3f && Dot(toPlayerDir, e.strikeDir) > 0.3f);
			if (hit)
			{
				e.strikeHit = true;
				DamagePlayer(EnemyDamageAt(info, e.level), e.strikeDir);
			}
		}

		e.stateTimer -= dt;
		if (e.stateTimer <= 0.0f)
		{
			e.state = ENEMY_RECOVER;
			e.stateTimer = info.recover;
		}
		break;

	case ENEMY_RECOVER:
		e.stateTimer -= dt;
		if (e.stateTimer <= 0.0f) e.state = ENEMY_CHASE;
		break;

	default:
		break;
	}

	ResolveRouteCollisions(pos, e.collider.radius);
	if (e.state != ENEMY_STRIKE && playerAwake)
		PushOutOfCircle(pos, e.collider.radius, player->Position(), player->collider.radius);

	KeepOutOfLight(pos, e.collider.radius);

	e.SetPosition(pos);
	e.SetYaw(yaw);
}

void Game::KeepOutOfLight(Vec3& pos, float radius) const
{
	// The lantern's light itself keeps creatures out.
	Vec3 lanternAt;
	if (lantern && NearLantern(pos, lantern->zoneRadius + radius, &lanternAt))
		PushOutOfCircle(pos, radius, lanternAt, lantern->zoneRadius);

	// So does the clear air under the great tree.
	if (level == LEVEL_ROUTE && DistXZ(pos, routeMap.Sites().root) < kSanctuaryRadius + radius)
		PushOutOfCircle(pos, radius, routeMap.Sites().root, kSanctuaryRadius);
}

void Game::UpdateItems()
{
	std::vector<ItemActor*> items = LiveItems();

	// A relic lying about shows which entry it will fill: each one not yet found, and no two alike while there is a choice.
	for (size_t i = 0; i < items.size(); ++i)
	{
		ItemActor& item = *items[i];
		if (item.type != ITEM_RELIC || item.relicId >= 0) continue;

		bool shown[kRelicCount] = {};
		for (size_t j = 0; j < items.size(); ++j)
		{
			if (items[j]->type == ITEM_RELIC && items[j]->relicId >= 0) shown[items[j]->relicId] = true;
		}
		for (int r = RELIC_WATCH; r <= RELIC_PHONE && item.relicId < 0; ++r)
		{
			// The watch and the phone wait beside the two who carried them.
			if (r == RELIC_WATCH || r == RELIC_PHONE) continue;
			if (!relicFound[r] && !shown[r]) item.relicId = r;
		}
	}

	for (size_t i = 0; i < items.size(); ++i)
	{
		ItemActor& item = *items[i];

		// Drops that fall far behind are forgotten. The pipe and landmark finds stay: the guide waits for them.
		if (item.spawnIndex < 0 && !item.landmark && item.type != ITEM_RUSTY_PIPE && DistXZ(player->Position(), item.Position()) > 60.0f)
		{
			item.Destroy();
			continue;
		}

		if (deathTimer < 0.0f && DistXZ(player->Position(), item.Position()) < kPickupRange)
		{
			// A full bag leaves the find where it lies.
			if (TakesBagSlot(item.type) && !BagHasRoom())
			{
				if (messageTimer <= 0.0f) ShowMessage("배낭이 가득 찼다. 먼저 먹거나 마시거나 써서 비워라.", 2.5f);
				continue;
			}

			item.Destroy();
			PickUp(item);
		}
	}
}

void Game::UpdateCombatTimers(float dt)
{
	player->attackTimer = Maxf(player->attackTimer - dt, 0.0f);
	player->swingTimer = Maxf(player->swingTimer - dt, 0.0f);
	player->hurtTimer = Maxf(player->hurtTimer - dt, 0.0f);
	player->flash = Maxf(player->flash - dt * 4.0f, 0.0f);
	levelUpTimer = Maxf(levelUpTimer - dt, 0.0f);

	for (size_t i = 0; i < popups.size(); ++i)
	{
		popups[i].timer -= dt;
		popups[i].pos.y += dt * 1.2f;
	}

	popups.erase(std::remove_if(popups.begin(), popups.end(),
		[](const Popup& p) { return p.timer <= 0.0f; }), popups.end());
}

void Game::DrawWisps()
{
	for (size_t i = 0; i < wisps.size(); ++i)
	{
		const Wisp& w = wisps[i];
		float k = Saturatef(w.life / w.maxLife);
		// One being drawn in keeps its size until it is taken.
		float s = w.size * (w.seek > 0.0f ? 0.6f + 0.4f * k : k);

		DrawParams params;
		params.tint = w.tint;
		params.emissive = (0.6f + 1.4f * k) * w.glow;
		renderer->DrawModel(w.plain ? MODEL_MOTE : MODEL_WISP, w.pos, 0.0f, Vec3(s, s, s), params);
	}
}

EnemyActor* Game::SpawnEnemy(int type, int enemyLevel, const Vec3& pos, int chunkX, int chunkZ, int spawnIndex)
{
	const EnemyInfo& info = GetEnemyInfo(type);

	EnemyActor* e = enemyGroup->AddChild(new EnemyActor(type, enemyLevel));
	e->SetPosition(pos);
	e->home = pos;
	e->moveTarget = pos;
	e->maxHealth = EnemyHealthAt(info, enemyLevel);
	e->health = e->maxHealth;
	e->SetYaw(combatRng.Range(0.0f, 2.0f * kPi));
	e->stateTimer = combatRng.Range(0.5f, 2.0f);
	e->chunkX = chunkX;
	e->chunkZ = chunkZ;
	e->spawnIndex = spawnIndex;
	return e;
}

ItemActor* Game::DropItem(int type, const Vec3& pos, int chunkX, int chunkZ, int spawnIndex)
{
	ItemActor* item = itemGroup->AddChild(new ItemActor(type));
	item->SetPosition(Vec3(pos.x, 0.0f, pos.z));
	item->phase = combatRng.Range(0.0f, 6.0f);
	item->chunkX = chunkX;
	item->chunkZ = chunkZ;
	item->spawnIndex = spawnIndex;
	return item;
}

void Game::PickUp(ItemActor& item)
{
	const ItemInfo& info = GetItemInfo(item.type);
	if (item.type != ITEM_RELIC) ShowMessage(info.pickupText, 3.5f);
	++pickups[item.type];
	PlayPlayerAction("pickup");

	// What is taken comes apart into light that gathers into whoever took it; a relic in a warm burst.
	bool relic = item.type == ITEM_RELIC;
	for (int i = 0, n = relic ? 16 : 5; i < n; ++i)
	{
		float a = fxRng.Range(0.0f, 2.0f * kPi);
		Wisp w;
		w.pos = item.Position() + Vec3(0.0f, 0.4f, 0.0f);
		w.vel = Vec3(cosf(a) * 2.2f, fxRng.Range(1.0f, 2.6f), sinf(a) * 2.2f);
		w.maxLife = w.life = 1.6f;
		w.tint = relic ? Vec3(1.00f, 0.90f, 0.62f) : Vec3(0.75f, 1.00f, 0.85f);
		w.size = 0.7f;
		w.glow = 1.6f;
		w.seek = 1.0f;
		w.sway = 0.0f;
		w.plain = true;
		wisps.push_back(w);
	}

	if (item.spawnIndex >= 0)
	{
		ChunkState& state = chunkStates[ChunkMap::Key(item.chunkX, item.chunkZ)];
		if (item.spawnIndex < (int)state.itemTaken.size())
			state.itemTaken[item.spawnIndex] = true;
	}

	switch (item.type)
	{
	case ITEM_RUSTY_PIPE:
		EquipWeapon();
		break;
	case ITEM_RELIC:
		FindRelic(item.relicId);
		break;
	case ITEM_CANNED_FOOD:
		// Only the first can is a journal entry; later ones are just food.
		++inventory[item.type];
		if (!relicFound[RELIC_PEACHES]) FindRelic(RELIC_PEACHES);
		break;
	default:
		++inventory[item.type];
		NoteFirstFind(item.type);
		break;
	}
}

void Game::EquipWeapon()
{
	if (!player->weapon) player->weapon = player->AddChild(new WeaponActor());
}

void Game::UseHerb()
{
	if (deathTimer >= 0.0f) return;

	float maxHealth = MaxHealth(stats);
	if (inventory[ITEM_HERB] <= 0)
	{
		ShowMessage("약초가 없다. 짐승들이 가끔 떨어뜨린다.", 2.5f);
		return;
	}
	if (health >= maxHealth)
	{
		ShowMessage("다친 데가 없다.", 1.5f);
		return;
	}

	--inventory[ITEM_HERB];
	++herbsUsed;
	float heal = Minf(maxHealth * kHerbHeal, maxHealth - health);
	health += heal;

	char text[16];
	sprintf_s(text, sizeof(text), "+%d", (int)(heal + 0.5f));
	AddPopup(player->Position() + Vec3(0.0f, 2.0f, 0.0f), text, kHealColor);
}

void Game::GrantXp(int amount)
{
	amount = (int)((float)amount * XpScale(stats) + 0.5f);

	char text[32];
	sprintf_s(text, sizeof(text), "경험치 +%d", amount);
	AddPopup(player->Position() + Vec3(0.0f, 2.4f, 0.0f), text, kXpColor);

	int gained = GainXp(stats, amount);
	if (gained <= 0) return;

	health = MaxHealth(stats);
	levelUpTimer = kLevelUpTime;

	// Spore light wells up around the player and rises away.
	Vec3 feet = player->Position();
	for (int i = 0; i < 22; ++i)
	{
		float a = fxRng.Range(0.0f, 2.0f * kPi);
		float r = fxRng.Range(0.3f, 1.1f);
		Wisp w;
		w.pos = feet + Vec3(cosf(a) * r, fxRng.Range(0.05f, 0.4f), sinf(a) * r);
		w.vel = Vec3(cosf(a) * 0.3f, fxRng.Range(1.4f, 2.8f), sinf(a) * 0.3f);
		w.maxLife = fxRng.Range(1.3f, 2.1f);
		w.life = w.maxLife;
		wisps.push_back(w);
	}

	char buf[256];
	sprintf_s(buf, sizeof(buf), "레벨 업! 레벨 %d이 되었다. C를 눌러 능력치 %d점을 분배하라.",
			  stats.level, stats.unspentPoints);
	ShowMessage(buf, 6.0f);
}

void Game::AddPopup(const Vec3& pos, const char* text, const Vec3& color)
{
	Popup p;
	p.pos = pos;
	p.color = color;
	p.timer = kPopupTime;
	strncpy_s(p.text, sizeof(p.text), text, _TRUNCATE);
	popups.push_back(p);
}

void Game::OpenStatPanel()
{
	if (deathTimer >= 0.0f) return;

	statPanelOpen = true;
	statCursor = 0;
	for (int i = 0; i < STAT_COUNT; ++i) pendingPoints[i] = 0;
}

void Game::CloseStatPanel()
{
	statPanelOpen = false;
	for (int i = 0; i < STAT_COUNT; ++i) pendingPoints[i] = 0;
}

void Game::HandleStatPanelKey(unsigned char key)
{
	int pendingTotal = 0;
	for (int i = 0; i < STAT_COUNT; ++i) pendingTotal += pendingPoints[i];

	switch (key)
	{
	case 'w':
		statCursor = (statCursor + STAT_COUNT - 1) % STAT_COUNT;
		break;
	case 's':
		statCursor = (statCursor + 1) % STAT_COUNT;
		break;
	case 'd':
		if (pendingTotal < stats.unspentPoints) ++pendingPoints[statCursor];
		break;
	case 'a':
		if (pendingPoints[statCursor] > 0) --pendingPoints[statCursor];
		break;
	case 'e':
		ConfirmStats();
		break;
	case 'c':
	case 27:
		CloseStatPanel();
		break;
	default:
		break;
	}
}

void Game::ConfirmStats()
{
	int spent = 0;
	for (int i = 0; i < STAT_COUNT; ++i) spent += pendingPoints[i];

	if (spent == 0)
	{
		CloseStatPanel();
		return;
	}

	float oldMax = MaxHealth(stats);
	for (int i = 0; i < STAT_COUNT; ++i)
	{
		stats.stat[i] += pendingPoints[i];
		pendingPoints[i] = 0;
	}
	stats.unspentPoints -= spent;
	health += MaxHealth(stats) - oldMax;
	++statConfirmations;
	statPanelOpen = false;

	char buf[256];
	sprintf_s(buf, sizeof(buf), "능력치를 분배했다. 남은 점수 %d점.", stats.unspentPoints);
	ShowMessage(buf, 3.0f);
}

void Game::DrawPopups()
{
	for (size_t i = 0; i < popups.size(); ++i)
	{
		const Popup& p = popups[i];
		float sx, sy;
		if (!renderer->WorldToScreen(p.pos, sx, sy)) continue;

		float fade = Saturatef(p.timer / kPopupTime * 1.5f);
		int tw = renderer->TextWidth(p.text, true);
		renderer->DrawTexts((int)sx - tw / 2, (int)sy, p.text, p.color * fade, true);
	}
}

void Game::DrawCombatHud()
{
	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	std::vector<EnemyActor*> enemies = LiveEnemies();
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		const EnemyActor& e = *enemies[i];
		if (!e.alive || e.health >= e.maxHealth) continue;

		const EnemyInfo& info = GetEnemyInfo(e.type);
		float sx, sy;
		if (!renderer->WorldToScreen(e.Position() + Vec3(0.0f, 1.3f + info.radius, 0.0f), sx, sy)) continue;
		renderer->DrawBarPx(sx - 22.0f, sy - 4.0f, 44.0f, 5.0f, e.health / e.maxHealth,
							Vec3(0.80f, 0.30f, 0.25f), Vec3(0.95f, 0.75f, 0.35f));
	}

	float maxHealth = MaxHealth(stats);
	int need = XpToNextLevel(stats.level);
	char buf[128];

	// Under the objective box, and whatever comes after goes under it in turn.
	float top = hudLeftBottom + 8.0f;
	float panelH = stats.unspentPoints > 0 ? 134.0f : 100.0f;
	hudLeftBottom = top + panelH;
	if (!UnderMemoMap(18.0f, top, 330.0f, panelH))
	{
		int y = (int)top;
		renderer->DrawPanelPx(18.0f, top, 330.0f, 100.0f, UI_MIST, kHudPanel, 0.38f);

		sprintf_s(buf, sizeof(buf), "Lv %d", stats.level);
		renderer->DrawTexts(32, y + 32, buf, kHudAccent, true);

		sprintf_s(buf, sizeof(buf), "체력  %d / %d", (int)(health + 0.5f), (int)(maxHealth + 0.5f));
		renderer->DrawTexts(104, y + 20, buf, kHudInk, false);
		renderer->DrawBarPx(104.0f, top + 26.0f, 228.0f, 10.0f, health / maxHealth,
							Vec3(0.75f, 0.28f, 0.25f), Vec3(0.45f, 0.85f, 0.55f));

		sprintf_s(buf, sizeof(buf), "경험치  %d / %d", stats.xp, need);
		renderer->DrawTexts(104, y + 54, buf, kHudDim, false);
		renderer->DrawBarPx(104.0f, top + 60.0f, 228.0f, 8.0f, (float)stats.xp / (float)need,
							Vec3(0.40f, 0.55f, 0.75f), Vec3(0.60f, 0.85f, 0.95f));

		int meals = inventory[ITEM_BERRIES] + inventory[ITEM_VEGETABLE] + inventory[ITEM_CANNED_FOOD]
				  + inventory[ITEM_MEAT] + inventory[ITEM_COOKED_MEAT];
		sprintf_s(buf, sizeof(buf), "%s   약초 %d [Q]  물 %d [R]  음식 %d [F]",
				  player->weapon ? "파이프" : "맨손", inventory[ITEM_HERB], inventory[ITEM_CLEAN_WATER], meals);
		renderer->DrawTexts(32, y + 88, buf, kHudDim, false);

		if (stats.unspentPoints > 0)
		{
			float pulse = 0.65f + 0.35f * sinf(time * 5.0f);
			sprintf_s(buf, sizeof(buf), "분배할 능력치 %d점  [C]", stats.unspentPoints);
			renderer->DrawPanelPx(18.0f, top + 106.0f, 330.0f, 28.0f, UI_MIST, kHudPanel, 0.38f);
			renderer->DrawTexts(32, y + 125, buf, kHudAccent * pulse, false);
		}
	}

	if (levelUpTimer > 0.0f)
	{
		const char* banner = "레벨 업";
		float fade = Saturatef(levelUpTimer);
		int tw = renderer->TextWidth(banner, true);
		renderer->DrawTexts(w / 2 - tw / 2, h / 2 - 90, banner, Vec3(0.85f, 0.97f, 0.70f) * fade, true);
	}

	if (deathTimer >= 0.0f)
		renderer->DrawFade(Vec3(0.02f, 0.03f, 0.04f), Saturatef(deathTimer / kSleepDuration) * 0.9f);
}

void Game::DrawStatPanel()
{
	if (!statPanelOpen) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	renderer->DrawFade(Vec3(0.02f, 0.03f, 0.04f), 0.55f);

	float pw = 640.0f, ph = 380.0f;
	float px = (float)w * 0.5f - pw * 0.5f;
	float py = (float)h * 0.5f - ph * 0.5f;
	renderer->DrawPanelPx(px, py, pw, ph, UI_WINDOW, Vec3(0.10f, 0.11f, 0.10f), 0.94f);
	renderer->DrawPanelPx(px, py, pw, 3.0f, UI_RULE, Vec3(0.45f, 0.60f, 0.50f), 0.8f);

	int pending = 0;
	CharacterStats preview = stats;
	for (int i = 0; i < STAT_COUNT; ++i)
	{
		pending += pendingPoints[i];
		preview.stat[i] += pendingPoints[i];
	}
	int remaining = stats.unspentPoints - pending;
	float weapon = player->weapon ? player->weapon->power : 0.0f;

	int x = (int)px + 36;
	int y = (int)py + 44;
	char buf[128];

	sprintf_s(buf, sizeof(buf), "능력치  -  레벨 %d", stats.level);
	renderer->DrawTexts(x, y, buf, kHudInk, true);
	sprintf_s(buf, sizeof(buf), "분배할 점수: %d", remaining);
	renderer->DrawTexts(x + 360, y, buf, remaining > 0 ? kHudAccent : kHudDim, false);

	y += 42;
	for (int i = 0; i < STAT_COUNT; ++i)
	{
		bool selected = (i == statCursor);
		if (selected)
			renderer->DrawPanelPx((float)x - 14.0f, (float)y - 18.0f, pw - 44.0f, 26.0f, UI_SELECT, Vec3(0.25f, 0.40f, 0.34f), 0.45f);

		// Columns sit at fixed offsets: the font is proportional, so padding with spaces would not line up.
		Vec3 color = selected ? kHudInk : kHudDim;
		if (selected) renderer->DrawTexts(x, y, ">", color, false);
		renderer->DrawTexts(x + 16, y, StatName(i), color, false);

		if (pendingPoints[i] > 0)
			sprintf_s(buf, sizeof(buf), "%d  (+%d)", stats.stat[i], pendingPoints[i]);
		else
			sprintf_s(buf, sizeof(buf), "%d", stats.stat[i]);
		renderer->DrawTexts(x + 110, y, buf, color, false);
		renderer->DrawTexts(x + 200, y, StatHint(i), kHudDim, false);
		y += 30;
	}

	y += 16;
	renderer->DrawTexts(x, y, "달라지는 것", kHudDim, false);
	y += 26;

	renderer->DrawTexts(x, y, "최대 체력", kHudInk, false);
	sprintf_s(buf, sizeof(buf), "%d  ->  %d", (int)(MaxHealth(stats) + 0.5f), (int)(MaxHealth(preview) + 0.5f));
	renderer->DrawTexts(x + 110, y, buf, kHudInk, false);
	renderer->DrawTexts(x + 300, y, "공격력", kHudInk, false);
	sprintf_s(buf, sizeof(buf), "%.1f  ->  %.1f", AttackPower(stats, weapon), AttackPower(preview, weapon));
	renderer->DrawTexts(x + 410, y, buf, kHudInk, false);
	y += 24;

	renderer->DrawTexts(x, y, "이동 속도", kHudInk, false);
	sprintf_s(buf, sizeof(buf), "%d%%  ->  %d%%", (int)(MoveSpeedScale(stats) * 100.0f + 0.5f), (int)(MoveSpeedScale(preview) * 100.0f + 0.5f));
	renderer->DrawTexts(x + 110, y, buf, kHudInk, false);
	renderer->DrawTexts(x + 300, y, "경험치 획득", kHudInk, false);
	sprintf_s(buf, sizeof(buf), "%d%%  ->  %d%%", (int)(XpScale(stats) * 100.0f + 0.5f), (int)(XpScale(preview) * 100.0f + 0.5f));
	renderer->DrawTexts(x + 410, y, buf, kHudInk, false);

	if (level == LEVEL_ROUTE && (guide == GUIDE_ASSIGN_STATS || guide == GUIDE_ASSIGN_AGAIN))
		renderer->DrawTexts(x, (int)(py + ph) - 48, "팁: D를 몇 번 눌러 오른쪽 숫자가 어떻게 바뀌는지 보라.", kHudDim, false);

	renderer->DrawTexts(x, (int)(py + ph) - 22, "W/S 선택    D 올리기    A 내리기    E 확정    C 닫기", kHudAccent, false);
}
