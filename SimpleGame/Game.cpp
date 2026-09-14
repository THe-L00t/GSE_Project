#include "stdafx.h"
#include "Game.h"

#include <cstdio>
#include <cstring>

// Mulangae Village, the morning the spores arrived.
// Prototype scope: movement, roll, quarter view, atmosphere, one small quest.
// All on-screen text is ASCII; the bitmap fonts freeglut ships cannot draw Hangul.

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

Game::Game(Renderer* renderer)
	: m_Renderer(renderer)
	, m_WaterCenter(-15.0f, 0.03f, -14.0f)
	, m_WaterSizeX(22.0f)
	, m_WaterSizeZ(18.0f)
	, m_PlayerPos(2.0f, 0.0f, 1.5f)
	, m_PlayerYaw(kPi)
	, m_WalkPhase(0.0f)
	, m_RollTimer(0.0f)
	, m_RollCooldown(0.0f)
	, m_RollAngle(0.0f)
	, m_CamYaw(DegToRad(45.0f))
	, m_CamPitch(DegToRad(30.0f))
	, m_CamDistance(55.0f)
	, m_OrthoHeight(20.0f)
	, m_Time(0.0f)
	, m_TimeOfDay(0.27f)          // just after dawn
	, m_DayLength(240.0f)
	, m_TimeScale(1.0f)
	, m_SporeExposure(0.15f)
	, m_Stage(QUEST_FIND_GRANDMOTHER)
	, m_Fragments(0)
	, m_FragmentGoal(3)
	, m_LetterOpen(false)
	, m_LetterFound(false)
	, m_MessageTimer(0.0f)
	, m_TitleTimer(0.0f)
	, m_EndingTimer(-1.0f)
	, m_Quit(false)
	, m_TargetSleeper(-1)
	, m_TargetLetter(false)
{
	m_CamTarget = m_PlayerPos;
	BuildWorld();
}

// ------------------------------------------------------------- world ----

void Game::AddHouse(const Vec3& pos, float w, float h, float d, float yaw,
					const Vec3& wall, const Vec3& roof)
{
	Prop body;
	body.pos = pos;
	body.size = Vec3(w, h, d);
	body.yaw = yaw;
	body.color = wall;
	body.solid = true;
	m_Props.push_back(body);

	Prop cap;
	cap.pos = Vec3(pos.x, pos.y + h, pos.z);
	cap.size = Vec3(w + 0.7f, 0.45f, d + 0.7f);
	cap.yaw = yaw;
	cap.color = roof;
	cap.solid = false;
	m_Props.push_back(cap);

	// A second, smaller slab suggests a gable without needing new geometry.
	Prop ridge;
	ridge.pos = Vec3(pos.x, pos.y + h + 0.45f, pos.z);
	ridge.size = Vec3(w * 0.62f, 0.40f, d * 0.62f);
	ridge.yaw = yaw;
	ridge.color = roof * 0.88f;
	ridge.solid = false;
	m_Props.push_back(ridge);
}

void Game::AddTree(const Vec3& pos, float scale)
{
	Prop trunk;
	trunk.pos = pos;
	trunk.size = Vec3(0.45f * scale, 2.6f * scale, 0.45f * scale);
	trunk.color = Vec3(0.22f, 0.19f, 0.16f);
	trunk.solid = true;
	m_Props.push_back(trunk);

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
		m_Props.push_back(leaf);
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
		m_Props.push_back(p);
	}

	Prop rail;
	rail.pos = LerpV(from, to, 0.5f);
	rail.pos.y = 0.62f;
	rail.yaw = atan2f(d.x, d.z);
	rail.size = Vec3(0.08f, 0.12f, len);
	rail.color = Vec3(0.24f, 0.22f, 0.19f);
	rail.solid = false;
	m_Props.push_back(rail);
}

