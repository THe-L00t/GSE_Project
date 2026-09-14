#include "stdafx.h"
#include "Game.h"

#include <cstdio>
#include <cstring>

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
}

Game::Game(Renderer* r)
	: renderer(r)
{
	camTarget = playerPos;
	BuildWorld();
}

void Game::AddHouse(const Vec3& pos, float w, float h, float d, float yaw,
					const Vec3& wall, const Vec3& roof)
{
	Prop body;
	body.pos = pos;
	body.size = Vec3(w, h, d);
	body.yaw = yaw;
	body.color = wall;
	body.solid = true;
	props.push_back(body);

	Prop cap;
	cap.pos = Vec3(pos.x, pos.y + h, pos.z);
	cap.size = Vec3(w + 0.7f, 0.45f, d + 0.7f);
	cap.yaw = yaw;
	cap.color = roof;
	cap.solid = false;
	props.push_back(cap);

	Prop ridge;
	ridge.pos = Vec3(pos.x, pos.y + h + 0.45f, pos.z);
	ridge.size = Vec3(w * 0.62f, 0.40f, d * 0.62f);
	ridge.yaw = yaw;
	ridge.color = roof * 0.88f;
	ridge.solid = false;
	props.push_back(ridge);
}

void Game::AddTree(const Vec3& pos, float scale)
{
	Prop trunk;
	trunk.pos = pos;
	trunk.size = Vec3(0.45f * scale, 2.6f * scale, 0.45f * scale);
	trunk.color = Vec3(0.22f, 0.19f, 0.16f);
	trunk.solid = true;
	props.push_back(trunk);

	const float foliage[3][4] =
	{
		{ 2.5f, 1.5f, 2.5f, 0.0f },
		{ 2.0f, 1.3f, 2.0f, 0.7f },
		{ 1.4f, 1.1f, 1.4f, 1.4f },
	};
	float y = pos.y + 2.2f * scale;
	for (int i = 0; i < 3; ++i)
	{
		Prop leaf;
		leaf.pos = Vec3(pos.x, y, pos.z);
		leaf.size = Vec3(foliage[i][0] * scale, foliage[i][1] * scale, foliage[i][2] * scale);
		leaf.yaw = foliage[i][3];
		leaf.color = Vec3(0.17f + 0.03f * i, 0.28f + 0.03f * i, 0.19f);
		leaf.solid = false;
		props.push_back(leaf);
		y += foliage[i][1] * scale * 0.72f;
	}
}

void Game::AddFence(const Vec3& from, const Vec3& to)
{
	Vec3 d = to - from;
	float len = Length(d);
	int posts = (int)(len / 1.5f) + 1;
	for (int i = 0; i <= posts; ++i)
	{
		float t = (float)i / (float)posts;
		Prop p;
		p.pos = LerpV(from, to, t);
		p.size = Vec3(0.14f, 1.0f, 0.14f);
		p.color = Vec3(0.26f, 0.24f, 0.20f);
		p.solid = false;
		props.push_back(p);
	}

	Prop rail;
	rail.pos = LerpV(from, to, 0.5f);
	rail.pos.y = 0.62f;
	rail.yaw = atan2f(d.x, d.z);
	rail.size = Vec3(0.08f, 0.12f, len);
	rail.color = Vec3(0.24f, 0.22f, 0.19f);
	rail.solid = false;
	props.push_back(rail);
}

