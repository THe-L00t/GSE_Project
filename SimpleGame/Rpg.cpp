#include "stdafx.h"
#include "Rpg.h"

#include "Models.h"

namespace
{
	const char* kStatNames[STAT_COUNT] = { "Strength", "Vitality", "Agility", "Senses" };

	const char* kStatHints[STAT_COUNT] =
	{
		"+2.2 attack",
		"+12 max health",
		"+3% speed, quicker swings",
		"+6% XP, creatures drop more",
	};

	const ItemInfo kItems[ITEM_TYPE_COUNT] =
	{
		{ "Herb",           MODEL_HERB,        "A bitter herb. Press Q to eat it and heal." },
		{ "Water flask",    MODEL_WATER_FLASK, "A flask of clean water. Press R to drink and rinse the spores out." },
		{ "Old relic",      MODEL_RELIC,       "An old-world relic." },
		{ "Rusty pipe",     MODEL_PIPE_PICKUP, "A rusty pipe. Better than bare hands." },
		{ "Berries",        MODEL_BERRY_RED,   "A handful of red berries. Press F to eat." },
		{ "Radish",         MODEL_VEGETABLE,   "A radish from the patch. Press F to eat." },
		{ "Canned peaches", MODEL_CANNED_FOOD, "A can of peaches, sealed before the Bloom. Press F to eat." },
	};

	// name, model, health, damage, speed, radius, aggro, attack range, windup, recover, lunge, xp, herb, water, relic, lore
	const EnemyInfo kEnemies[ENEMY_TYPE_COUNT] =
	{
		{ "Spore mite", MODEL_MITE, 18.0f,  5.0f, 3.2f, 0.45f, 7.0f, 1.3f, 0.45f, 0.8f,  7.0f,  9, 0.35f, 0.10f, 0.05f,
		  "Spore mite: a tick fat with spores. Where it walks, the air turns sweet and heavy." },
		{ "Husk",       MODEL_HUSK, 40.0f,  9.0f, 2.2f, 0.45f, 8.0f, 1.6f, 0.70f, 1.0f,  0.0f, 18, 0.30f, 0.25f, 0.15f,
		  "Husk: an empty coat the spores have learned to wear. No one is inside." },
		{ "Moss boar",  MODEL_BOAR, 85.0f, 14.0f, 2.6f, 0.95f, 3.5f, 5.5f, 0.85f, 1.4f, 12.0f, 40, 0.60f, 0.20f, 0.25f,
		  "Moss boar: it only charges when startled. Walk wide, or stand your ground." },
	};

	const char* kRelicNames[kRelicCount] =
	{
		"Hand torch", "Canned peaches", "Wristwatch", "Pocket radio",
		"Bus ticket", "Family photo", "Smartphone", "School badge",
	};

	const char* kRelicTexts[kRelicCount] =
	{
		"Push the switch and the dark steps back. The battery will not come back.",
		"Sealed before the Bloom. Sweeter than anything that grows now.",
		"Still ticking. Nobody remembers what it was hurrying toward.",
		"Only static now, and under it, sometimes, something like breathing.",
		"Route 32, one adult. The bus never came back for the return.",
		"Four people squinting into the sun. The house behind them is a hill now.",
		"A black mirror. People once carried the whole world in one of these.",
		"Ginkgo Town Middle School. The ginkgo on it is the only thing still growing.",
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

float XpScale(const CharacterStats& stats)
{
	return 1.0f + 0.06f * (float)(stats.stat[STAT_SENSES] - 3);
}

float FindScale(const CharacterStats& stats)
{
	return 1.0f + 0.08f * (float)(stats.stat[STAT_SENSES] - 3);
}

float NatureGuard(int natureInsight)
{
	return 1.0f / (1.0f + 0.15f * (float)natureInsight);
}

float FoodValue(int itemType)
{
	switch (itemType)
	{
	case ITEM_BERRIES:     return 15.0f;
	case ITEM_VEGETABLE:   return 20.0f;
	case ITEM_CANNED_FOOD: return 45.0f;
	default:               return 0.0f;
	}
}

bool TakesBagSlot(int itemType)
{
	return itemType != ITEM_RELIC && itemType != ITEM_RUSTY_PIPE;
}

const char* RelicName(int id)
{
	return (id >= 0 && id < kRelicCount) ? kRelicNames[id] : "";
}

const char* RelicText(int id)
{
	return (id >= 0 && id < kRelicCount) ? kRelicTexts[id] : "";
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