void Game::BuildWorld()
{
	const Vec3 wallA(0.44f, 0.42f, 0.38f);
	const Vec3 wallB(0.38f, 0.38f, 0.36f);
	const Vec3 roofA(0.21f, 0.21f, 0.20f);
	const Vec3 roofB(0.24f, 0.22f, 0.19f);

	// Grandmother's house, at the north edge facing the village.
	AddHouse(Vec3(-2.0f, 0.0f, -10.0f), 7.0f, 3.6f, 6.0f, 0.0f, wallA, roofA);
	AddHouse(Vec3(9.5f, 0.0f, -7.0f), 5.5f, 3.2f, 5.0f, 0.12f, wallB, roofB);
	AddHouse(Vec3(-10.5f, 0.0f, 3.0f), 5.0f, 3.0f, 4.6f, -0.15f, wallA, roofB);
	AddHouse(Vec3(9.0f, 0.0f, 8.5f), 5.0f, 3.1f, 5.0f, 0.05f, wallB, roofA);
	AddHouse(Vec3(-7.0f, 0.0f, 16.5f), 4.6f, 2.9f, 4.4f, 0.20f, wallA, roofB);

	// The well the tutorial would open with, now standing unused.
	{
		Prop ring;
		ring.pos = Vec3(2.5f, 0.0f, -2.0f);
		ring.size = Vec3(1.9f, 0.85f, 1.9f);
		ring.color = Vec3(0.33f, 0.33f, 0.31f);
		ring.solid = true;
		m_Props.push_back(ring);

		Prop water;
		water.pos = Vec3(2.5f, 0.85f, -2.0f);
		water.size = Vec3(1.5f, 0.04f, 1.5f);
		water.color = Vec3(0.10f, 0.16f, 0.18f);
		water.solid = false;
		m_Props.push_back(water);

		for (int i = 0; i < 2; ++i)
		{
			Prop post;
			post.pos = Vec3(2.5f + (i == 0 ? -0.85f : 0.85f), 0.0f, -2.0f);
			post.size = Vec3(0.16f, 2.4f, 0.16f);
			post.color = Vec3(0.25f, 0.23f, 0.19f);
			post.solid = false;
			m_Props.push_back(post);
		}

		Prop roof;
		roof.pos = Vec3(2.5f, 2.4f, -2.0f);
		roof.size = Vec3(2.6f, 0.28f, 2.2f);
		roof.color = Vec3(0.22f, 0.21f, 0.18f);
		roof.solid = false;
		m_Props.push_back(roof);
	}

	// Trees, kept clear of the road painted by the ground shader.
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

	// A truck that stopped where the road gave out, already half bark.
	{
		Prop bed;
		bed.pos = Vec3(11.5f, 0.0f, 15.0f);
		bed.size = Vec3(2.1f, 1.1f, 4.4f);
		bed.yaw = 0.30f;
		bed.color = Vec3(0.30f, 0.27f, 0.23f);
		bed.solid = true;
		m_Props.push_back(bed);

		Prop cab;
		cab.pos = Vec3(11.9f, 1.1f, 13.6f);
		cab.size = Vec3(1.9f, 1.1f, 1.7f);
		cab.yaw = 0.30f;
		cab.color = Vec3(0.26f, 0.25f, 0.22f);
		cab.solid = false;
		m_Props.push_back(cab);

		Prop moss;
		moss.pos = Vec3(11.5f, 1.1f, 15.6f);
		moss.size = Vec3(2.0f, 0.22f, 2.6f);
		moss.yaw = 0.30f;
		moss.color = Vec3(0.20f, 0.33f, 0.20f);
		moss.solid = false;
		m_Props.push_back(moss);
	}

	// Road sign at the southern edge: the way to Route 32.
	{
		const float signZ = 24.5f;
		const float signX = RoadCenter(signZ) + 3.2f;

		Prop post;
		post.pos = Vec3(signX, 0.0f, signZ);
		post.size = Vec3(0.16f, 2.3f, 0.16f);
		post.color = Vec3(0.27f, 0.25f, 0.21f);
		post.solid = false;
		m_Props.push_back(post);

		Prop board;
		board.pos = Vec3(signX, 1.7f, signZ);
		board.size = Vec3(1.9f, 0.75f, 0.10f);
		board.yaw = -0.5f;
		board.color = Vec3(0.42f, 0.44f, 0.40f);
		board.emissive = 0.10f;
		board.solid = false;
		m_Props.push_back(board);
	}

	// The villagers, asleep where the spores caught them.
	Sleeper grandma;
	grandma.pos = Vec3(-2.0f, 0.0f, -6.2f);
	grandma.yaw = 0.35f;
	grandma.isGrandma = true;
	grandma.phase = 0.0f;
	m_Sleepers.push_back(grandma);

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
		m_Sleepers.push_back(s);
	}

	m_LetterPos = Vec3(-0.6f, 0.0f, -6.0f);
}

