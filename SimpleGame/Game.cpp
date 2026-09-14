#include "stdafx.h"
#include "Game.h"

#include <cstdio>
#include <cstring>

#include "Models.h"

// On-screen text stays ASCII: the bitmap fonts freeglut ships cannot draw Hangul.

namespace
{
	// Must match roadCenter() in Shaders/Lit.fs so props line up with the painted road.
	float RoadCenter(float z) { return sinf(z * 0.05f) * 3.0f; }

	const float kPlayerRadius = 0.38f;
	const float kWalkSpeed = 4.2f;
	const float kRollSpeed = 12.0f;
	const float kRollTime = 0.28f;
	const float kRollCooldown = 0.70f;
	const float kInteractRange = 2.3f;

	const float kExitZ = 26.0f;

	const Vec3 kUnitScale(1.0f, 1.0f, 1.0f);
}

Game::Game(Renderer* r)
	: renderer(r)
{
	camTarget = playerPos;
	BuildWorld();
}

void Game::AddProp(int model, const Vec3& pos, const Vec3& scale, float yaw, float halfX, float halfZ)
{
	Prop p;
	p.model = model;
	p.pos = pos;
	p.scale = scale;
	p.yaw = yaw;
	p.halfX = halfX;
	p.halfZ = halfZ;
	props.push_back(p);
}

void Game::AddHouse(const Vec3& pos, float w, float h, float d, float yaw, int model)
{
	AddProp(model, pos, Vec3(w, h, d), yaw, w * 0.5f, d * 0.5f);
}

void Game::AddTree(const Vec3& pos, float scale)
{
	AddProp(MODEL_TREE, pos, Vec3(scale, scale, scale), 0.0f, 0.225f * scale, 0.225f * scale);
}

void Game::AddFence(const Vec3& from, const Vec3& to)
{
	Vec3 d = to - from;
	float len = Length(d);
	int posts = (int)(len / 1.5f) + 1;
	for (int i = 0; i <= posts; ++i)
	{
		float t = (float)i / (float)posts;
		AddProp(MODEL_FENCE_POST, LerpV(from, to, t), kUnitScale, 0.0f, 0.0f, 0.0f);
	}

	Vec3 mid = LerpV(from, to, 0.5f);
	AddProp(MODEL_FENCE_RAIL, Vec3(mid.x, 0.62f, mid.z), Vec3(0.08f, 0.12f, len), atan2f(d.x, d.z), 0.0f, 0.0f);
}

