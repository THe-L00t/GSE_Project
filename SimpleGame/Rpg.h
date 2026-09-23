#pragma once

// Character growth, items and enemies: data and rules only, no rendering.

enum StatId
{
	STAT_STRENGTH,
	STAT_VITALITY,
	STAT_AGILITY,
	STAT_SENSES,
	STAT_COUNT
};

struct CharacterStats
{
	int level = 1;
	int xp = 0;                  // progress inside the current level
	int unspentPoints = 0;
	int stat[STAT_COUNT] = { 3, 3, 3, 3 };
};

const char* StatName(int id);
const char* StatHint(int id);    // what one point buys, shown in the stat panel

int   XpToNextLevel(int level);
int   GainXp(CharacterStats& stats, int amount);   // returns the number of levels gained

float MaxHealth(const CharacterStats& stats);
float AttackPower(const CharacterStats& stats, float weaponBonus);
float MoveSpeedScale(const CharacterStats& stats);
float AttackCooldown(const CharacterStats& stats);
float XpScale(const CharacterStats& stats);       // Senses: more XP from every source
float FindScale(const CharacterStats& stats);     // Senses: creatures drop more

// Spore resistance belongs to Nature Insight alone: a multiplier on spore exposure gain.
float NatureGuard(int natureInsight);

enum ItemType
{
	ITEM_HERB,
	ITEM_CLEAN_WATER,
	ITEM_RELIC,
	ITEM_RUSTY_PIPE,
	ITEM_BERRIES,
	ITEM_VEGETABLE,
	ITEM_CANNED_FOOD,
	ITEM_TYPE_COUNT
};

// How much of the food meter one item fills; zero for anything that is not food.
float FoodValue(int itemType);

// Items that take a slot in the bag.
bool TakesBagSlot(int itemType);

const int kRelicCount = 8;

enum RelicId
{
	RELIC_TORCH,
	RELIC_PEACHES,
	RELIC_WATCH,
	RELIC_RADIO,
	RELIC_TICKET,
	RELIC_PHOTO,
	RELIC_PHONE,
	RELIC_BADGE
};

const char* RelicName(int id);
const char* RelicText(int id);

struct ItemInfo
{
	const char* name;
	int         model;
	const char* pickupText;
};

const ItemInfo& GetItemInfo(int type);

enum EnemyType
{
	ENEMY_SPORE_MITE,
	ENEMY_HUSK,
	ENEMY_MOSS_BOAR,
	ENEMY_TYPE_COUNT
};

struct EnemyInfo
{
	const char* name;
	int   model;
	float maxHealth;
	float damage;
	float speed;
	float radius;
	float aggroRange;
	float attackRange;
	float windup;        // telegraph before the hit lands
	float recover;       // pause after an attack
	float lunge;         // dash speed during the strike, zero for a standing swing
	int   xp;
	float herbChance;
	float waterChance;
	float relicChance;
	const char* lore;    // shown the first time the creature is met
};

const EnemyInfo& GetEnemyInfo(int type);

float EnemyHealthAt(const EnemyInfo& info, int level);
float EnemyDamageAt(const EnemyInfo& info, int level);
int   EnemyXpAt(const EnemyInfo& info, int level);