// ------------------------------------------------------------ update ----

void Game::Update(float dt, const bool* keys)
{
	m_Time += dt;

	if (!m_LetterOpen)
		UpdatePlayer(dt, keys);

	UpdateCamera(dt);
	UpdateWorldState(dt);
	UpdateInteractionTarget();

	if (m_MessageTimer > 0.0f) m_MessageTimer -= dt;
	m_TitleTimer += dt;
	if (m_EndingTimer >= 0.0f) m_EndingTimer += dt;
}

void Game::UpdatePlayer(float dt, const bool* keys)
{
	// Movement is camera relative: W always goes "up" the screen.
	Vec3 forward(-sinf(m_CamYaw), 0.0f, -cosf(m_CamYaw));
	Vec3 right(cosf(m_CamYaw), 0.0f, -sinf(m_CamYaw));

	Vec3 wish;
	if (keys['w']) wish = wish + forward;
	if (keys['s']) wish = wish - forward;
	if (keys['d']) wish = wish + right;
	if (keys['a']) wish = wish - right;

	float wishLen = Length(wish);
	if (wishLen > 0.001f) wish = wish * (1.0f / wishLen);

	if (m_RollCooldown > 0.0f) m_RollCooldown -= dt;

	if (m_RollTimer > 0.0f)
	{
		// The roll commits to its direction; steering during it would rob the weight.
		m_RollTimer -= dt;
		float k = Saturatef(m_RollTimer / kRollTime);
		float speed = kRollSpeed * (0.35f + 0.65f * k);
		m_PlayerPos = m_PlayerPos + m_RollDir * (speed * dt);
		m_RollAngle += dt / kRollTime * 2.0f * kPi;
		if (m_RollTimer <= 0.0f)
		{
			m_RollTimer = 0.0f;
			m_RollAngle = 0.0f;
		}
	}
	else if (wishLen > 0.001f)
	{
		m_PlayerPos = m_PlayerPos + wish * (kWalkSpeed * dt);
		m_PlayerYaw = atan2f(wish.x, wish.z);
		m_WalkPhase += dt * 9.0f;
	}
	else
	{
		m_WalkPhase = Approach(m_WalkPhase, 0.0f, 6.0f, dt);
	}

	ResolveCollisions();

	// Keep the player inside the playable field, but let them walk out south.
	m_PlayerPos.x = Clampf(m_PlayerPos.x, -30.0f, 30.0f);
	m_PlayerPos.z = Clampf(m_PlayerPos.z, -28.0f, 32.0f);
	m_PlayerPos.y = 0.0f;

	if (m_Stage == QUEST_LEAVE_VILLAGE && m_PlayerPos.z > kExitZ)
	{
		m_Stage = QUEST_DONE;
		m_EndingTimer = 0.0f;
		ShowMessage("", 0.0f);
	}
}

void Game::ResolveCollisions()
{
	// Axis-aligned pushes. Prop yaw is small enough that ignoring it reads fine.
	for (size_t i = 0; i < m_Props.size(); ++i)
	{
		const Prop& p = m_Props[i];
		if (!p.solid) continue;

		float hx = p.size.x * 0.5f + kPlayerRadius;
		float hz = p.size.z * 0.5f + kPlayerRadius;
		float dx = m_PlayerPos.x - p.pos.x;
		float dz = m_PlayerPos.z - p.pos.z;

		if (fabsf(dx) < hx && fabsf(dz) < hz)
		{
			float penX = hx - fabsf(dx);
			float penZ = hz - fabsf(dz);
			if (penX < penZ)
				m_PlayerPos.x += (dx < 0.0f ? -penX : penX);
			else
				m_PlayerPos.z += (dz < 0.0f ? -penZ : penZ);
		}
	}

	// The reservoir edge stops you too.
	float hx = m_WaterSizeX * 0.5f + kPlayerRadius;
	float hz = m_WaterSizeZ * 0.5f + kPlayerRadius;
	float dx = m_PlayerPos.x - m_WaterCenter.x;
	float dz = m_PlayerPos.z - m_WaterCenter.z;
	if (fabsf(dx) < hx && fabsf(dz) < hz)
	{
		float penX = hx - fabsf(dx);
		float penZ = hz - fabsf(dz);
		if (penX < penZ)
			m_PlayerPos.x += (dx < 0.0f ? -penX : penX);
		else
			m_PlayerPos.z += (dz < 0.0f ? -penZ : penZ);
	}
}

