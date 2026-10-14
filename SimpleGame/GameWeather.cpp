#include "stdafx.h"
#include "Game.h"

#include <cstdio>

namespace
{
	const int kSeasonDays = 3;   // in-game days per season

	const char* kSeasonNames[SEASON_COUNT] = { "봄", "여름", "가을", "겨울" };
	const char* kWeatherNames[WEATHER_COUNT] = { "맑음", "비", "눈", "강풍" };

	const char* kSeasonArrivals[SEASON_COUNT] =
	{
		"봄이 온다. 눈 녹은 자리마다 새싹이 오른다.",
		"여름이다. 소나기가 잦아진다.",
		"가을이다. 바람이 거세지고 잎이 진다.",
		"겨울이다. 밤이 길어지고 눈이 내린다.",
	};

	// Odds of clear, rain, snow and gale for each season.
	const float kWeatherOdds[SEASON_COUNT][WEATHER_COUNT] =
	{
		{ 55.0f, 35.0f,  0.0f, 10.0f },
		{ 50.0f, 40.0f,  0.0f, 10.0f },
		{ 40.0f, 20.0f,  0.0f, 40.0f },
		{ 35.0f,  0.0f, 50.0f, 15.0f },
	};
}

void Game::AdvanceDay()
{
	if (++seasonDay < kSeasonDays) return;

	seasonDay = 0;
	season = (season + 1) % SEASON_COUNT;
	if (level == LEVEL_ROUTE) ShowMessage(kSeasonArrivals[season], 5.0f);
	RollWeather();
}

void Game::RollWeather()
{
	float total = 0.0f;
	for (int i = 0; i < WEATHER_COUNT; ++i) total += kWeatherOdds[season][i];

	float roll = weatherRng.Unit() * total;
	int next = WEATHER_CLEAR;
	for (int i = 0; i < WEATHER_COUNT; ++i)
	{
		roll -= kWeatherOdds[season][i];
		if (roll < 0.0f)
		{
			next = i;
			break;
		}
	}

	weatherTimer = weatherRng.Range(70.0f, 150.0f);
	windAngle = weatherRng.Range(0.0f, 2.0f * kPi);
	if (next == weather) return;
	weather = next;

	if (level != LEVEL_ROUTE) return;
	switch (weather)
	{
	case WEATHER_RAIN:
		ShowMessage("비가 내린다. 빗물이 포자를 씻어 내지만 몸이 식는다.", 4.5f);
		break;
	case WEATHER_SNOW:
		ShowMessage("눈이 내린다. 발이 무거워지고 몸이 빨리 식는다.", 4.5f);
		break;
	case WEATHER_WIND:
		ShowMessage(season == SEASON_AUTUMN ? "거센 바람에 낙엽이 날린다. 바람을 거스르면 걸음이 무겁다."
											: "거센 바람이 분다. 바람을 거스르면 걸음이 무겁다.", 4.5f);
		break;
	default:
		ShowMessage("날이 갠다.", 2.5f);
		break;
	}
}

void Game::UpdateWeather(float dt)
{
	// Weather runs on game time, so fast time (T) hurries it along with the sun.
	float gameDt = dt * timeScale;

	weatherTimer -= gameDt;
	if (weatherTimer <= 0.0f) RollWeather();

	float rainTarget = weather == WEATHER_RAIN ? (season == SEASON_SUMMER ? 1.0f : 0.75f) : 0.0f;
	float snowTarget = weather == WEATHER_SNOW ? 1.0f : 0.0f;
	float windTarget = weather == WEATHER_WIND ? 1.0f : (weather == WEATHER_CLEAR ? 0.1f : 0.3f);
	rainLevel = Approach(rainLevel, rainTarget, 0.25f, gameDt);
	snowLevel = Approach(snowLevel, snowTarget, 0.20f, gameDt);
	windLevel = Approach(windLevel, windTarget, 0.30f, gameDt);

	// Wrapped at 1000: every drift rate in Particle.vs turns that into a whole number of field widths.
	windDriftX = fmodf(windDriftX + sinf(windAngle) * windLevel * dt, 1000.0f);
	windDriftZ = fmodf(windDriftZ + cosf(windAngle) * windLevel * dt, 1000.0f);

	for (int i = 0; i < SEASON_COUNT; ++i)
		seasonWeight[i] = Approach(seasonWeight[i], i == season ? 1.0f : 0.0f, 0.05f, gameDt);

	// Winter keeps a frost on the ground; anything above it melts, faster in the warm seasons.
	float frost = season == SEASON_WINTER ? 0.2f : 0.0f;
	if (snowLevel > 0.05f)
		snowCover = Minf(snowCover + 0.02f * snowLevel * gameDt, 1.0f);
	else
		snowCover = Maxf(snowCover - (season == SEASON_WINTER ? 0.002f : 0.012f) * gameDt, frost);
	if (snowCover < frost) snowCover = Minf(snowCover + 0.01f * gameDt, frost);

	if (rainLevel > 0.05f)
		wetness = Minf(wetness + 0.05f * rainLevel * gameDt, 1.0f);
	else
		wetness = Maxf(wetness - (season == SEASON_SUMMER ? 0.02f : 0.01f) * gameDt, 0.0f);
}

float Game::WindAgainst(const Vec3& moveDir) const
{
	Vec3 blow(sinf(windAngle), 0.0f, cosf(windAngle));
	return -Dot(moveDir, blow);
}

void Game::DrawWeatherHud()
{
	char buf[128];
	sprintf_s(buf, sizeof(buf), "%s %d일째  ·  %s", kSeasonNames[season], seasonDay + 1, kWeatherNames[weather]);
	renderer->DrawTexts(32, 104, buf, kHudDim, false);
}
