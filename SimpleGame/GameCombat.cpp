#include "stdafx.h"
#include "Game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "Collision.h"

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
	float power = AttackPower(stats, player->weapon ? player->weapon->power : 0.0f) * (food <= 0.0f ? 0.8f : 1.0f);
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

	if (e.spawnIndex >= 0)
	{
		ChunkState& state = chunkStates[ChunkMap::Key(e.chunkX, e.chunkZ)];
		if (e.spawnIndex < (int)state.respawnAt.size())
			state.respawnAt[e.spawnIndex] = time + kRespawnDelay;
	}

	float find = FindScale(stats);
	if (combatRng.Chance(info.herbChance * find)) DropItem(ITEM_HERB, e.Position() + Vec3(0.4f, 0.0f, 0.2f), 0, 0, -1);
	if (combatRng.Chance(info.waterChance * find)) DropItem(ITEM_CLEAN_WATER, e.Position() + Vec3(-0.4f, 0.0f, 0.1f), 0, 0, -1);
	if (combatRng.Chance(info.relicChance * find)) DropItem(ITEM_RELIC, e.Position() + Vec3(0.0f, 0.0f, -0.4f), 0, 0, -1);

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

	char text[16];
	sprintf_s(text, sizeof(text), "-%d", (int)(amount + 0.5f));
	AddPopup(player->Position() + Vec3(0.0f, 2.0f, 0.0f), text, kHurtColor);

	if (health <= 0.0f) FallAsleep("You sink into a long sleep...");
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

	ShowMessage("You wake beside the lantern. Nothing is lost but time.", 4.0f);
}

void Game::UpdateEnemies(float dt)
{
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

	// Keep creatures from stacking into one another.
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		for (size_t j = i + 1; j < enemies.size(); ++j)
		{
			if (!enemies[i]->alive || !enemies[j]->alive) continue;
			Vec3 pos = enemies[i]->Position();
			PushOutOfCircle(pos, enemies[i]->collider.radius, enemies[j]->Position(), enemies[j]->collider.radius);
			enemies[i]->SetPosition(pos);
		}
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
		if (e.stateTimer <= 0.0f)
		{
			e.moveTarget = e.home + Vec3(combatRng.Range(-4.0f, 4.0f), 0.0f, combatRng.Range(-4.0f, 4.0f));
			e.stateTimer = combatRng.Range(2.0f, 4.5f);
		}
		MoveToward(pos, yaw, e.moveTarget, speed * 0.35f, dt);
		if (playerAwake && dist < info.aggroRange)
		{
			e.state = ENEMY_CHASE;
			e.provoked = true;
		}
		break;

	case ENEMY_CHASE:
		// A provoked creature chases well past its notice range; the boar is only ever provoked.
		if (!playerAwake || dist > Maxf(info.aggroRange * 1.8f, e.provoked ? 16.0f : 0.0f) || DistXZ(pos, e.home) > 22.0f)
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

	e.SetPosition(pos);
	e.SetYaw(yaw);
}

