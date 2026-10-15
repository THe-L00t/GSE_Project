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
