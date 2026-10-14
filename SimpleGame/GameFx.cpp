#include "stdafx.h"
#include "Game.h"

#include <algorithm>

namespace
{
	// Meter values where the screen starts to show the body: frost a little before the shiver message,
	// hunger and thirst at the same low mark the HUD warns at.
	const float kFrostFrom = 45.0f;
	const float kHungerFrom = 25.0f;

	const int   kMaxFootprints = 64;
	const float kStride = 0.62f;          // ground covered between two prints
	const float kPrintLife = 20.0f;       // seconds until a print has faded away
	const float kSnowForPrints = 0.25f;   // snow cover or wetness the ground needs before it takes a print
	const float kWetForPrints = 0.35f;
}

void Game::UpdateScreenFx(float dt)
{
	// A dream fragment is a dialog spoken by the dream; the screen drifts in slowly and wakes a little faster.
	bool dreaming = DialogOpen() && dialogSpeaker.find("꿈 조각") == 0;
	screenFx.dream = Approach(screenFx.dream, dreaming ? 1.0f : 0.0f, dreaming ? 1.4f : 2.2f, dt);

	// The body only shows out on the road, and not while the long sleep takes over the screen.
	float frost = 0.0f;
	float hunger = 0.0f;
	if (level == LEVEL_ROUTE && deathTimer < 0.0f)
	{
		if (warmthActive) frost = Saturatef((kFrostFrom - warmthMeter) / kFrostFrom);
		float starving = Saturatef((kHungerFrom - foodMeter) / kHungerFrom);
		float parched = Saturatef((kHungerFrom - waterMeter) / kHungerFrom);
		hunger = Maxf(starving, parched);
	}
	screenFx.frost = Approach(screenFx.frost, frost, 1.5f, dt);
	screenFx.hunger = Approach(screenFx.hunger, hunger, 1.5f, dt);
}

void Game::UpdateFootprints()
{
	footprints.erase(std::remove_if(footprints.begin(), footprints.end(),
		[this](const Footprint& f) { return time - f.born > kPrintLife; }), footprints.end());

	Vec3 pos = player->Position();
	float moved = DistXZ(pos, lastPrintPos);

	// A jump in position is a level change or a wake at the safe point, not a walk.
	if (moved > 3.0f || deathTimer >= 0.0f)
	{
		lastPrintPos = pos;
		return;
	}
	if (moved < kStride) return;
	lastPrintPos = pos;

	// Snow takes the deeper print; mud only shows once the rain has really soaked in. A roll leaves a smear, not steps.
	bool snowy = snowCover > kSnowForPrints;
	bool muddy = wetness > kWetForPrints;
	if ((!snowy && !muddy) || player->rollTimer > 0.0f) return;

	float yaw = player->Yaw();
	float side = printLeft ? -0.13f : 0.13f;
	printLeft = !printLeft;

	Footprint f;
	f.pos = pos + Vec3(cosf(yaw), 0.0f, -sinf(yaw)) * side;
	f.yaw = yaw;
	f.born = time;
	f.strength = snowy ? 0.30f + 0.25f * snowCover : 0.20f + 0.20f * wetness;
	f.color = snowy ? Vec3(0.30f, 0.36f, 0.48f) : Vec3(0.06f, 0.05f, 0.04f);
	footprints.push_back(f);
	if ((int)footprints.size() > kMaxFootprints) footprints.erase(footprints.begin());
}

void Game::DrawFootprints()
{
	for (size_t i = 0; i < footprints.size(); ++i)
	{
		const Footprint& f = footprints[i];
		float fade = 1.0f - Saturatef((time - f.born) / kPrintLife);
		renderer->DrawDecal(f.pos, f.yaw, 0.14f, 0.28f, f.color, f.strength * fade);
	}
}
