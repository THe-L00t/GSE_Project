#include "stdafx.h"
#include "Rpg.h"

#include "Models.h"

namespace
{
	const char* kStatNames[STAT_COUNT] = { "힘", "체력", "민첩", "감각" };

	const char* kStatHints[STAT_COUNT] =
	{
		"공격력 +2.2",
		"최대 체력 +12",
		"이동 속도 +3%, 휘두르기가 빨라짐",
		"경험치 +6%, 전리품이 늘어남",
	};

	const ItemInfo kItems[ITEM_TYPE_COUNT] =
	{
		{ "약초",         MODEL_HERB,        "쓴 약초. Q로 먹으면 체력이 회복된다." },
		{ "물병",         MODEL_WATER_FLASK, "깨끗한 물이 든 병. R로 마시면 포자가 씻겨 나간다." },
		{ "옛 유물",      MODEL_RELIC,       "옛 세상의 유물." },
		{ "녹슨 파이프",  MODEL_PIPE_PICKUP, "녹슨 파이프. 맨손보다는 낫다." },
		{ "열매",         MODEL_BERRY_RED,   "빨간 열매 한 줌. F로 먹는다." },
		{ "무",           MODEL_VEGETABLE,   "텃밭에서 뽑은 무. F로 먹는다." },
		{ "복숭아 통조림", MODEL_CANNED_FOOD, "개화 전에 밀봉된 복숭아 통조림. F로 먹는다." },
	};

	// name, model, health, damage, speed, radius, aggro, attack range, windup, recover, lunge, xp, herb, water, relic, lore
	const EnemyInfo kEnemies[ENEMY_TYPE_COUNT] =
	{
		{ "포자 진드기", MODEL_MITE, 18.0f,  5.0f, 3.2f, 0.45f, 7.0f, 1.3f, 0.45f, 0.8f,  7.0f,  9, 0.35f, 0.10f, 0.05f,
		  "포자 진드기: 포자로 배가 부푼 진드기. 지나간 자리의 공기가 달큰하고 무거워진다." },
		{ "허물",        MODEL_HUSK, 40.0f,  9.0f, 2.2f, 0.45f, 8.0f, 1.6f, 0.70f, 1.0f,  0.0f, 18, 0.30f, 0.25f, 0.15f,
		  "허물: 포자가 입는 법을 배운 빈 외투. 안에는 아무도 없다." },
		{ "이끼 멧돼지", MODEL_BOAR, 85.0f, 14.0f, 2.6f, 0.95f, 3.5f, 5.5f, 0.85f, 1.4f, 12.0f, 40, 0.60f, 0.20f, 0.25f,
		  "이끼 멧돼지: 놀랐을 때만 돌진한다. 멀찍이 돌아가거나, 버텨라." },
	};

	const char* kRelicNames[kRelicCount] =
	{
		"손전등", "복숭아 통조림", "손목시계", "휴대용 라디오",
		"버스표", "가족사진", "스마트폰", "학교 배지",
	};

	const char* kRelicTexts[kRelicCount] =
	{
		"스위치를 누르면 어둠이 물러난다. 배터리는 다시 채울 수 없다.",
		"개화 전에 밀봉됐다. 지금 자라는 어떤 것보다 달다.",
		"아직 째깍거린다. 무엇을 향해 서둘렀는지 기억하는 이는 없다.",
		"이제는 잡음뿐. 그 아래로 가끔, 숨소리 같은 것이 들린다.",
		"32번 국도, 성인 1명. 돌아오는 버스는 끝내 오지 않았다.",
		"햇빛에 눈을 찡그린 네 사람. 뒤의 집은 이제 언덕이 되었다.",
		"검은 거울. 사람들은 한때 이 안에 온 세상을 넣고 다녔다.",
		"은행나무읍 중학교. 배지 속 은행나무만이 아직 자라고 있다.",
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