void Game::BuildWorld()
{
	// Grandmother's house
	AddHouse(Vec3(-2.0f, 0.0f, -10.0f), 7.0f, 3.6f, 6.0f, 0.0f, MODEL_HOUSE_A);
	AddHouse(Vec3(9.5f, 0.0f, -7.0f), 5.5f, 3.2f, 5.0f, 0.12f, MODEL_HOUSE_B);
	AddHouse(Vec3(-10.5f, 0.0f, 3.0f), 5.0f, 3.0f, 4.6f, -0.15f, MODEL_HOUSE_A);
	AddHouse(Vec3(9.0f, 0.0f, 8.5f), 5.0f, 3.1f, 5.0f, 0.05f, MODEL_HOUSE_B);
	AddHouse(Vec3(-7.0f, 0.0f, 16.5f), 4.6f, 2.9f, 4.4f, 0.20f, MODEL_HOUSE_A);

	AddProp(MODEL_WELL, Vec3(2.5f, 0.0f, -2.0f), kUnitScale, 0.0f, 0.95f, 0.95f);

	// Trees stay clear of the road painted by Lit.fs.
	AddTree(Vec3(-6.5f, 0.0f, -17.0f), 1.15f);
	AddTree(Vec3(6.5f, 0.0f, -15.5f), 0.95f);
	AddTree(Vec3(15.0f, 0.0f, -2.0f), 1.20f);
	AddTree(Vec3(-13.5f, 0.0f, 10.0f), 1.05f);
	AddTree(Vec3(14.0f, 0.0f, 18.0f), 1.10f);
	AddTree(Vec3(-12.0f, 0.0f, -4.0f), 0.90f);
	AddTree(Vec3(17.0f, 0.0f, 9.0f), 1.00f);
	AddTree(Vec3(-8.0f, 0.0f, 22.0f), 1.15f);
	AddTree(Vec3(11.0f, 0.0f, 24.0f), 0.95f);

	AddFence(Vec3(-6.0f, 0.0f, -5.5f), Vec3(-6.0f, 0.0f, 1.5f));
	AddFence(Vec3(-6.0f, 0.0f, 1.5f), Vec3(-1.0f, 0.0f, 1.5f));
	AddFence(Vec3(12.5f, 0.0f, 4.0f), Vec3(12.5f, 0.0f, 12.0f));

	AddProp(MODEL_TRUCK, Vec3(11.5f, 0.0f, 15.0f), kUnitScale, 0.30f, 1.05f, 2.2f);

	// Road sign toward Route 32
	const float signZ = 24.5f;
	AddProp(MODEL_SIGN, Vec3(RoadCenter(signZ) + 3.2f, 0.0f, signZ), kUnitScale, 0.0f, 0.0f, 0.0f);
	props.back().emissive = 0.10f;

	Sleeper grandma;
	grandma.pos = Vec3(-2.0f, 0.0f, -6.2f);
	grandma.yaw = 0.35f;
	grandma.isGrandma = true;
	grandma.phase = 0.0f;
	sleepers.push_back(grandma);

	const float others[4][3] =
	{
		{ 8.5f, -3.5f, 1.20f },
		{ -8.5f, 6.0f, -0.60f },
		{ 6.5f, 11.0f, 2.10f },
		{ -4.5f, 17.5f, 0.80f },
	};
	for (int i = 0; i < 4; ++i)
	{
		Sleeper s;
		s.pos = Vec3(others[i][0], 0.0f, others[i][1]);
		s.yaw = others[i][2];
		s.phase = 1.3f * (float)(i + 1);
		sleepers.push_back(s);
	}

	letterPos = Vec3(-0.6f, 0.0f, -6.0f);
}

void Game::Update(float dt, const bool* keys)
{
	time += dt;

	if (!letterOpen)
		UpdatePlayer(dt, keys);

	UpdateCamera(dt);
	UpdateWorldState(dt);
	UpdateInteractionTarget();

	if (messageTimer > 0.0f) messageTimer -= dt;
	titleTimer += dt;
	if (endingTimer >= 0.0f) endingTimer += dt;
}

void Game::UpdatePlayer(float dt, const bool* keys)
{
	// Camera relative: W always moves up the screen.
	Vec3 forward(-sinf(camYaw), 0.0f, -cosf(camYaw));
	Vec3 right(cosf(camYaw), 0.0f, -sinf(camYaw));

	Vec3 wish;
	if (keys['w']) wish = wish + forward;
	if (keys['s']) wish = wish - forward;
	if (keys['d']) wish = wish + right;
	if (keys['a']) wish = wish - right;

	float wishLen = Length(wish);
	if (wishLen > 0.001f) wish = wish * (1.0f / wishLen);

	if (rollCooldown > 0.0f) rollCooldown -= dt;

	if (rollTimer > 0.0f)
	{
		// The roll commits to its direction; steering during it would rob the weight.
		rollTimer -= dt;
		float k = Saturatef(rollTimer / kRollTime);
		float speed = kRollSpeed * (0.35f + 0.65f * k);
		playerPos = playerPos + rollDir * (speed * dt);
		rollAngle += dt / kRollTime * 2.0f * kPi;
		if (rollTimer <= 0.0f)
		{
			rollTimer = 0.0f;
			rollAngle = 0.0f;
		}
	}
	else if (wishLen > 0.001f)
	{
		playerPos = playerPos + wish * (kWalkSpeed * dt);
		playerYaw = atan2f(wish.x, wish.z);
		walkPhase += dt * 9.0f;
	}
	else
	{
		walkPhase = Approach(walkPhase, 0.0f, 6.0f, dt);
	}

	ResolveCollisions();

	// Clamp to the field, but leave the south open for the exit.
	playerPos.x = Clampf(playerPos.x, -30.0f, 30.0f);
	playerPos.z = Clampf(playerPos.z, -28.0f, 32.0f);
	playerPos.y = 0.0f;

	if (stage == QUEST_LEAVE_VILLAGE && playerPos.z > kExitZ)
	{
		stage = QUEST_DONE;
		endingTimer = 0.0f;
		ShowMessage("", 0.0f);
	}
}