void Game::UpdateCamera(float dt)
{
	m_CamTarget = LerpV(m_CamTarget, m_PlayerPos, 1.0f - expf(-6.0f * dt));
}

void Game::UpdateWorldState(float dt)
{
	m_TimeOfDay += (dt * m_TimeScale) / m_DayLength;
	while (m_TimeOfDay >= 1.0f) m_TimeOfDay -= 1.0f;

	// Spore exposure. Nature Insight slows it; the water's edge clears your head.
	float insight = 1.0f - 0.18f * (float)m_Fragments;
	if (insight < 0.3f) insight = 0.3f;

	float rate = 0.028f * insight;

	float nearWaterX = Maxf(fabsf(m_PlayerPos.x - m_WaterCenter.x) - m_WaterSizeX * 0.5f, 0.0f);
	float nearWaterZ = Maxf(fabsf(m_PlayerPos.z - m_WaterCenter.z) - m_WaterSizeZ * 0.5f, 0.0f);
	float waterDist = sqrtf(nearWaterX * nearWaterX + nearWaterZ * nearWaterZ);
	if (waterDist < 4.5f) rate = -0.075f;

	// Rolling holds your breath.
	if (m_RollTimer > 0.0f) rate = Minf(rate, 0.0f);

	m_SporeExposure = Saturatef(m_SporeExposure + rate * dt);
}

void Game::UpdateInteractionTarget()
{
	m_TargetSleeper = -1;
	m_TargetLetter = false;
	m_Prompt.clear();

	if (m_LetterOpen)
	{
		m_Prompt = "[E]  Close";
		return;
	}

	// The letter wins over the sleeper it lies beside.
	if (m_LetterFound && m_Stage == QUEST_READ_LETTER &&
		DistXZ(m_PlayerPos, m_LetterPos) < kInteractRange)
	{
		m_TargetLetter = true;
		m_Prompt = "[E]  Read the letter";
		return;
	}

	float best = kInteractRange;
	for (size_t i = 0; i < m_Sleepers.size(); ++i)
	{
		float d = DistXZ(m_PlayerPos, m_Sleepers[i].pos);
		if (d < best)
		{
			best = d;
			m_TargetSleeper = (int)i;
		}
	}

	if (m_TargetSleeper >= 0)
	{
		const Sleeper& s = m_Sleepers[m_TargetSleeper];
		if (s.isGrandma)
			m_Prompt = (m_Stage == QUEST_FIND_GRANDMOTHER) ? "[E]  Look at grandmother" : "[E]  Sit with her";
		else
			m_Prompt = s.visited ? "" : "[E]  Rest beside them";
	}
}

