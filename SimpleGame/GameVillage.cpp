#include "stdafx.h"
#include "Game.h"

#include <cstdio>

#include "Collision.h"
#include "Models.h"

namespace
{
	const float kExitZ = 26.0f;
	const float kContinueDelay = 2.5f;

	const Vec3 kUnitScale(1.0f, 1.0f, 1.0f);
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

void Game::BuildVillage()
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

void Game::UpdateVillage(float dt, const bool* keys)
{
	if (!letterOpen)
	{
		UpdatePlayer(dt, keys);
		ResolveVillageCollisions();

		// Clamp to the field, but leave the south open for the exit.
		playerPos.x = Clampf(playerPos.x, -30.0f, 30.0f);
		playerPos.z = Clampf(playerPos.z, -28.0f, 32.0f);

		if (stage == QUEST_LEAVE_VILLAGE && playerPos.z > kExitZ)
		{
			stage = QUEST_DONE;
			endingTimer = 0.0f;
			ShowMessage("", 0.0f);
		}
	}

	// Nature Insight slows exposure; the water's edge clears it.
	float insight = Maxf(1.0f - 0.18f * (float)fragments, 0.3f);
	float rate = 0.028f * insight;

	float nearWaterX = Maxf(fabsf(playerPos.x - waterCenter.x) - waterSizeX * 0.5f, 0.0f);
	float nearWaterZ = Maxf(fabsf(playerPos.z - waterCenter.z) - waterSizeZ * 0.5f, 0.0f);
	float waterDist = sqrtf(nearWaterX * nearWaterX + nearWaterZ * nearWaterZ);
	if (waterDist < 4.5f) rate = -0.075f;

	// Rolling holds your breath.
	if (rollTimer > 0.0f) rate = Minf(rate, 0.0f);

	sporeExposure = Saturatef(sporeExposure + rate * dt);

	UpdateInteractionTarget();
	if (endingTimer >= 0.0f) endingTimer += dt;
}

void Game::ResolveVillageCollisions()
{
	// Axis-aligned pushes. Prop yaw is small enough that ignoring it reads fine.
	for (size_t i = 0; i < props.size(); ++i)
	{
		const Prop& p = props[i];
		if (p.halfX > 0.0f && p.halfZ > 0.0f)
			PushOutOfBox(playerPos, kPlayerRadius, p.pos, p.halfX, p.halfZ);
	}

	PushOutOfBox(playerPos, kPlayerRadius, waterCenter, waterSizeX * 0.5f, waterSizeZ * 0.5f);
}

void Game::UpdateInteractionTarget()
{
	targetSleeper = -1;
	targetLetter = false;
	prompt.clear();

	if (stage == QUEST_DONE) return;

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
	if (stage == QUEST_DONE)
	{
		if (endingTimer > kContinueDelay) StartRoute();
		return;
	}

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

void Game::DrawVillage()
{
	GroundParams ground;
	ground.stage = 0.0f;
	ground.dampCenter = waterCenter;
	ground.dampStrength = 1.0f;
	renderer->DrawGround(camTarget, 400.0f, ground);
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

	DrawSleepers();
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

void Game::DrawVillageHud()
{
	const int w = renderer->GetWidth();

	DrawObjective(ObjectiveText());

	char buf[64];
	sprintf_s(buf, sizeof(buf), "Dream fragments  %d / %d", fragments, fragmentGoal);
	int tw = renderer->TextWidth(buf, false);
	renderer->DrawRectPx((float)(w - tw - 46), 18.0f, (float)(tw + 28), 38.0f, kHudPanel, 0.38f);
	renderer->DrawTexts(w - tw - 32, 42, buf, fragments >= fragmentGoal ? kHudAccent : kHudInk, false);

	DrawCommonHud("WASD move    SPACE roll    E interact    T time    F2 skip    ESC quit");
	DrawLetterPanel();
	DrawTitleCard("MULANGAE VILLAGE", "the morning the spores arrived");
	DrawEndingCard();
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
	renderer->DrawTexts((int)px + 40, (int)(py + ph) - 18, "[E]  Close", kHudAccent, false);
}

void Game::DrawEndingCard()
{
	if (endingTimer < 0.0f) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	float a = Saturatef(endingTimer * 0.5f);
	renderer->DrawFade(Vec3(0.62f, 0.72f, 0.66f), a * 0.82f);

	if (endingTimer <= 1.0f) return;

	float ta = Saturatef((endingTimer - 1.0f) * 0.8f);
	const char* t1 = "You left Mulangae Village.";
	const char* t2 = "Route 32 lies ahead.";
	const char* t3 = endingTimer > kContinueDelay ? "[E]  Continue to Route 32      ESC  Quit" : "";
	int w1 = renderer->TextWidth(t1, true);
	int w2 = renderer->TextWidth(t2, false);
	int w3 = renderer->TextWidth(t3, false);

	renderer->DrawTexts(w / 2 - w1 / 2, h / 2 - 24, t1, Vec3(0.10f, 0.14f, 0.12f) * ta, true);
	renderer->DrawTexts(w / 2 - w2 / 2, h / 2 + 6, t2, Vec3(0.16f, 0.22f, 0.18f) * ta, false);
	renderer->DrawTexts(w / 2 - w3 / 2, h / 2 + 40, t3, Vec3(0.20f, 0.26f, 0.22f) * ta, false);
}