void Game::ResolveCollisions()
{
	// Axis-aligned pushes. Prop yaw is small enough that ignoring it reads fine.
	for (size_t i = 0; i < props.size(); ++i)
	{
		const Prop& p = props[i];
		if (p.halfX <= 0.0f || p.halfZ <= 0.0f) continue;

		float hx = p.halfX + kPlayerRadius;
		float hz = p.halfZ + kPlayerRadius;
		float dx = playerPos.x - p.pos.x;
		float dz = playerPos.z - p.pos.z;

		if (fabsf(dx) < hx && fabsf(dz) < hz)
		{
			float penX = hx - fabsf(dx);
			float penZ = hz - fabsf(dz);
			if (penX < penZ)
				playerPos.x += (dx < 0.0f ? -penX : penX);
			else
				playerPos.z += (dz < 0.0f ? -penZ : penZ);
		}
	}

	float hx = waterSizeX * 0.5f + kPlayerRadius;
	float hz = waterSizeZ * 0.5f + kPlayerRadius;
	float dx = playerPos.x - waterCenter.x;
	float dz = playerPos.z - waterCenter.z;
	if (fabsf(dx) < hx && fabsf(dz) < hz)
	{
		float penX = hx - fabsf(dx);
		float penZ = hz - fabsf(dz);
		if (penX < penZ)
			playerPos.x += (dx < 0.0f ? -penX : penX);
		else
			playerPos.z += (dz < 0.0f ? -penZ : penZ);
	}
}

void Game::UpdateCamera(float dt)
{
	camTarget = LerpV(camTarget, playerPos, 1.0f - expf(-6.0f * dt));
}

void Game::UpdateWorldState(float dt)
{
	timeOfDay += (dt * timeScale) / dayLength;
	while (timeOfDay >= 1.0f) timeOfDay -= 1.0f;

	// Nature Insight slows exposure; the water's edge clears it.
	float insight = 1.0f - 0.18f * (float)fragments;
	if (insight < 0.3f) insight = 0.3f;

	float rate = 0.028f * insight;

	float nearWaterX = Maxf(fabsf(playerPos.x - waterCenter.x) - waterSizeX * 0.5f, 0.0f);
	float nearWaterZ = Maxf(fabsf(playerPos.z - waterCenter.z) - waterSizeZ * 0.5f, 0.0f);
	float waterDist = sqrtf(nearWaterX * nearWaterX + nearWaterZ * nearWaterZ);
	if (waterDist < 4.5f) rate = -0.075f;

	// Rolling holds your breath.
	if (rollTimer > 0.0f) rate = Minf(rate, 0.0f);

	sporeExposure = Saturatef(sporeExposure + rate * dt);
}

void Game::UpdateInteractionTarget()
{
	targetSleeper = -1;
	targetLetter = false;
	prompt.clear();

	if (letterOpen)
	{
		prompt = "[E]  Close";
		return;
	}

	// The letter wins over the sleeper it lies beside.
	if (letterFound && stage == QUEST_READ_LETTER &&
		DistXZ(playerPos, letterPos) < kInteractRange)
	{
		targetLetter = true;
		prompt = "[E]  Read the letter";
		return;
	}

	float best = kInteractRange;
	for (size_t i = 0; i < sleepers.size(); ++i)
	{
		float d = DistXZ(playerPos, sleepers[i].pos);
		if (d < best)
		{
			best = d;
			targetSleeper = (int)i;
		}
	}

	if (targetSleeper >= 0)
	{
		const Sleeper& s = sleepers[targetSleeper];
		if (s.isGrandma)
			prompt = (stage == QUEST_FIND_GRANDMOTHER) ? "[E]  Look at grandmother" : "[E]  Sit with her";
		else
			prompt = s.visited ? "" : "[E]  Rest beside them";
	}
}