void Game::TryInteract()
{
	if (m_LetterOpen)
	{
		m_LetterOpen = false;
		if (m_Stage == QUEST_READ_LETTER)
		{
			m_Stage = QUEST_LEAVE_VILLAGE;
			ShowMessage("Follow the road south, out of the village.", 5.0f);
		}
		return;
	}

	if (m_TargetLetter)
	{
		m_LetterOpen = true;
		return;
	}

	if (m_TargetSleeper < 0) return;

	Sleeper& s = m_Sleepers[m_TargetSleeper];

	if (s.isGrandma)
	{
		if (m_Stage == QUEST_FIND_GRANDMOTHER)
		{
			m_Stage = QUEST_READ_LETTER;
			m_LetterFound = true;
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
		++m_Fragments;
		char buf[128];
		sprintf_s(buf, sizeof(buf), "A dream fragment: the first spring, seen from someone else's eyes.  Nature Insight %d", m_Fragments);
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
		m_Quit = true;
		return;
	}

	if (key == 'e')
	{
		TryInteract();
		return;
	}

	if (key == ' ')
	{
		if (m_LetterOpen) return;
		if (m_RollTimer > 0.0f || m_RollCooldown > 0.0f) return;

		m_RollTimer = kRollTime;
		m_RollCooldown = kRollCooldown;
		m_RollAngle = 0.0f;
		m_RollDir = Vec3(sinf(m_PlayerYaw), 0.0f, cosf(m_PlayerYaw));
		return;
	}

	if (key == 't')
	{
		// Demo aid: run the day cycle fast enough to see dawn, noon, dusk, night.
		m_TimeScale = (m_TimeScale > 1.5f) ? 1.0f : 12.0f;
		ShowMessage(m_TimeScale > 1.5f ? "Time: fast" : "Time: normal", 2.0f);
		return;
	}
}

void Game::ShowMessage(const char* text, float seconds)
{
	m_Message = text;
	m_MessageTimer = seconds;
}

// ------------------------------------------------------- environment ----

Vec3 Game::SkyColorNow() const
{
	return MakeEnv().fogColor;
}

float Game::SporeDensityNow() const
{
	// The village was engulfed this morning, so the field is thick throughout.
	float a = (m_TimeOfDay - 0.25f) * 2.0f * kPi;
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
		{ Vec3(0.17f, 0.20f, 0.36f), Vec3(0.07f, 0.09f, 0.16f), Vec3(0.02f, 0.03f, 0.05f), Vec3(0.05f, 0.07f, 0.13f), 0.055f, 0.60f },
		{ Vec3(0.64f, 0.49f, 0.70f), Vec3(0.30f, 0.27f, 0.41f), Vec3(0.06f, 0.06f, 0.10f), Vec3(0.50f, 0.45f, 0.58f), 0.062f, 0.70f },
		{ Vec3(0.98f, 1.00f, 0.90f), Vec3(0.40f, 0.46f, 0.42f), Vec3(0.09f, 0.12f, 0.09f), Vec3(0.63f, 0.70f, 0.65f), 0.024f, 0.80f },
		{ Vec3(1.00f, 0.64f, 0.36f), Vec3(0.38f, 0.32f, 0.30f), Vec3(0.07f, 0.06f, 0.06f), Vec3(0.60f, 0.46f, 0.38f), 0.038f, 0.76f },
		{ Vec3(0.17f, 0.20f, 0.36f), Vec3(0.07f, 0.09f, 0.16f), Vec3(0.02f, 0.03f, 0.05f), Vec3(0.05f, 0.07f, 0.13f), 0.055f, 0.60f },
	};

	float t = m_TimeOfDay * 4.0f;
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
	float ang = (m_TimeOfDay - 0.25f) * 2.0f * kPi;
	float elev = sinf(ang);
	env.sunDir = Normalize(Vec3(cosf(ang) * 0.75f, Maxf(elev, -0.15f) * 0.9f + 0.18f, 0.42f));

	// Exposure thickens the air around the player.
	env.fogDensity += m_SporeExposure * 0.020f;

	return env;
}

// ------------------------------------------------------------ render ----

void Game::Render()
{
	SceneEnv env = MakeEnv();

	m_Renderer->BeginFrame(env.fogColor);
	m_Renderer->SetEnv(env, m_Time);

	// Fixed quarter view: 45 degrees of yaw, 30 of pitch, orthographic.
	Vec3 dir(cosf(m_CamPitch) * sinf(m_CamYaw), sinf(m_CamPitch), cosf(m_CamPitch) * cosf(m_CamYaw));
	Vec3 eye = m_CamTarget + dir * m_CamDistance;

	float aspect = (float)m_Renderer->GetWidth() / (float)Maxf((float)m_Renderer->GetHeight(), 1.0f);
	float hh = m_OrthoHeight * 0.5f;
	float hw = hh * aspect;

	Mat4 view = MatLookAt(eye, m_CamTarget, Vec3(0.0f, 1.0f, 0.0f));
	Mat4 proj = MatOrtho(-hw, hw, -hh, hh, 0.1f, 400.0f);
	float pixelsPerUnit = (float)m_Renderer->GetHeight() / m_OrthoHeight;

	m_Renderer->SetCamera(view, proj, eye, pixelsPerUnit);

	DrawWorld();
	DrawSleepers();
	DrawPlayer();

	// Spores last: additive, and they should sit over everything.
	Vec3 sporeColor(0.55f, 0.95f, 0.80f);
	m_Renderer->DrawSpores(Vec3(m_CamTarget.x, 0.0f, m_CamTarget.z),
						   Vec3(70.0f, 16.0f, 70.0f),
						   sporeColor,
						   SporeDensityNow() * 0.55f,
						   0.13f);

	m_Renderer->BeginUI();

	float haze = 0.08f + m_SporeExposure * 0.34f;
	float vignette = 0.28f + m_SporeExposure * 0.42f;
	m_Renderer->DrawAtmosphere(vignette, haze, Vec3(0.42f, 0.70f, 0.62f));

	DrawHUD();
	DrawLetterPanel();
	DrawTitleCards();

	m_Renderer->EndUI();
}

