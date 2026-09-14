#include "stdafx.h"
#include "Game.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "Collision.h"
#include "Models.h"

namespace
{
	const float kSwingTime = 0.22f;
	const float kPipeBonus = 6.0f;
	const float kHurtTime = 0.6f;
	const float kSleepDuration = 2.2f;
	const float kPickupRange = 1.1f;
	const float kPopupTime = 0.9f;
	const float kRespawnDelay = 90.0f;
	const float kHerbHeal = 0.4f;        // fraction of max health
	const float kWaterClear = 0.45f;
	const float kDyingTime = 0.6f;
	const float kRelicXp = 12.0f;

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

	void MoveToward(Enemy& e, const Vec3& target, float speed, float dt)
	{
		Vec3 to(target.x - e.pos.x, 0.0f, target.z - e.pos.z);
		float dist = Length(to);
		if (dist < 0.05f) return;

		float step = Minf(speed * dt, dist);
		e.pos = e.pos + to * (step / dist);
		e.yaw = atan2f(to.x, to.z);
	}
}

void Game::Attack()
{
	if (level != LEVEL_ROUTE || statPanelOpen || deathTimer >= 0.0f) return;
	if (attackTimer > 0.0f || rollTimer > 0.0f) return;

	attackTimer = AttackCooldown(stats);
	swingTimer = kSwingTime;

	Vec3 facing(sinf(playerYaw), 0.0f, cosf(playerYaw));
	float power = AttackPower(stats, hasWeapon ? kPipeBonus : 0.0f);
	float reach = hasWeapon ? 2.1f : 1.5f;

	for (size_t i = 0; i < enemies.size(); ++i)
	{
		Enemy& e = enemies[i];
		if (!e.alive) continue;

		const EnemyInfo& info = GetEnemyInfo(e.type);
		Vec3 to(e.pos.x - playerPos.x, 0.0f, e.pos.z - playerPos.z);
		float dist = Length(to);
		if (dist > reach + info.radius) continue;

		// A wide arc in front; point-blank enemies count wherever they stand.
		if (dist > 0.6f && Dot(to * (1.0f / dist), facing) < 0.35f) continue;

		DamageEnemy(e, power * combatRng.Range(0.9f, 1.1f), to);
	}
}

void Game::DamageEnemy(Enemy& e, float amount, const Vec3& push)
{
	if (!e.alive) return;

	const EnemyInfo& info = GetEnemyInfo(e.type);
	e.health -= amount;
	e.flash = 1.0f;
	e.knockback = FlatDirection(push) * (e.type == ENEMY_MOSS_BOAR ? 2.5f : 6.0f);

	char text[16];
	sprintf_s(text, sizeof(text), "%d", (int)(amount + 0.5f));
	AddPopup(e.pos + Vec3(0.0f, 1.2f + info.radius, 0.0f), text, kHitColor);

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

void Game::KillEnemy(Enemy& e)
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

	if (combatRng.Chance(info.herbChance)) DropItem(ITEM_HERB, e.pos + Vec3(0.4f, 0.0f, 0.2f), 0, 0, -1);
	if (combatRng.Chance(info.waterChance)) DropItem(ITEM_CLEAN_WATER, e.pos + Vec3(-0.4f, 0.0f, 0.1f), 0, 0, -1);
	if (combatRng.Chance(info.relicChance)) DropItem(ITEM_RELIC, e.pos + Vec3(0.0f, 0.0f, -0.4f), 0, 0, -1);

	GrantXp(EnemyXpAt(info, e.level));
}

void Game::DamagePlayer(float amount, const Vec3& push)
{
	// The roll is the dodge: nothing lands while it plays.
	if (rollTimer > 0.0f || hurtTimer > 0.0f || deathTimer >= 0.0f) return;

	health -= amount;
	hurtTimer = kHurtTime;
	playerFlash = 1.0f;
	playerPos = playerPos + FlatDirection(push) * 0.6f;

	char text[16];
	sprintf_s(text, sizeof(text), "-%d", (int)(amount + 0.5f));
	AddPopup(playerPos + Vec3(0.0f, 2.0f, 0.0f), text, kHurtColor);

	if (health <= 0.0f) FallAsleep("You sink into a long sleep...");
}

void Game::FallAsleep(const char* reason)
{
	health = 0.0f;
	deathTimer = 0.0f;
	swingTimer = 0.0f;
	ShowMessage(reason, kSleepDuration + 1.0f);
}

void Game::WakeAtSafePoint()
{
	deathTimer = -1.0f;
	health = MaxHealth(stats);
	sporeExposure = 0.2f;
	playerPos = safePoint;
	camTarget = playerPos;
	rollTimer = 0.0f;
	rollAngle = 0.0f;
	hurtTimer = 1.5f;

	for (size_t i = 0; i < enemies.size(); ++i)
	{
		if (!enemies[i].alive) continue;
		enemies[i].state = ENEMY_IDLE;
		enemies[i].stateTimer = 0.0f;
	}

	ShowMessage("You wake beside the lantern. Nothing is lost but time.", 4.0f);
}