void Game::BuildWorld()
{
	const Vec3 wallA(0.44f, 0.42f, 0.38f);
	const Vec3 wallB(0.38f, 0.38f, 0.36f);
	const Vec3 roofA(0.21f, 0.21f, 0.20f);
	const Vec3 roofB(0.24f, 0.22f, 0.19f);

	// Grandmother's house
	AddHouse(Vec3(-2.0f, 0.0f, -10.0f), 7.0f, 3.6f, 6.0f, 0.0f, wallA, roofA);
	AddHouse(Vec3(9.5f, 0.0f, -7.0f), 5.5f, 3.2f, 5.0f, 0.12f, wallB, roofB);
	AddHouse(Vec3(-10.5f, 0.0f, 3.0f), 5.0f, 3.0f, 4.6f, -0.15f, wallA, roofB);
	AddHouse(Vec3(9.0f, 0.0f, 8.5f), 5.0f, 3.1f, 5.0f, 0.05f, wallB, roofA);
	AddHouse(Vec3(-7.0f, 0.0f, 16.5f), 4.6f, 2.9f, 4.4f, 0.20f, wallA, roofB);

	// Well
	{
		Prop ring;
		ring.pos = Vec3(2.5f, 0.0f, -2.0f);
		ring.size = Vec3(1.9f, 0.85f, 1.9f);
		ring.color = Vec3(0.33f, 0.33f, 0.31f);
		ring.solid = true;
		props.push_back(ring);

		Prop water;
		water.pos = Vec3(2.5f, 0.85f, -2.0f);
		water.size = Vec3(1.5f, 0.04f, 1.5f);
		water.color = Vec3(0.10f, 0.16f, 0.18f);
		water.solid = false;
		props.push_back(water);

		for (int i = 0; i < 2; ++i)
		{
			Prop post;
			post.pos = Vec3(2.5f + (i == 0 ? -0.85f : 0.85f), 0.0f, -2.0f);
			post.size = Vec3(0.16f, 2.4f, 0.16f);
			post.color = Vec3(0.25f, 0.23f, 0.19f);
			post.solid = false;
			props.push_back(post);
		}

		Prop roof;
		roof.pos = Vec3(2.5f, 2.4f, -2.0f);
		roof.size = Vec3(2.6f, 0.28f, 2.2f);
		roof.color = Vec3(0.22f, 0.21f, 0.18f);
		roof.solid = false;
		props.push_back(roof);
	}

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

	// Abandoned truck
	{
		Prop bed;
		bed.pos = Vec3(11.5f, 0.0f, 15.0f);
		bed.size = Vec3(2.1f, 1.1f, 4.4f);
		bed.yaw = 0.30f;
		bed.color = Vec3(0.30f, 0.27f, 0.23f);
		bed.solid = true;
		props.push_back(bed);

		Prop cab;
		cab.pos = Vec3(11.9f, 1.1f, 13.6f);
		cab.size = Vec3(1.9f, 1.1f, 1.7f);
		cab.yaw = 0.30f;
		cab.color = Vec3(0.26f, 0.25f, 0.22f);
		cab.solid = false;
		props.push_back(cab);

		Prop moss;
		moss.pos = Vec3(11.5f, 1.1f, 15.6f);
		moss.size = Vec3(2.0f, 0.22f, 2.6f);
		moss.yaw = 0.30f;
		moss.color = Vec3(0.20f, 0.33f, 0.20f);
		moss.solid = false;
		props.push_back(moss);
	}

	// Road sign toward Route 32
	{
		const float signZ = 24.5f;
		const float signX = RoadCenter(signZ) + 3.2f;

		Prop post;
		post.pos = Vec3(signX, 0.0f, signZ);
		post.size = Vec3(0.16f, 2.3f, 0.16f);
		post.color = Vec3(0.27f, 0.25f, 0.21f);
		post.solid = false;
		props.push_back(post);

		Prop board;
		board.pos = Vec3(signX, 1.7f, signZ);
		board.size = Vec3(1.9f, 0.75f, 0.10f);
		board.yaw = -0.5f;
		board.color = Vec3(0.42f, 0.44f, 0.40f);
		board.emissive = 0.10f;
		board.solid = false;
		props.push_back(board);
	}

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
		if (!p.solid) continue;

		float hx = p.size.x * 0.5f + kPlayerRadius;
		float hz = p.size.z * 0.5f + kPlayerRadius;
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
		renderer->DrawBox(p.pos, p.size, p.yaw, p.color, p.emissive);
	}

	if (letterFound && stage == QUEST_READ_LETTER)
	{
		float glow = 0.25f + 0.15f * sinf(time * 2.0f);
		renderer->DrawBox(letterPos, Vec3(0.42f, 0.03f, 0.30f), 0.4f,
						  Vec3(0.86f, 0.84f, 0.76f), glow);
	}
}

void Game::DrawSleepers()
{
	for (size_t i = 0; i < sleepers.size(); ++i)
	{
		const Sleeper& s = sleepers[i];

		Mat4 root = Mul(MatTranslate(s.pos), MatRotateY(s.yaw));

		float breath = 1.0f + 0.06f * sinf(time * 0.7f + s.phase);

		Vec3 cloth = s.isGrandma ? Vec3(0.40f, 0.36f, 0.34f) : Vec3(0.33f, 0.33f, 0.32f);
		Mat4 body = Mul(root, MatScale(Vec3(0.58f, 0.38f * breath, 1.75f)));
		renderer->DrawBox(body, cloth, 0.0f);

		Mat4 head = Mul(Mul(root, MatTranslate(Vec3(0.0f, 0.0f, 0.98f))),
						MatScale(Vec3(0.34f, 0.34f, 0.34f)));
		renderer->DrawBox(head, Vec3(0.46f, 0.42f, 0.38f), 0.0f);

		float pulse = 0.10f + 0.08f * sinf(time * 0.9f + s.phase);
		float mossLen = s.isGrandma ? 1.30f : 0.95f;
		Mat4 moss = Mul(Mul(root, MatTranslate(Vec3(0.0f, 0.38f * breath, -0.15f))),
						MatScale(Vec3(0.52f, 0.07f, mossLen)));
		renderer->DrawBox(moss, Vec3(0.26f, 0.46f, 0.30f), pulse);

		if (!s.isGrandma && !s.visited)
		{
			float h = 1.05f + 0.06f * sinf(time * 1.3f + s.phase);
			Mat4 mote = Mul(Mul(root, MatTranslate(Vec3(0.0f, h, 0.2f))),
							MatScale(Vec3(0.10f, 0.10f, 0.10f)));
			renderer->DrawBox(mote, Vec3(0.60f, 0.95f, 0.82f), 0.85f);
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

	Mat4 body = Mul(root, MatScale(Vec3(0.55f, 1.12f, 0.42f)));
	renderer->DrawBox(body, Vec3(0.30f, 0.36f, 0.36f), 0.0f);

	Mat4 head = Mul(Mul(root, MatTranslate(Vec3(0.0f, 1.12f, 0.0f))),
					MatScale(Vec3(0.40f, 0.40f, 0.40f)));
	renderer->DrawBox(head, Vec3(0.52f, 0.46f, 0.41f), 0.0f);

	Mat4 pack = Mul(Mul(root, MatTranslate(Vec3(0.0f, 0.48f, -0.26f))),
					MatScale(Vec3(0.44f, 0.46f, 0.24f)));
	renderer->DrawBox(pack, Vec3(0.34f, 0.30f, 0.24f), 0.0f);
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