void Game::DrawWorld()
{
	m_Renderer->DrawGround(m_CamTarget, 400.0f);
	m_Renderer->DrawWater(m_WaterCenter, m_WaterSizeX, m_WaterSizeZ);

	for (size_t i = 0; i < m_Props.size(); ++i)
	{
		const Prop& p = m_Props[i];
		m_Renderer->DrawBox(p.pos, p.size, p.yaw, p.color, p.emissive);
	}

	// The letter, once you know it is there.
	if (m_LetterFound && m_Stage == QUEST_READ_LETTER)
	{
		float glow = 0.25f + 0.15f * sinf(m_Time * 2.0f);
		m_Renderer->DrawBox(m_LetterPos, Vec3(0.42f, 0.03f, 0.30f), 0.4f,
							Vec3(0.86f, 0.84f, 0.76f), glow);
	}
}

void Game::DrawSleepers()
{
	for (size_t i = 0; i < m_Sleepers.size(); ++i)
	{
		const Sleeper& s = m_Sleepers[i];

		Mat4 root = Mul(MatTranslate(s.pos), MatRotateY(s.yaw));

		// Slow breathing: they are asleep, not dead.
		float breath = 1.0f + 0.06f * sinf(m_Time * 0.7f + s.phase);

		Vec3 cloth = s.isGrandma ? Vec3(0.40f, 0.36f, 0.34f) : Vec3(0.33f, 0.33f, 0.32f);
		Mat4 body = Mul(root, MatScale(Vec3(0.58f, 0.38f * breath, 1.75f)));
		m_Renderer->DrawBox(body, cloth, 0.0f);

		Mat4 head = Mul(Mul(root, MatTranslate(Vec3(0.0f, 0.0f, 0.98f))),
						MatScale(Vec3(0.34f, 0.34f, 0.34f)));
		m_Renderer->DrawBox(head, Vec3(0.46f, 0.42f, 0.38f), 0.0f);

		// Moss already taking hold, breathing with a faint light.
		float pulse = 0.10f + 0.08f * sinf(m_Time * 0.9f + s.phase);
		float mossLen = s.isGrandma ? 1.30f : 0.95f;
		Mat4 moss = Mul(Mul(root, MatTranslate(Vec3(0.0f, 0.38f * breath, -0.15f))),
						MatScale(Vec3(0.52f, 0.07f, mossLen)));
		m_Renderer->DrawBox(moss, Vec3(0.26f, 0.46f, 0.30f), pulse);

		if (!s.isGrandma && !s.visited)
		{
			// A dream still unread hangs just above them.
			float h = 1.05f + 0.06f * sinf(m_Time * 1.3f + s.phase);
			Mat4 mote = Mul(Mul(root, MatTranslate(Vec3(0.0f, h, 0.2f))),
							MatScale(Vec3(0.10f, 0.10f, 0.10f)));
			m_Renderer->DrawBox(mote, Vec3(0.60f, 0.95f, 0.82f), 0.85f);
		}
	}
}