void Game::TryInteract()
{
	if (letterOpen)
	{
		letterOpen = false;
		if (stage == QUEST_READ_LETTER)
		{
			stage = QUEST_LEAVE_VILLAGE;
			ShowMessage("Follow the road south, out of the village.", 5.0f);
		}
		return;
	}

	if (targetLetter)
	{
		letterOpen = true;
		return;
	}

	if (targetSleeper < 0) return;

	Sleeper& s = sleepers[targetSleeper];

	if (s.isGrandma)
	{
		if (stage == QUEST_FIND_GRANDMOTHER)
		{
			stage = QUEST_READ_LETTER;
			letterFound = true;
			ShowMessage("She is breathing. Moss has started at her fingertips. A letter lies beside her.", 6.5f);
		}
		else
		{
			ShowMessage("She is breathing. She will not wake.", 3.5f);
		}
		return;
	}

	if (!s.visited)
	{
		s.visited = true;
		++fragments;
		char buf[128];
		sprintf_s(buf, sizeof(buf), "A dream fragment: the first spring, seen from someone else's eyes.  Nature Insight %d", fragments);
		ShowMessage(buf, 5.0f);
	}
	else
	{
		ShowMessage("Their sleep is quiet.", 2.5f);
	}
}

void Game::OnKeyDown(unsigned char key)
{
	if (key >= 'A' && key <= 'Z') key = key - 'A' + 'a';

	if (key == 27) // ESC
	{
		quit = true;
		return;
	}

	if (key == 'e')
	{
		TryInteract();
		return;
	}

	if (key == ' ')
	{
		if (letterOpen) return;
		if (rollTimer > 0.0f || rollCooldown > 0.0f) return;

		rollTimer = kRollTime;
		rollCooldown = kRollCooldown;
		rollAngle = 0.0f;
		rollDir = Vec3(sinf(playerYaw), 0.0f, cosf(playerYaw));
		return;
	}

	if (key == 't')
	{
		timeScale = (timeScale > 1.5f) ? 1.0f : 12.0f;
		ShowMessage(timeScale > 1.5f ? "Time: fast" : "Time: normal", 2.0f);
		return;
	}
}

void Game::ShowMessage(const char* text, float seconds)
{
	message = text;
	messageTimer = seconds;
}

Vec3 Game::SkyColorNow() const
{
	return MakeEnv().fogColor;
}

float Game::SporeDensityNow() const
{
	float a = (timeOfDay - 0.25f) * 2.0f * kPi;
	float day = Saturatef(sinf(a));
	return 0.72f + 0.28f * (1.0f - day);
}

SceneEnv Game::MakeEnv() const
{
	struct Anchor
	{
		Vec3 sun, sky, ground, fog;
		float density, sat;
	};

	// midnight -> dawn (violet) -> noon (pale green) -> dusk (orange) -> midnight
	static const Anchor kAnchors[5] =
	{
		{ Vec3(0.17f, 0.20f, 0.36f), Vec3(0.07f, 0.09f, 0.16f), Vec3(0.02f, 0.03f, 0.05f), Vec3(0.05f, 0.07f, 0.13f), 0.028f, 0.60f },
		{ Vec3(0.64f, 0.49f, 0.70f), Vec3(0.30f, 0.27f, 0.41f), Vec3(0.06f, 0.06f, 0.10f), Vec3(0.50f, 0.45f, 0.58f), 0.032f, 0.70f },
		{ Vec3(0.98f, 1.00f, 0.90f), Vec3(0.40f, 0.46f, 0.42f), Vec3(0.09f, 0.12f, 0.09f), Vec3(0.63f, 0.70f, 0.65f), 0.012f, 0.80f },
		{ Vec3(1.00f, 0.64f, 0.36f), Vec3(0.38f, 0.32f, 0.30f), Vec3(0.07f, 0.06f, 0.06f), Vec3(0.60f, 0.46f, 0.38f), 0.019f, 0.76f },
		{ Vec3(0.17f, 0.20f, 0.36f), Vec3(0.07f, 0.09f, 0.16f), Vec3(0.02f, 0.03f, 0.05f), Vec3(0.05f, 0.07f, 0.13f), 0.028f, 0.60f },
	};

	float t = timeOfDay * 4.0f;
	int i = (int)t;
	if (i < 0) i = 0;
	if (i > 3) i = 3;
	float f = t - (float)i;
	f = f * f * (3.0f - 2.0f * f);

	const Anchor& a = kAnchors[i];
	const Anchor& b = kAnchors[i + 1];

	SceneEnv env;
	env.sunColor = LerpV(a.sun, b.sun, f);
	env.skyColor = LerpV(a.sky, b.sky, f);
	env.groundColor = LerpV(a.ground, b.ground, f);
	env.fogColor = LerpV(a.fog, b.fog, f);
	env.fogDensity = Lerpf(a.density, b.density, f);
	env.saturation = Lerpf(a.sat, b.sat, f);

	// The sun rises at 0.25 and sets at 0.75. A floor keeps night readable.
	float ang = (timeOfDay - 0.25f) * 2.0f * kPi;
	float elev = sinf(ang);
	env.sunDir = Normalize(Vec3(cosf(ang) * 0.75f, Maxf(elev, -0.15f) * 0.9f + 0.18f, 0.42f));

	env.fogOrigin = camTarget;
	env.fogDensity += sporeExposure * 0.010f;

	return env;
}

