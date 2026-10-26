#include "stdafx.h"
#include "Game.h"

#include <cstdarg>
#include <cstdio>

namespace
{
	const char* kTutorialNames[] =
	{
		"WAKE", "FETCH_WATER", "BRING_WATER", "FORAGE", "BRING_FOOD", "EAT", "FETCH_TORCH", "BRING_TORCH",
		"DUSK", "WATCH_DEER", "BEDTIME", "FIND_GRANDMOTHER", "READ_LETTER", "PACK", "LEAVE", "DONE"
	};

	const char* kGuideNames[] =
	{
		"TAKE_PIPE", "FIGHT_MITES", "ASSIGN_STATS", "FIGHT_BOAR", "ASSIGN_AGAIN", "BUS_STOP", "FIND_WATER", "REACH_TOWN", "EXPLORE",
		"TOWN_WALL", "RIDGE", "ROOT", "HOME", "STOCK"
	};

	const char* kEnemyStateNames[] =
	{
		"IDLE", "CHASE", "WINDUP", "STRIKE", "RECOVER", "DYING"
	};

	template <size_t N>
	const char* NameAt(const char* (&names)[N], int index)
	{
		return (index >= 0 && index < (int)N) ? names[index] : "?";
	}

	void Append(std::string& out, const char* format, ...)
	{
		char buf[512];
		va_list args;
		va_start(args, format);
		vsnprintf(buf, sizeof(buf), format, args);
		va_end(args);
		out += buf;
	}

	void AppendVec(std::string& out, const char* key, const Vec3& v)
	{
		Append(out, "\"%s\":[%.2f,%.2f,%.2f]", key, v.x, v.y, v.z);
	}
}

std::string Game::MonitorJson() const
{
	std::string out;
	out.reserve(4096);

	int minutes = (int)(timeOfDay * 24.0f * 60.0f);
	Append(out, "{\"world\":{\"level\":\"%s\",\"day\":%d,\"clock\":\"%02d:%02d\",\"timeOfDay\":%.3f,\"timeScale\":%.1f,"
				"\"time\":%.1f,\"tick\":%d,\"sporeExposure\":%.3f,\"tutorial\":\"%s\",\"guide\":\"%s\",\"chunk\":[%d,%d],\"paused\":%s},",
		   level == LEVEL_VILLAGE ? "VILLAGE" : "ROUTE", day, minutes / 60, minutes % 60, timeOfDay, timeScale,
		   time, tick, sporeExposure, NameAt(kTutorialNames, tutorial), NameAt(kGuideNames, guide),
		   playerChunkX, playerChunkZ, MenuOpen() ? "true" : "false");

	static const char* kSeasonIds[SEASON_COUNT] = { "SPRING", "SUMMER", "AUTUMN", "WINTER" };
	static const char* kWeatherIds[WEATHER_COUNT] = { "CLEAR", "RAIN", "SNOW", "WIND" };
	Append(out, "\"weather\":{\"season\":\"%s\",\"seasonDay\":%d,\"weather\":\"%s\",\"next\":%.0f,"
				"\"rain\":%.2f,\"snow\":%.2f,\"wind\":%.2f,\"snowCover\":%.2f,\"wetness\":%.2f},",
		   kSeasonIds[season], seasonDay + 1, kWeatherIds[weather], weatherTimer,
		   rainLevel, snowLevel, windLevel, snowCover, wetness);

	const StorySites& sites = routeMap.Sites();
	int built = 0;
	for (int i = 0; i < kHomeProjects; ++i) built += homeBuilt[i] ? 1 : 0;
	Append(out, "\"story\":{\"root\":[%.0f,%.0f],\"ridge\":[%.0f,%.0f],\"parents\":[%.0f,%.0f],\"wallRead\":%s,\"ridgeClimbed\":%s,"
				"\"cityEntered\":%s,\"parentsFound\":%s,\"rootReached\":%s,\"homeBuilt\":%d,\"settled\":%s},",
		   sites.root.x, sites.root.z, sites.ridge.x, sites.ridge.z, sites.parents.x, sites.parents.z,
		   townWallRead ? "true" : "false", ridgeClimbed ? "true" : "false", cityEntered ? "true" : "false",
		   parentsFound ? "true" : "false", rootReached ? "true" : "false", built, settled ? "true" : "false");

	Append(out, "\"perf\":{\"fps\":%.1f,\"lowFps\":%.1f,\"worstMs\":%.1f,\"updateMs\":%.2f,\"renderMs\":%.2f,"
				"\"gpuShadow\":%.2f,\"gpuScene\":%.2f,\"gpuPost\":%.2f,\"gpuUi\":%.2f,\"drawCalls\":%d,\"spikes\":%d},",
		   perf.fps, perf.lowFps, perf.worstMs, perf.updateMs, perf.renderMs,
		   perf.gpu.shadow, perf.gpu.scene, perf.gpu.post, perf.gpu.ui, perf.drawCalls, perf.spikes);

	const Vec3 playerPos = player->Position();
	out += "\"player\":{";
	AppendVec(out, "pos", playerPos);
	Append(out, ",\"health\":%.1f,\"maxHealth\":%.1f,\"level\":%d,\"xp\":%d,\"xpNext\":%d,"
				"\"water\":%.1f,\"food\":%.1f,\"warmth\":%.1f,\"warmthActive\":%s},",
		   health, MaxHealth(stats), stats.level, stats.xp, XpToNextLevel(stats.level),
		   waterMeter, foodMeter, warmthMeter, warmthActive ? "true" : "false");

	// Only one of the two grandmothers is shown at a time; neither exists past the village.
	out += "\"grandma\":";
	if (grandmaNpc && grandmaNpc->visible)
	{
		out += "{\"state\":\"AWAKE\",";
		AppendVec(out, "pos", grandmaNpc->WorldPosition());
		out += "},";
	}
	else if (grandmaSleeper && grandmaSleeper->visible)
	{
		out += "{\"state\":\"ASLEEP\",";
		AppendVec(out, "pos", grandmaSleeper->WorldPosition());
		Append(out, ",\"visited\":%s},", grandmaSleeper->visited ? "true" : "false");
	}
	else
	{
		out += "null,";
	}

	out += "\"deer\":";
	if (deer)
	{
		out += "{";
		AppendVec(out, "pos", deer->WorldPosition());
		out += ",";
		AppendVec(out, "goal", deer->goal);
		Append(out, ",\"leading\":%s,\"arrived\":%s,\"distance\":%.1f},",
			   deer->leading ? "true" : "false", deer->arrived ? "true" : "false", DistXZ(deer->WorldPosition(), playerPos));
	}
	else
	{
		out += "null,";
	}

	out += "\"enemies\":[";
	std::vector<EnemyActor*> enemies = LiveEnemies();
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		const EnemyActor& e = *enemies[i];
		if (i > 0) out += ",";
		Append(out, "{\"type\":\"%s\",\"level\":%d,\"state\":\"%s\",\"health\":%.1f,\"maxHealth\":%.1f,\"provoked\":%s,\"distance\":%.1f,",
			   GetEnemyInfo(e.type).name, e.level, NameAt(kEnemyStateNames, e.state), e.health, e.maxHealth,
			   e.provoked ? "true" : "false", DistXZ(e.WorldPosition(), playerPos));
		AppendVec(out, "pos", e.WorldPosition());
		out += "}";
	}
	out += "]}";

	return out;
}