void Game::DrawPlayer()
{
	float bob = (m_RollTimer > 0.0f) ? 0.0f : fabsf(sinf(m_WalkPhase)) * 0.05f;

	Mat4 base = Mul(MatTranslate(Vec3(m_PlayerPos.x, m_PlayerPos.y + bob, m_PlayerPos.z)),
					MatRotateY(m_PlayerYaw));

	// Roll pivots around the waist so the tumble reads from a quarter view.
	const float pivot = 0.62f;
	Mat4 root = base;
	if (m_RollTimer > 0.0f)
	{
		Mat4 spin = Mul(Mul(MatTranslate(Vec3(0.0f, pivot, 0.0f)), MatRotateX(m_RollAngle)),
						MatTranslate(Vec3(0.0f, -pivot, 0.0f)));
		root = Mul(base, spin);
	}

	Mat4 body = Mul(root, MatScale(Vec3(0.55f, 1.12f, 0.42f)));
	m_Renderer->DrawBox(body, Vec3(0.30f, 0.36f, 0.36f), 0.0f);

	Mat4 head = Mul(Mul(root, MatTranslate(Vec3(0.0f, 1.12f, 0.0f))),
					MatScale(Vec3(0.40f, 0.40f, 0.40f)));
	m_Renderer->DrawBox(head, Vec3(0.52f, 0.46f, 0.41f), 0.0f);

	Mat4 pack = Mul(Mul(root, MatTranslate(Vec3(0.0f, 0.48f, -0.26f))),
					MatScale(Vec3(0.44f, 0.46f, 0.24f)));
	m_Renderer->DrawBox(pack, Vec3(0.34f, 0.30f, 0.24f), 0.0f);
}

// --------------------------------------------------------------- HUD ----

const char* Game::ObjectiveText() const
{
	switch (m_Stage)
	{
	case QUEST_FIND_GRANDMOTHER: return "Find your grandmother.";
	case QUEST_READ_LETTER:      return "Read the letter beside her.";
	case QUEST_LEAVE_VILLAGE:    return "Leave the village. Follow the road south.";
	default:                     return "You left Mulangae Village.";
	}
}

void Game::DrawHUD()
{
	const int w = m_Renderer->GetWidth();
	const int h = m_Renderer->GetHeight();

	const Vec3 ink(0.90f, 0.93f, 0.90f);
	const Vec3 dim(0.62f, 0.68f, 0.65f);
	const Vec3 accent(0.60f, 0.92f, 0.80f);

	// Objective, top left.
	m_Renderer->DrawRectPx(18.0f, 18.0f, 430.0f, 62.0f, Vec3(0.03f, 0.05f, 0.05f), 0.38f);
	m_Renderer->DrawTexts(32, 40, "OBJECTIVE", dim, false);
	m_Renderer->DrawTexts(32, 64, ObjectiveText(), ink, false);

	// Dream fragments, top right.
	{
		char buf[64];
		sprintf_s(buf, sizeof(buf), "Dream fragments  %d / %d", m_Fragments, m_FragmentGoal);
		int tw = m_Renderer->TextWidth(buf, false);
		m_Renderer->DrawRectPx((float)(w - tw - 46), 18.0f, (float)(tw + 28), 38.0f,
							   Vec3(0.03f, 0.05f, 0.05f), 0.38f);
		m_Renderer->DrawTexts(w - tw - 32, 42, buf, m_Fragments >= m_FragmentGoal ? accent : ink, false);
	}

	// Spore exposure, bottom left. It never kills you here; it only closes in.
	{
		float bx = 24.0f, by = (float)h - 54.0f, bw = 220.0f, bh = 12.0f;
		m_Renderer->DrawTexts(24, h - 62, "SPORE EXPOSURE", dim, false);
		m_Renderer->DrawRectPx(bx, by, bw, bh, Vec3(0.05f, 0.07f, 0.07f), 0.55f);
		m_Renderer->DrawRectPx(bx, by, bw * m_SporeExposure, bh,
							   LerpV(Vec3(0.35f, 0.70f, 0.60f), Vec3(0.75f, 0.95f, 0.55f), m_SporeExposure),
							   0.85f);
	}

	// Controls, bottom right.
	{
		const char* help = "WASD move    SPACE roll    E interact    T time    ESC quit";
		int tw = m_Renderer->TextWidth(help, false);
		m_Renderer->DrawTexts(w - tw - 24, h - 24, help, dim, false);
	}

	// Interaction prompt, just under the middle of the screen.
	if (!m_Prompt.empty())
	{
		int tw = m_Renderer->TextWidth(m_Prompt.c_str(), true);
		m_Renderer->DrawRectPx((float)(w / 2 - tw / 2 - 18), (float)(h - 150), (float)(tw + 36), 40.0f,
							   Vec3(0.03f, 0.05f, 0.05f), 0.45f);
		m_Renderer->DrawTexts(w / 2 - tw / 2, h - 123, m_Prompt.c_str(), accent, true);
	}

	// Transient message.
	if (m_MessageTimer > 0.0f && !m_Message.empty())
	{
		float alpha = Saturatef(m_MessageTimer);
		int tw = m_Renderer->TextWidth(m_Message.c_str(), false);
		m_Renderer->DrawRectPx((float)(w / 2 - tw / 2 - 20), (float)(h - 100), (float)(tw + 40), 36.0f,
							   Vec3(0.02f, 0.04f, 0.04f), 0.55f * alpha);
		m_Renderer->DrawTexts(w / 2 - tw / 2, h - 77, m_Message.c_str(), ink, false);
	}
}