void Game::UpdateEnemies(float dt)
{
	for (size_t i = 0; i < enemies.size(); ++i)
		UpdateEnemy(enemies[i], dt);

	// Keep creatures from stacking into one another.
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		for (size_t j = i + 1; j < enemies.size(); ++j)
		{
			if (!enemies[i].alive || !enemies[j].alive) continue;
			PushOutOfCircle(enemies[i].pos, GetEnemyInfo(enemies[i].type).radius,
							enemies[j].pos, GetEnemyInfo(enemies[j].type).radius);
		}
	}

	enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
		[](const Enemy& e) { return !e.alive && e.stateTimer <= 0.0f; }), enemies.end());
}

void Game::UpdateEnemy(Enemy& e, float dt)
{
	const EnemyInfo& info = GetEnemyInfo(e.type);

	e.flash = Maxf(e.flash - dt * 4.0f, 0.0f);
	e.pos = e.pos + e.knockback * dt;
	e.knockback = e.knockback * expf(-8.0f * dt);

	if (!e.alive)
	{
		e.stateTimer -= dt;
		return;
	}

	Vec3 toPlayer(playerPos.x - e.pos.x, 0.0f, playerPos.z - e.pos.z);
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
		MoveToward(e, e.moveTarget, speed * 0.35f, dt);
		if (playerAwake && dist < info.aggroRange) e.state = ENEMY_CHASE;
		break;

	case ENEMY_CHASE:
		if (!playerAwake || dist > info.aggroRange * 1.8f || DistXZ(e.pos, e.home) > 22.0f)
		{
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
			MoveToward(e, playerPos, speed, dt);
		}
		break;

	case ENEMY_WINDUP:
		// The boar commits to its line when the windup starts; smaller creatures keep tracking.
		if (e.type != ENEMY_MOSS_BOAR) e.strikeDir = toPlayerDir;
		e.yaw = atan2f(e.strikeDir.x, e.strikeDir.z);
		e.stateTimer -= dt;
		if (e.stateTimer <= 0.0f)
		{
			e.state = ENEMY_STRIKE;
			e.stateTimer = info.lunge > 0.0f ? 0.4f : 0.15f;
			e.strikeHit = false;
		}
		break;

	case ENEMY_STRIKE:
		if (info.lunge > 0.0f) e.pos = e.pos + e.strikeDir * (info.lunge * dt);

		if (!e.strikeHit && playerAwake)
		{
			bool hit = (info.lunge > 0.0f)
				? (dist < info.radius + kPlayerRadius + 0.25f)
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

	ResolveRouteCollisions(e.pos, info.radius);
	if (e.state != ENEMY_STRIKE && playerAwake)
		PushOutOfCircle(e.pos, info.radius, playerPos, kPlayerRadius);
}

void Game::UpdateItems()
{
	for (size_t i = 0; i < worldItems.size(); ++i)
	{
		WorldItem& item = worldItems[i];

		// Drops that fall far behind are forgotten.
		if (item.spawnIndex < 0 && DistXZ(playerPos, item.pos) > 60.0f)
		{
			item.taken = true;
			continue;
		}

		if (deathTimer < 0.0f && DistXZ(playerPos, item.pos) < kPickupRange)
		{
			item.taken = true;
			PickUp(item);
		}
	}

	worldItems.erase(std::remove_if(worldItems.begin(), worldItems.end(),
		[](const WorldItem& item) { return item.taken; }), worldItems.end());
}

void Game::UpdateCombatTimers(float dt)
{
	attackTimer = Maxf(attackTimer - dt, 0.0f);
	swingTimer = Maxf(swingTimer - dt, 0.0f);
	hurtTimer = Maxf(hurtTimer - dt, 0.0f);
	playerFlash = Maxf(playerFlash - dt * 4.0f, 0.0f);
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

	Enemy e;
	e.type = type;
	e.level = enemyLevel;
	e.pos = pos;
	e.home = pos;
	e.moveTarget = pos;
	e.maxHealth = EnemyHealthAt(info, enemyLevel);
	e.health = e.maxHealth;
	e.yaw = combatRng.Range(0.0f, 2.0f * kPi);
	e.stateTimer = combatRng.Range(0.5f, 2.0f);
	e.chunkX = chunkX;
	e.chunkZ = chunkZ;
	e.spawnIndex = spawnIndex;
	enemies.push_back(e);
}

void Game::DropItem(int type, const Vec3& pos, int chunkX, int chunkZ, int spawnIndex)
{
	WorldItem item;
	item.type = type;
	item.pos = Vec3(pos.x, 0.0f, pos.z);
	item.phase = combatRng.Range(0.0f, 6.0f);
	item.chunkX = chunkX;
	item.chunkZ = chunkZ;
	item.spawnIndex = spawnIndex;
	worldItems.push_back(item);
}

void Game::PickUp(WorldItem& item)
{
	const ItemInfo& info = GetItemInfo(item.type);
	ShowMessage(info.pickupText, 3.5f);
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
		hasWeapon = true;
		break;
	case ITEM_RELIC:
		GrantXp((int)kRelicXp);
		break;
	default:
		++inventory[item.type];
		break;
	}
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
	AddPopup(playerPos + Vec3(0.0f, 2.0f, 0.0f), text, kHealColor);
}