void Game::Render()
{
	SceneEnv env = MakeEnv();

	renderer->BeginFrame(env.fogColor);
	renderer->SetEnv(env, time);

	Vec3 dir(cosf(camPitch) * sinf(camYaw), sinf(camPitch), cosf(camPitch) * cosf(camYaw));
	Vec3 eye = camTarget + dir * camDistance;

	float aspect = (float)renderer->GetWidth() / (float)Maxf((float)renderer->GetHeight(), 1.0f);
	float hh = orthoHeight * 0.5f;
	float hw = hh * aspect;

	Mat4 view = MatLookAt(eye, camTarget, Vec3(0.0f, 1.0f, 0.0f));
	Mat4 proj = MatOrtho(-hw, hw, -hh, hh, 0.1f, 400.0f);
	float pixelsPerUnit = (float)renderer->GetHeight() / orthoHeight;

	renderer->SetCamera(view, proj, eye, pixelsPerUnit);

	DrawWorld();
	DrawSleepers();
	DrawPlayer();

	// Spores last: additive, and they should sit over everything.
	Vec3 sporeColor(0.55f, 0.95f, 0.80f);
	renderer->DrawSpores(Vec3(camTarget.x, 0.0f, camTarget.z),
						 Vec3(70.0f, 16.0f, 70.0f),
						 sporeColor,
						 SporeDensityNow() * 0.55f,
						 0.13f);

	renderer->BeginUI();

	float haze = 0.08f + sporeExposure * 0.34f;
	float vignette = 0.28f + sporeExposure * 0.42f;
	renderer->DrawAtmosphere(vignette, haze, Vec3(0.42f, 0.70f, 0.62f));

	DrawHUD();
	DrawLetterPanel();
	DrawTitleCards();

	renderer->EndUI();
}

void Game::DrawWorld()
{
	renderer->DrawGround(camTarget, 400.0f);
	renderer->DrawWater(waterCenter, waterSizeX, waterSizeZ);

	for (size_t i = 0; i < props.size(); ++i)
	{
		const Prop& p = props[i];
		DrawParams params;
		params.emissive = p.emissive;
		params.phase = p.pos.x * 0.37f + p.pos.z * 0.21f;
		renderer->DrawModel(p.model, p.pos, p.yaw, p.scale, params);
	}

	if (letterFound && stage == QUEST_READ_LETTER)
	{
		DrawParams params;
		params.emissive = 0.3f;
		renderer->DrawModel(MODEL_LETTER, letterPos, 0.4f, kUnitScale, params);
	}
}

