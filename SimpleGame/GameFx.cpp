#include "stdafx.h"
#include "Game.h"

#include <algorithm>

#include "Models.h"

namespace
{
	// Meter values where the screen starts to show the body: frost a little before the shiver message,
	// hunger and thirst at the same low mark the HUD warns at.
	const float kFrostFrom = 45.0f;
	const float kHungerFrom = 25.0f;
	const float kLowHealth = 0.33f;       // share of health below which the edges stay red

	const float kShakeSize = 0.22f;       // world units at the moment of the blow

	// Region colour casts: home is neutral, the open road a touch cool, the ginkgo town warm and gold.
	const Vec3 kVillageGrade(1.00f, 1.00f, 1.00f);
	const Vec3 kRouteGrade(0.97f, 1.00f, 1.03f);
	const Vec3 kTownGrade(1.06f, 1.00f, 0.86f);

	const float kCutawayFade = 0.2f;      // seconds for the view through a tree to open or close

	const int   kMaxFootprints = 64;
	const float kStride = 0.62f;          // ground covered between two prints
	const float kPrintLife = 20.0f;       // seconds until a print has faded away
	const float kSnowForPrints = 0.25f;   // snow cover or wetness the ground needs before it takes a print
	const float kWetForPrints = 0.35f;

	const int   kMaxWisps = 400;
	const float kGlintRange = 24.0f;      // relics further off neither twinkle nor shed glints
	const Vec3  kRelicLight(1.00f, 0.90f, 0.62f);
	const Vec3  kSporeLight(0.55f, 0.95f, 0.80f);

	const float kRippleTime = 1.1f;       // seconds the level-up wave takes to cross the screen
	const float kRingTime = 1.3f;         // and the rings of light on the ground to spread and fade
}

void Game::UpdateScreenFx(float dt)
{
	// A dream fragment is a dialog spoken by the dream; the screen drifts in slowly and wakes a little faster.
	bool dreaming = DialogOpen() && dialogSpeaker.find("꿈 조각") == 0;
	screenFx.dream = Approach(screenFx.dream, dreaming ? 1.0f : 0.0f, dreaming ? 1.4f : 2.2f, dt);

	// Grandmother's map notes read as a sketch on her paper; Hayul's memories as an old photograph.
	bool memo = DialogOpen() && dialogSpeaker == "할머니의 지도 메모";
	bool recall = DialogOpen() && (dialogSpeaker == "하율" || dialogSpeaker == "기억 속 할머니");
	screenFx.memo = Approach(screenFx.memo, memo ? 1.0f : 0.0f, 2.5f, dt);
	screenFx.recall = Approach(screenFx.recall, recall ? 1.0f : 0.0f, recall ? 1.6f : 2.4f, dt);

	// A level-up sends one ring of bent air out from the player in its first moments.
	float sinceLevelUp = kLevelUpTime - levelUpTimer;
	screenFx.ripple = (levelUpTimer > 0.0f && sinceLevelUp < kRippleTime) ? Maxf(sinceLevelUp / kRippleTime, 0.001f) : 0.0f;
	screenFx.rippleAt = player->Position() + Vec3(0.0f, 0.9f, 0.0f);

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

	// A blow reddens the edges and shakes the view; below a third of health the red stays, beating faintly.
	hurtFlash = Maxf(hurtFlash - dt * 1.6f, 0.0f);
	shake = Maxf(shake - dt * 3.5f, 0.0f);
	float low = 0.0f;
	if (level == LEVEL_ROUTE && deathTimer < 0.0f) low = Saturatef((kLowHealth - health / MaxHealth(stats)) / kLowHealth);
	screenFx.hurt = Maxf(hurtFlash, low * (0.25f + 0.12f * sinf(time * 4.0f)));

	// The cast eases over a few seconds, so stepping onto the road or into town reads as a change of light.
	Vec3 grade = kVillageGrade;
	if (level == LEVEL_ROUTE)
	{
		float town = SmoothStep(kTownGateZ - 14.0f, kTownGateZ + 4.0f, player->Position().z);
		grade = LerpV(kRouteGrade, kTownGrade, town);
	}
	screenFx.grade.x = Approach(screenFx.grade.x, grade.x, 0.8f, dt);
	screenFx.grade.y = Approach(screenFx.grade.y, grade.y, 0.8f, dt);
	screenFx.grade.z = Approach(screenFx.grade.z, grade.z, 0.8f, dt);

	// Open a view through whatever stands between the eye and the player, and close it again once clear.
	bool hidden = deathTimer < 0.0f && PlayerHidden();
	cutaway = hidden ? Minf(cutaway + dt / kCutawayFade, 1.0f) : Maxf(cutaway - dt / kCutawayFade, 0.0f);
}