void Game::UseWater()
{
	if (deathTimer >= 0.0f) return;

	if (inventory[ITEM_CLEAN_WATER] <= 0)
	{
		ShowMessage("No clean water left.", 2.0f);
		return;
	}

	--inventory[ITEM_CLEAN_WATER];
	sporeExposure = Maxf(sporeExposure - kWaterClear, 0.0f);
	ShowMessage("The water rinses the spores out. Breathing comes easier.", 2.5f);
}

void Game::GrantXp(int amount)
{
	char text[16];
	sprintf_s(text, sizeof(text), "+%d XP", amount);
	AddPopup(playerPos + Vec3(0.0f, 2.4f, 0.0f), text, kXpColor);

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

void Game::DrawEnemies()
{
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		const Enemy& e = enemies[i];
		const EnemyInfo& info = GetEnemyInfo(e.type);

		float scale = 1.0f + 0.06f * (float)(e.level - 1);
		float squash = e.alive ? 1.0f : 0.2f + 0.8f * Saturatef(e.stateTimer / kDyingTime);

		DrawParams params;
		params.phase = (float)(e.spawnIndex + 1) * 1.7f + e.home.x;
		params.flash = e.flash;

		// A reddening windup is the tell to roll.
		if (e.state == ENEMY_WINDUP)
		{
			float w = 1.0f - Saturatef(e.stateTimer / info.windup);
			params.tint = Vec3(1.0f, 1.0f - 0.55f * w, 1.0f - 0.55f * w);
			params.emissive = 0.25f * w;
		}

		Mat4 model = Mul(Mul(MatTranslate(e.pos), MatRotateY(e.yaw)), MatScale(Vec3(scale, scale * squash, scale)));
		if (e.alive) renderer->DrawShadow(e.pos, info.radius * 1.1f * scale);
		renderer->DrawModel(info.model, model, params);
	}
}

void Game::DrawItems()
{
	for (size_t i = 0; i < worldItems.size(); ++i)
	{
		const WorldItem& item = worldItems[i];

		DrawParams params;
		params.phase = item.phase;
		params.emissive = 0.35f;

		renderer->DrawShadow(item.pos, 0.3f);
		renderer->DrawModel(GetItemInfo(item.type).model, item.pos, 0.0f, Vec3(1.2f, 1.2f, 1.2f), params);
	}
}

void Game::DrawWeapon(const Mat4& root)
{
	// At rest the pipe hangs forward from the right hand; a swing sweeps it from over the shoulder.
	float angle = 2.7f;
	if (swingTimer > 0.0f)
	{
		float t = 1.0f - swingTimer / kSwingTime;
		t = 1.0f - (1.0f - t) * (1.0f - t);
		angle = Lerpf(-0.6f, 2.4f, t);
	}

	Mat4 hand = Mul(Mul(root, MatTranslate(Vec3(0.36f, 0.62f, 0.10f))), MatRotateX(angle));
	renderer->DrawModel(MODEL_PIPE, hand, DrawParams());
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

	for (size_t i = 0; i < enemies.size(); ++i)
	{
		const Enemy& e = enemies[i];
		if (!e.alive || e.health >= e.maxHealth) continue;

		const EnemyInfo& info = GetEnemyInfo(e.type);
		float sx, sy;
		if (!renderer->WorldToScreen(e.pos + Vec3(0.0f, 1.3f + info.radius, 0.0f), sx, sy)) continue;
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

	sprintf_s(buf, sizeof(buf), "%s    Herb x%d [Q]    Water x%d [R]",
			  hasWeapon ? "Rusty pipe" : "Bare hands", inventory[ITEM_HERB], inventory[ITEM_CLEAN_WATER]);
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
	float weapon = hasWeapon ? kPipeBonus : 0.0f;

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
	sprintf_s(buf, sizeof(buf), "Spore guard %4d%% -> %4d%%", (int)(100.0f / SporeResistance(stats) + 0.5f), (int)(100.0f / SporeResistance(preview) + 0.5f));
	renderer->DrawTexts(x + 300, y, buf, kHudInk, false);

	renderer->DrawTexts(x, (int)(py + ph) - 22, "W/S select    D add    A remove    E confirm    C close", kHudAccent, false);
}