void Game::DrawSleepers()
{
	for (size_t i = 0; i < sleepers.size(); ++i)
	{
		const Sleeper& s = sleepers[i];
		Mat4 root = Mul(MatTranslate(s.pos), MatRotateY(s.yaw));

		DrawParams params;
		params.phase = s.phase;
		renderer->DrawModel(s.isGrandma ? MODEL_SLEEPER_ELDER : MODEL_SLEEPER, root, params);

		if (!s.isGrandma && !s.visited)
		{
			params.emissive = 0.6f;
			renderer->DrawModel(MODEL_DREAM_MOTE, root, params);
		}
	}
}

void Game::DrawPlayer()
{
	float bob = (rollTimer > 0.0f) ? 0.0f : fabsf(sinf(walkPhase)) * 0.05f;

	Mat4 base = Mul(MatTranslate(Vec3(playerPos.x, playerPos.y + bob, playerPos.z)),
					MatRotateY(playerYaw));

	// Roll pivots around the waist so the tumble reads from a quarter view.
	const float pivot = 0.62f;
	Mat4 root = base;
	if (rollTimer > 0.0f)
	{
		Mat4 spin = Mul(Mul(MatTranslate(Vec3(0.0f, pivot, 0.0f)), MatRotateX(rollAngle)),
						MatTranslate(Vec3(0.0f, -pivot, 0.0f)));
		root = Mul(base, spin);
	}

	renderer->DrawShadow(playerPos, 0.45f);
	renderer->DrawModel(MODEL_PLAYER, root, DrawParams());
}

const char* Game::ObjectiveText() const
{
	switch (stage)
	{
	case QUEST_FIND_GRANDMOTHER: return "Find your grandmother.";
	case QUEST_READ_LETTER:      return "Read the letter beside her.";
	case QUEST_LEAVE_VILLAGE:    return "Leave the village. Follow the road south.";
	default:                     return "You left Mulangae Village.";
	}
}

void Game::DrawHUD()
{
	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	const Vec3 ink(0.90f, 0.93f, 0.90f);
	const Vec3 dim(0.62f, 0.68f, 0.65f);
	const Vec3 accent(0.60f, 0.92f, 0.80f);

	renderer->DrawRectPx(18.0f, 18.0f, 430.0f, 62.0f, Vec3(0.03f, 0.05f, 0.05f), 0.38f);
	renderer->DrawTexts(32, 40, "OBJECTIVE", dim, false);
	renderer->DrawTexts(32, 64, ObjectiveText(), ink, false);

	{
		char buf[64];
		sprintf_s(buf, sizeof(buf), "Dream fragments  %d / %d", fragments, fragmentGoal);
		int tw = renderer->TextWidth(buf, false);
		renderer->DrawRectPx((float)(w - tw - 46), 18.0f, (float)(tw + 28), 38.0f,
							 Vec3(0.03f, 0.05f, 0.05f), 0.38f);
		renderer->DrawTexts(w - tw - 32, 42, buf, fragments >= fragmentGoal ? accent : ink, false);
	}

	{
		float bx = 24.0f, by = (float)h - 54.0f, bw = 220.0f, bh = 12.0f;
		renderer->DrawTexts(24, h - 62, "SPORE EXPOSURE", dim, false);
		renderer->DrawRectPx(bx, by, bw, bh, Vec3(0.05f, 0.07f, 0.07f), 0.55f);
		renderer->DrawRectPx(bx, by, bw * sporeExposure, bh,
							 LerpV(Vec3(0.35f, 0.70f, 0.60f), Vec3(0.75f, 0.95f, 0.55f), sporeExposure),
							 0.85f);
	}

	{
		const char* help = "WASD move    SPACE roll    E interact    T time    ESC quit";
		int tw = renderer->TextWidth(help, false);
		renderer->DrawTexts(w - tw - 24, h - 24, help, dim, false);
	}

	if (!prompt.empty())
	{
		int tw = renderer->TextWidth(prompt.c_str(), true);
		renderer->DrawRectPx((float)(w / 2 - tw / 2 - 18), (float)(h - 150), (float)(tw + 36), 40.0f,
							 Vec3(0.03f, 0.05f, 0.05f), 0.45f);
		renderer->DrawTexts(w / 2 - tw / 2, h - 123, prompt.c_str(), accent, true);
	}

	if (messageTimer > 0.0f && !message.empty())
	{
		float alpha = Saturatef(messageTimer);
		int tw = renderer->TextWidth(message.c_str(), false);
		renderer->DrawRectPx((float)(w / 2 - tw / 2 - 20), (float)(h - 100), (float)(tw + 40), 36.0f,
							 Vec3(0.02f, 0.04f, 0.04f), 0.55f * alpha);
		renderer->DrawTexts(w / 2 - tw / 2, h - 77, message.c_str(), ink, false);
	}
}