bool Game::PlayerHidden() const
{
	// An object of height H standing s in front of the player, toward the eye, covers them on screen once
	// H reaches s * tan(elevation); sideways it has to overlap the player's width.
	Vec3 toEye(sinf(camera->Yaw()), 0.0f, cosf(camera->Yaw()));
	float lift = tanf(camera->elevation);
	Vec3 pos = player->Position();

	std::vector<PropActor*> props;
	SceneGraph::Collect(scene.Root(), ACTOR_PROP, props);
	for (size_t i = 0; i < props.size(); ++i)
	{
		OccluderSize o = ModelOccluder(props[i]->model);
		if (o.radius <= 0.0f) continue;

		const Vec3& s = props[i]->Scale();
		float reach = o.radius * Maxf(s.x, s.z);
		Vec3 d = props[i]->WorldPosition() - pos;
		float ahead = d.x * toEye.x + d.z * toEye.z;
		float side = fabsf(d.x * toEye.z - d.z * toEye.x);
		if (side > reach + 0.35f) continue;
		if (ahead < -reach * 0.5f || ahead > o.height * s.y / lift + reach) continue;
		return true;
	}
	return false;
}

Vec3 Game::ShakeOffset() const
{
	// Falls off with the square, so the jolt is sharp and the settling quick.
	float size = kShakeSize * shake * shake;
	return Vec3(sinf(time * 53.0f) * size, 0.0f, sinf(time * 41.0f + 1.3f) * size);
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

void Game::UpdateWorldFx(float dt)
{
	Vec3 playerPos = player->Position();

	// A relic lying out catches the light now and then, and sheds a glint that rises and goes out.
	std::vector<ItemActor*> items = LiveItems();
	for (size_t i = 0; i < items.size(); ++i)
	{
		ItemActor& item = *items[i];
		if (item.type != ITEM_RELIC || DistXZ(item.Position(), playerPos) > kGlintRange)
		{
			item.sparkle = 0.0f;
			continue;
		}
		item.sparkle = powf(Maxf(sinf(time * 2.6f + item.phase * 3.0f), 0.0f), 12.0f);
		if (!fxRng.Chance(dt * 2.5f)) continue;

		Wisp w;
		w.pos = item.Position() + Vec3(fxRng.Range(-0.3f, 0.3f), fxRng.Range(0.3f, 0.8f), fxRng.Range(-0.3f, 0.3f));
		w.vel = Vec3(0.0f, 0.35f, 0.0f);
		w.maxLife = w.life = 0.7f;
		w.tint = kRelicLight;
		w.size = 0.5f;
		w.glow = 1.6f;
		w.sway = 0.1f;
		w.plain = true;
		wisps.push_back(w);
	}

	// The pipe's end leaves a short streak of light along its arc, filled in between updates so it never breaks.
	if (player->swingTimer > 0.0f && player->weapon)
	{
		// The pose is otherwise only set while drawing; set it now so the tip is this update's.
		player->UpdatePose(deathTimer);
		const float* m = player->weapon->WorldMatrix().m;
		Vec3 tip(m[4] * 0.92f + m[12], m[5] * 0.92f + m[13], m[6] * 0.92f + m[14]);
		for (int i = 0; i < 3 && swingTipValid; ++i)
		{
			Wisp w;
			w.pos = LerpV(swingTip, tip, (float)(i + 1) / 3.0f);
			w.maxLife = w.life = 0.18f;
			w.tint = Vec3(0.85f, 0.95f, 1.00f);
			w.size = 0.55f;
			w.glow = 2.0f;
			w.drag = 0.0f;
			w.sway = 0.0f;
			w.plain = true;
			wisps.push_back(w);
		}
		swingTip = tip;
		swingTipValid = true;
	}
	else
	{
		swingTipValid = false;
	}

	// A roll kicks up dust behind it, or a spray of powder off fresh snow; soaked ground gives none.
	bool snowy = snowCover > kSnowForPrints;
	if (player->rollTimer > 0.0f && (snowy || wetness < kWetForPrints))
	{
		for (int k = 0; k < 2; ++k)
		{
			if (!fxRng.Chance(dt * 24.0f)) continue;
			Wisp w;
			w.pos = playerPos + Vec3(fxRng.Range(-0.25f, 0.25f), 0.1f, fxRng.Range(-0.25f, 0.25f));
			w.vel = player->rollDir * -0.8f + Vec3(fxRng.Range(-0.6f, 0.6f), fxRng.Range(0.6f, 1.2f), fxRng.Range(-0.6f, 0.6f));
			w.maxLife = w.life = fxRng.Range(0.45f, 0.7f);
			w.tint = snowy ? Vec3(1.00f, 1.02f, 1.05f) : Vec3(0.55f, 0.50f, 0.42f);
			w.size = fxRng.Range(1.0f, 1.6f);
			w.glow = snowy ? 0.15f : 0.0f;
			w.drag = 2.5f;
			w.lift = -1.5f;
			w.sway = 0.0f;
			w.plain = true;
			wisps.push_back(w);
		}
	}
}

void Game::DrawLevelUpRings()
{
	if (levelUpTimer <= 0.0f) return;

	// Two rings of spore light run out over the ground, and the ground under the feet glows a while longer.
	float since = kLevelUpTime - levelUpTimer;
	Vec3 pos = player->Position();
	for (int i = 0; i < 2; ++i)
	{
		float k = (since - 0.25f * (float)i) / kRingTime;
		if (k <= 0.0f || k >= 1.0f) continue;
		float size = 1.0f + 5.0f * (1.0f - (1.0f - k) * (1.0f - k));
		renderer->DrawDecal(pos, 0.0f, size, size, kSporeLight * 2.4f, (1.0f - k) * 0.9f, 0.12f);
	}
	renderer->DrawDecal(pos, 0.0f, 1.6f, 1.6f, kSporeLight * 1.8f, Saturatef(levelUpTimer / kLevelUpTime) * 0.5f);
}

void Game::UpdateWisps(float dt)
{
	Vec3 chest = player->Position() + Vec3(0.0f, 1.0f, 0.0f);
	for (size_t i = 0; i < wisps.size(); ++i)
	{
		Wisp& w = wisps[i];
		w.life -= dt;

		// Flung out first, then drawn in harder and harder; taken once it reaches the chest.
		if (w.seek > 0.0f)
		{
			Vec3 to = chest - w.pos;
			float dist = Length(to);
			if (dist < 0.25f)
			{
				w.life = 0.0f;
				continue;
			}
			float age = 1.0f - Saturatef(w.life / w.maxLife);
			Vec3 want = to * ((2.0f + 12.0f * age) / dist);
			w.vel = LerpV(w.vel, want, 1.0f - expf(-w.seek * 6.0f * age * dt));
		}

		w.vel = w.vel * (1.0f - w.drag * dt);
		w.vel.y += w.lift * dt;
		w.pos = w.pos + w.vel * dt;
		w.pos.x += sinf(time * 2.3f + (float)i * 1.7f) * w.sway * dt;
		if (w.pos.y < 0.04f) w.pos.y = 0.04f;
	}

	wisps.erase(std::remove_if(wisps.begin(), wisps.end(),
		[](const Wisp& w) { return w.life <= 0.0f; }), wisps.end());
	if ((int)wisps.size() > kMaxWisps) wisps.erase(wisps.begin(), wisps.begin() + ((int)wisps.size() - kMaxWisps));
}