void Game::UpdateItems()
{
	std::vector<ItemActor*> items = LiveItems();
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
				if (messageTimer <= 0.0f) ShowMessage("Your bag is full. Eat, drink or use something first.", 2.5f);
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

void Game::SpawnEnemy(int type, int enemyLevel, const Vec3& pos, int chunkX, int chunkZ, int spawnIndex)
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
		++inventory[item.type];
		FindRelic(RELIC_PEACHES);
		break;
	default:
		++inventory[item.type];
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
		ShowMessage("No herbs. Creatures sometimes drop them.", 2.5f);
		return;
	}
	if (health >= maxHealth)
	{
		ShowMessage("You are not hurt.", 1.5f);
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

	char text[16];
	sprintf_s(text, sizeof(text), "+%d XP", amount);
	AddPopup(player->Position() + Vec3(0.0f, 2.4f, 0.0f), text, kXpColor);

	int gained = GainXp(stats, amount);
	if (gained <= 0) return;

	health = MaxHealth(stats);
	levelUpTimer = 3.0f;

	char buf[96];
	sprintf_s(buf, sizeof(buf), "Level up! You are level %d. Press C to assign %d stat points.",
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

	char buf[96];
	sprintf_s(buf, sizeof(buf), "Stats assigned. %d point%s left.", stats.unspentPoints, stats.unspentPoints == 1 ? "" : "s");
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

	renderer->DrawRectPx(18.0f, 88.0f, 330.0f, 100.0f, kHudPanel, 0.38f);

	sprintf_s(buf, sizeof(buf), "LV %d", stats.level);
	renderer->DrawTexts(32, 120, buf, kHudAccent, true);

	sprintf_s(buf, sizeof(buf), "HP  %d / %d", (int)(health + 0.5f), (int)(maxHealth + 0.5f));
	renderer->DrawTexts(104, 108, buf, kHudInk, false);
	renderer->DrawBarPx(104.0f, 114.0f, 228.0f, 10.0f, health / maxHealth,
						Vec3(0.75f, 0.28f, 0.25f), Vec3(0.45f, 0.85f, 0.55f));

	sprintf_s(buf, sizeof(buf), "XP  %d / %d", stats.xp, need);
	renderer->DrawTexts(104, 142, buf, kHudDim, false);
	renderer->DrawBarPx(104.0f, 148.0f, 228.0f, 8.0f, (float)stats.xp / (float)need,
						Vec3(0.40f, 0.55f, 0.75f), Vec3(0.60f, 0.85f, 0.95f));

	int meals = inventory[ITEM_BERRIES] + inventory[ITEM_VEGETABLE] + inventory[ITEM_CANNED_FOOD];
	sprintf_s(buf, sizeof(buf), "%s   Herb %d [Q]  Water %d [R]  Food %d [F]",
			  player->weapon ? "Pipe" : "Hands", inventory[ITEM_HERB], inventory[ITEM_CLEAN_WATER], meals);
	renderer->DrawTexts(32, 176, buf, kHudDim, false);

	if (stats.unspentPoints > 0)
	{
		float pulse = 0.65f + 0.35f * sinf(time * 5.0f);
		sprintf_s(buf, sizeof(buf), "%d stat point%s to assign  [C]", stats.unspentPoints, stats.unspentPoints == 1 ? "" : "s");
		renderer->DrawRectPx(18.0f, 194.0f, 330.0f, 28.0f, kHudPanel, 0.38f);
		renderer->DrawTexts(32, 213, buf, kHudAccent * pulse, false);
	}

	if (levelUpTimer > 0.0f)
	{
		const char* banner = "LEVEL UP";
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
	renderer->DrawRectPx(px, py, pw, ph, Vec3(0.10f, 0.11f, 0.10f), 0.94f);
	renderer->DrawRectPx(px, py, pw, 3.0f, Vec3(0.45f, 0.60f, 0.50f), 0.8f);

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

	sprintf_s(buf, sizeof(buf), "STATS  -  Level %d", stats.level);
	renderer->DrawTexts(x, y, buf, kHudInk, true);
	sprintf_s(buf, sizeof(buf), "Points to assign: %d", remaining);
	renderer->DrawTexts(x + 360, y, buf, remaining > 0 ? kHudAccent : kHudDim, false);

	y += 42;
	for (int i = 0; i < STAT_COUNT; ++i)
	{
		bool selected = (i == statCursor);
		if (selected)
			renderer->DrawRectPx((float)x - 14.0f, (float)y - 18.0f, pw - 44.0f, 26.0f, Vec3(0.25f, 0.40f, 0.34f), 0.45f);

		if (pendingPoints[i] > 0)
			sprintf_s(buf, sizeof(buf), "%s %-9s %2d  (+%d)", selected ? ">" : " ", StatName(i), stats.stat[i], pendingPoints[i]);
		else
			sprintf_s(buf, sizeof(buf), "%s %-9s %2d", selected ? ">" : " ", StatName(i), stats.stat[i]);

		renderer->DrawTexts(x, y, buf, selected ? kHudInk : kHudDim, false);
		renderer->DrawTexts(x + 280, y, StatHint(i), kHudDim, false);
		y += 30;
	}

	y += 16;
	renderer->DrawTexts(x, y, "What changes", kHudDim, false);
	y += 26;

	sprintf_s(buf, sizeof(buf), "Max health     %3d  ->  %3d", (int)(MaxHealth(stats) + 0.5f), (int)(MaxHealth(preview) + 0.5f));
	renderer->DrawTexts(x, y, buf, kHudInk, false);
	sprintf_s(buf, sizeof(buf), "Attack     %5.1f  ->  %5.1f", AttackPower(stats, weapon), AttackPower(preview, weapon));
	renderer->DrawTexts(x + 300, y, buf, kHudInk, false);
	y += 24;

	sprintf_s(buf, sizeof(buf), "Move speed     %3d%% ->  %3d%%", (int)(MoveSpeedScale(stats) * 100.0f + 0.5f), (int)(MoveSpeedScale(preview) * 100.0f + 0.5f));
	renderer->DrawTexts(x, y, buf, kHudInk, false);
	sprintf_s(buf, sizeof(buf), "XP gain     %4d%% -> %4d%%", (int)(XpScale(stats) * 100.0f + 0.5f), (int)(XpScale(preview) * 100.0f + 0.5f));
	renderer->DrawTexts(x + 300, y, buf, kHudInk, false);

	if (level == LEVEL_ROUTE && (guide == GUIDE_ASSIGN_STATS || guide == GUIDE_ASSIGN_AGAIN))
		renderer->DrawTexts(x, (int)(py + ph) - 48, "Tip: press D a few times and watch the right-hand numbers move.", kHudDim, false);

	renderer->DrawTexts(x, (int)(py + ph) - 22, "W/S select    D add    A remove    E confirm    C close", kHudAccent, false);
}
