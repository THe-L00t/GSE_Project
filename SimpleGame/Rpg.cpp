#include "stdafx.h"
#include "Rpg.h"

#include "Models.h"

namespace
{
	const char* kStatNames[STAT_COUNT] = { "Strength", "Vitality", "Agility", "Insight" };

	const char* kStatHints[STAT_COUNT] =
	{
		"+2.2 attack",
		"+12 max health",
		"+3% speed, quicker swings",
		"spores build up slower",
	};

	const ItemInfo kItems[ITEM_TYPE_COUNT] =
	{
		{ "Herb",        MODEL_HERB,        "A bitter herb. Press Q to eat it and heal." },
		{ "Clean water", MODEL_WATER_FLASK, "Clean water. Press R to rinse the spores out." },
		{ "Old relic",   MODEL_RELIC,       "An old-world relic. Turning it over teaches you something." },
		{ "Rusty pipe",  MODEL_PIPE_PICKUP, "A rusty pipe. Better than bare hands." },
	};

	// name, model, health, damage, speed, radius, aggro, attack range, windup, recover, lunge, xp, herb, water, relic
	const EnemyInfo kEnemies[ENEMY_TYPE_COUNT] =
	{
		{ "Spore mite", MODEL_MITE, 18.0f,  5.0f, 3.2f, 0.45f,  7.0f, 1.3f, 0.45f, 0.8f,  7.0f,  9, 0.35f, 0.10f, 0.05f },
		{ "Husk",       MODEL_HUSK, 40.0f,  9.0f, 2.2f, 0.45f,  8.0f, 1.6f, 0.70f, 1.0f,  0.0f, 18, 0.30f, 0.25f, 0.15f },
		{ "Moss boar",  MODEL_BOAR, 85.0f, 14.0f, 2.6f, 0.95f, 10.0f, 5.5f, 0.85f, 1.4f, 12.0f, 40, 0.60f, 0.20f, 0.25f },
	};
}

const char* StatName(int id)
{
	return (id >= 0 && id < STAT_COUNT) ? kStatNames[id] : "";
}

const char* StatHint(int id)
{
	return (id >= 0 && id < STAT_COUNT) ? kStatHints[id] : "";
}

int XpToNextLevel(int level)
{
	return 10 + 15 * level;
}

int GainXp(CharacterStats& stats, int amount)
{
	int gained = 0;
	stats.xp += amount;
	while (stats.xp >= XpToNextLevel(stats.level))
	{
		stats.xp -= XpToNextLevel(stats.level);
		++stats.level;
		stats.unspentPoints += 3;
		++gained;
	}
	return gained;
}

float MaxHealth(const CharacterStats& stats)
{
	return 40.0f + 12.0f * (float)stats.stat[STAT_VITALITY] + 4.0f * (float)(stats.level - 1);
}

float AttackPower(const CharacterStats& stats, float weaponBonus)
{
	return 4.0f + 2.2f * (float)stats.stat[STAT_STRENGTH] + weaponBonus;
}

float MoveSpeedScale(const CharacterStats& stats)
{
	return 1.0f + 0.03f * (float)(stats.stat[STAT_AGILITY] - 3);
}

float AttackCooldown(const CharacterStats& stats)
{
	return 0.55f / (1.0f + 0.06f * (float)(stats.stat[STAT_AGILITY] - 3));
}

float SporeResistance(const CharacterStats& stats)
{
	return 1.0f / (1.0f + 0.12f * (float)(stats.stat[STAT_INSIGHT] - 3));
}

const ItemInfo& GetItemInfo(int type)
{
	if (type < 0 || type >= ITEM_TYPE_COUNT) type = ITEM_HERB;
	return kItems[type];
}

const EnemyInfo& GetEnemyInfo(int type)
{
	if (type < 0 || type >= ENEMY_TYPE_COUNT) type = ENEMY_SPORE_MITE;
	return kEnemies[type];
}

float EnemyHealthAt(const EnemyInfo& info, int level)
{
	return info.maxHealth * (1.0f + 0.25f * (float)(level - 1));
}

float EnemyDamageAt(const EnemyInfo& info, int level)
{
	return info.damage * (1.0f + 0.20f * (float)(level - 1));
}

int EnemyXpAt(const EnemyInfo& info, int level)
{
	return (int)((float)info.xp * (1.0f + 0.30f * (float)(level - 1)) + 0.5f);
}