void Game::DrawLetterPanel()
{
	if (!letterOpen) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	renderer->DrawFade(Vec3(0.02f, 0.03f, 0.04f), 0.55f);

	float pw = 620.0f, ph = 290.0f;
	float px = (float)w * 0.5f - pw * 0.5f;
	float py = (float)h * 0.5f - ph * 0.5f;
	renderer->DrawRectPx(px, py, pw, ph, Vec3(0.10f, 0.11f, 0.10f), 0.94f);
	renderer->DrawRectPx(px, py, pw, 3.0f, Vec3(0.45f, 0.60f, 0.50f), 0.8f);

	const char* lines[] =
	{
		"The spores have reached us, so the village will sleep soon.",
		"I am not telling you to run.",
		"",
		"There is a tree at the heart of the city.",
		"They say the people there do not fall asleep. They wake.",
		"",
		"Go and see it with your own eyes.",
		"Then choose, for yourself, where you will live.",
	};

	int y = (int)py + 52;
	for (int i = 0; i < 8; ++i)
	{
		renderer->DrawTexts((int)px + 40, y, lines[i], Vec3(0.88f, 0.90f, 0.86f), false);
		y += 26;
	}
	renderer->DrawTexts((int)(px + pw) - 130, y + 12, "- Sunim", Vec3(0.70f, 0.76f, 0.72f), false);
	renderer->DrawTexts((int)px + 40, (int)(py + ph) - 18, "[E]  Close", Vec3(0.60f, 0.92f, 0.80f), false);
}

void Game::DrawTitleCards()
{
	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	if (titleTimer < 8.0f)
	{
		float alpha = 1.0f;
		if (titleTimer < 1.0f) alpha = titleTimer;
		else if (titleTimer > 6.0f) alpha = Saturatef((8.0f - titleTimer) * 0.5f);

		const char* t1 = "MULANGAE VILLAGE";
		const char* t2 = "the morning the spores arrived";
		int w1 = renderer->TextWidth(t1, true);
		int w2 = renderer->TextWidth(t2, false);

		renderer->DrawTexts(w / 2 - w1 / 2, h / 2 - 20, t1, Vec3(0.92f, 0.95f, 0.92f) * alpha, true);
		renderer->DrawTexts(w / 2 - w2 / 2, h / 2 + 10, t2, Vec3(0.70f, 0.82f, 0.76f) * alpha, false);
	}

	if (endingTimer >= 0.0f)
	{
		float a = Saturatef(endingTimer * 0.5f);
		renderer->DrawFade(Vec3(0.62f, 0.72f, 0.66f), a * 0.82f);

		if (endingTimer > 1.0f)
		{
			float ta = Saturatef((endingTimer - 1.0f) * 0.8f);
			const char* t1 = "You left Mulangae Village.";
			const char* t2 = "Route 32 lies ahead.";
			const char* t3 = "-- prototype end --   ESC to quit";
			int w1 = renderer->TextWidth(t1, true);
			int w2 = renderer->TextWidth(t2, false);
			int w3 = renderer->TextWidth(t3, false);

			renderer->DrawTexts(w / 2 - w1 / 2, h / 2 - 24, t1, Vec3(0.10f, 0.14f, 0.12f) * ta, true);
			renderer->DrawTexts(w / 2 - w2 / 2, h / 2 + 6, t2, Vec3(0.16f, 0.22f, 0.18f) * ta, false);
			renderer->DrawTexts(w / 2 - w3 / 2, h / 2 + 40, t3, Vec3(0.20f, 0.26f, 0.22f) * ta, false);
		}
	}
}