void Game::DrawLetterPanel()
{
	if (!m_LetterOpen) return;

	const int w = m_Renderer->GetWidth();
	const int h = m_Renderer->GetHeight();

	m_Renderer->DrawFade(Vec3(0.02f, 0.03f, 0.04f), 0.55f);

	float pw = 620.0f, ph = 290.0f;
	float px = (float)w * 0.5f - pw * 0.5f;
	float py = (float)h * 0.5f - ph * 0.5f;
	m_Renderer->DrawRectPx(px, py, pw, ph, Vec3(0.10f, 0.11f, 0.10f), 0.94f);
	m_Renderer->DrawRectPx(px, py, pw, 3.0f, Vec3(0.45f, 0.60f, 0.50f), 0.8f);

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
		m_Renderer->DrawTexts((int)px + 40, y, lines[i], Vec3(0.88f, 0.90f, 0.86f), false);
		y += 26;
	}
	m_Renderer->DrawTexts((int)(px + pw) - 130, y + 12, "- Sunim", Vec3(0.70f, 0.76f, 0.72f), false);
	m_Renderer->DrawTexts((int)px + 40, (int)(py + ph) - 18, "[E]  Close", Vec3(0.60f, 0.92f, 0.80f), false);
}

void Game::DrawTitleCards()
{
	const int w = m_Renderer->GetWidth();
	const int h = m_Renderer->GetHeight();

	// Opening card.
	if (m_TitleTimer < 8.0f)
	{
		float alpha = 1.0f;
		if (m_TitleTimer < 1.0f) alpha = m_TitleTimer;
		else if (m_TitleTimer > 6.0f) alpha = Saturatef((8.0f - m_TitleTimer) * 0.5f);

		const char* t1 = "MULANGAE VILLAGE";
		const char* t2 = "the morning the spores arrived";
		int w1 = m_Renderer->TextWidth(t1, true);
		int w2 = m_Renderer->TextWidth(t2, false);

		m_Renderer->DrawTexts(w / 2 - w1 / 2, h / 2 - 20, t1, Vec3(0.92f, 0.95f, 0.92f) * alpha, true);
		m_Renderer->DrawTexts(w / 2 - w2 / 2, h / 2 + 10, t2, Vec3(0.70f, 0.82f, 0.76f) * alpha, false);
	}

	// Ending card.
	if (m_EndingTimer >= 0.0f)
	{
		float a = Saturatef(m_EndingTimer * 0.5f);
		m_Renderer->DrawFade(Vec3(0.62f, 0.72f, 0.66f), a * 0.82f);

		if (m_EndingTimer > 1.0f)
		{
			float ta = Saturatef((m_EndingTimer - 1.0f) * 0.8f);
			const char* t1 = "You left Mulangae Village.";
			const char* t2 = "Route 32 lies ahead.";
			const char* t3 = "-- prototype end --   ESC to quit";
			int w1 = m_Renderer->TextWidth(t1, true);
			int w2 = m_Renderer->TextWidth(t2, false);
			int w3 = m_Renderer->TextWidth(t3, false);

			m_Renderer->DrawTexts(w / 2 - w1 / 2, h / 2 - 24, t1, Vec3(0.10f, 0.14f, 0.12f) * ta, true);
			m_Renderer->DrawTexts(w / 2 - w2 / 2, h / 2 + 6, t2, Vec3(0.16f, 0.22f, 0.18f) * ta, false);
			m_Renderer->DrawTexts(w / 2 - w3 / 2, h / 2 + 40, t3, Vec3(0.20f, 0.26f, 0.22f) * ta, false);
		}
	}
}
