#include "stdafx.h"
#include "Models.h"

#include <cstdio>
#include <direct.h>

namespace
{
	const char* kModelNames[] =
	{
		"player", "pipe", "pipe_pickup", "sleeper", "sleeper_elder", "dream_mote", "letter",
		"house_a", "house_b", "tree", "pine", "well", "truck", "fence_post", "fence_rail", "sign",
		"rock", "bush", "ruin", "car", "lantern",
		"mite", "husk", "boar",
		"herb", "water_flask", "relic",
		"shadow", "ground",
		"grandma", "shed", "garden_bed", "vegetable", "berry_red", "berry_pale", "deer",
		"canned_food", "bus_stop", "ginkgo", "pole",
	};

	const Vec3 kMoss(0.24f, 0.38f, 0.24f);
	const Vec3 kDarkMoss(0.17f, 0.29f, 0.19f);
	const Vec3 kSporeGlow(0.55f, 0.95f, 0.80f);
	const Vec3 kGlass(0.12f, 0.16f, 0.18f);
	const Vec3 kMetal(0.18f, 0.18f, 0.17f);

	void AddWheel(ModelRecipe& r, float x, float z, float diameter, float width)
	{
		r.Add(SHAPE_CYLINDER, Vec3(x, diameter * 0.5f, z), Vec3(diameter, width, diameter), Vec3(0.11f, 0.11f, 0.10f))
			.Rotated(Vec3(0.0f, 0.0f, kPi * 0.5f));
		float side = x > 0.0f ? 1.0f : -1.0f;
		r.Add(SHAPE_CYLINDER, Vec3(x + side * width * 0.5f, diameter * 0.5f, z), Vec3(diameter * 0.5f, 0.02f, diameter * 0.5f), Vec3(0.30f, 0.24f, 0.18f))
			.Rotated(Vec3(0.0f, 0.0f, kPi * 0.5f));
	}

	void AddSleeper(ModelRecipe& r, bool elder)
	{
		const Vec3 cloth = elder ? Vec3(0.40f, 0.36f, 0.34f) : Vec3(0.33f, 0.33f, 0.32f);
		const Vec3 mat(0.28f, 0.25f, 0.21f);
		const Vec3 hair = elder ? Vec3(0.62f, 0.60f, 0.58f) : Vec3(0.16f, 0.14f, 0.12f);
		const float mossLength = elder ? 1.30f : 0.95f;

		r.Add(SHAPE_BOX, Vec3(0.0f, 0.03f, 0.05f), Vec3(0.86f, 0.06f, 2.10f), mat);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.10f, 0.98f), Vec3(0.52f, 0.10f, 0.32f), Vec3(0.52f, 0.50f, 0.46f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.19f, 0.0f), Vec3(0.58f, 0.38f, 1.75f), cloth, ANIM_BREATHE);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.39f, 0.62f), Vec3(0.62f, 0.05f, 0.18f), cloth * 1.15f, ANIM_BREATHE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.24f, 0.98f), Vec3(0.34f, 0.32f, 0.38f), Vec3(0.46f, 0.42f, 0.38f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.30f, 1.06f), Vec3(0.36f, 0.20f, 0.30f), hair);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.415f, -0.15f), Vec3(0.52f, 0.07f, mossLength), Vec3(0.26f, 0.46f, 0.30f), ANIM_BREATHE | ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.22f, 0.30f, -0.40f), Vec3(0.20f, 0.30f, 0.60f), kDarkMoss, ANIM_BREATHE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.12f, 0.44f, 0.40f), Vec3(0.18f, 0.10f, 0.18f), kSporeGlow, ANIM_BREATHE | ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.10f, 0.44f, -0.30f), Vec3(0.10f, 0.07f, 0.10f), kSporeGlow, ANIM_BREATHE | ANIM_PULSE);
	}

	void AddHouse(ModelRecipe& r, const Vec3& wall, const Vec3& roof)
	{
		const Vec3 stone(0.30f, 0.30f, 0.29f);
		const Vec3 frame(0.24f, 0.22f, 0.19f);
		const Vec3 door(0.20f, 0.16f, 0.13f);
		const float slope = 0.583f;   // atan(0.33 / 0.5), the pitch of the gable

		r.Add(SHAPE_BOX, Vec3(0.0f, 0.04f, 0.0f), Vec3(1.04f, 0.08f, 1.04f), stone);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.5f, 0.0f), Vec3(1.0f, 1.0f, 1.0f), wall);
		r.Add(SHAPE_PRISM, Vec3(0.0f, 1.165f, 0.0f), Vec3(1.0f, 0.33f, 1.0f), wall * 0.92f);

		r.Add(SHAPE_BOX, Vec3(0.0f, 1.19f, 0.267f), Vec3(1.14f, 0.05f, 0.70f), roof).Rotated(Vec3(slope, 0.0f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.19f, -0.267f), Vec3(1.14f, 0.05f, 0.70f), roof * 0.90f).Rotated(Vec3(-slope, 0.0f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.345f, 0.0f), Vec3(1.16f, 0.05f, 0.08f), roof * 0.75f);

		r.Add(SHAPE_BOX, Vec3(0.28f, 1.32f, -0.18f), Vec3(0.13f, 0.40f, 0.13f), stone);
		r.Add(SHAPE_BOX, Vec3(0.28f, 1.53f, -0.18f), Vec3(0.17f, 0.04f, 0.17f), kMetal);

		r.Add(SHAPE_BOX, Vec3(0.0f, 0.27f, 0.503f), Vec3(0.26f, 0.54f, 0.015f), frame);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.25f, 0.508f), Vec3(0.20f, 0.50f, 0.02f), door);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.06f, 0.25f, 0.52f), Vec3(0.03f, 0.03f, 0.02f), Vec3(0.55f, 0.48f, 0.32f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.03f, 0.58f), Vec3(0.34f, 0.06f, 0.16f), stone);

		for (int i = 0; i < 2; ++i)
		{
			float x = (i == 0) ? -0.30f : 0.30f;
			r.Add(SHAPE_BOX, Vec3(x, 0.60f, 0.503f), Vec3(0.24f, 0.24f, 0.015f), frame);
			r.Add(SHAPE_BOX, Vec3(x, 0.60f, 0.508f), Vec3(0.18f, 0.18f, 0.02f), kGlass);
			r.Add(SHAPE_BOX, Vec3(x, 0.60f, 0.512f), Vec3(0.18f, 0.015f, 0.02f), frame);
			r.Add(SHAPE_BOX, Vec3(x, 0.47f, 0.52f), Vec3(0.27f, 0.03f, 0.05f), frame);
		}
		r.Add(SHAPE_BOX, Vec3(0.503f, 0.60f, 0.0f), Vec3(0.015f, 0.24f, 0.24f), frame);
		r.Add(SHAPE_BOX, Vec3(0.508f, 0.60f, 0.0f), Vec3(0.02f, 0.18f, 0.18f), kGlass);

		r.Add(SHAPE_ELLIPSOID, Vec3(0.25f, 1.22f, 0.28f), Vec3(0.40f, 0.08f, 0.34f), kMoss, ANIM_PULSE).Rotated(Vec3(slope, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.30f, 1.12f, -0.36f), Vec3(0.30f, 0.07f, 0.22f), kDarkMoss).Rotated(Vec3(-slope, 0.0f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(-0.506f, 0.42f, 0.18f), Vec3(0.02f, 0.72f, 0.20f), kDarkMoss);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.45f, 0.05f, 0.30f), Vec3(0.26f, 0.14f, 0.44f), kMoss);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.46f, 0.04f, -0.40f), Vec3(0.22f, 0.10f, 0.30f), kDarkMoss);
	}

	void AddBerryBush(ModelRecipe& r, const Vec3& berry)
	{
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.10f, 0.0f), Vec3(1.0f, 0.25f, 0.9f), Vec3(0.13f, 0.22f, 0.15f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.36f, 0.0f), Vec3(0.95f, 0.70f, 0.85f), Vec3(0.18f, 0.29f, 0.19f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.30f, 0.30f, 0.20f), Vec3(0.55f, 0.50f, 0.55f), Vec3(0.21f, 0.32f, 0.20f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.28f, 0.28f, -0.15f), Vec3(0.50f, 0.45f, 0.50f), Vec3(0.16f, 0.27f, 0.18f), ANIM_SWAY);

		const float spots[7][3] =
		{
			{ 0.32f, 0.52f, 0.30f }, { -0.30f, 0.46f, 0.22f }, { 0.05f, 0.66f, 0.18f }, { 0.40f, 0.36f, -0.10f },
			{ -0.38f, 0.34f, -0.25f }, { 0.12f, 0.58f, -0.32f }, { -0.10f, 0.40f, 0.42f },
		};
		for (int i = 0; i < 7; ++i)
			r.Add(SHAPE_ELLIPSOID, Vec3(spots[i][0], spots[i][1], spots[i][2]), Vec3(0.09f, 0.09f, 0.09f), berry, ANIM_SWAY);
	}
}

const char* ModelName(int id)
{
	static_assert(sizeof(kModelNames) / sizeof(kModelNames[0]) == MODEL_COUNT, "model name table out of sync with ModelId");
	if (id < 0 || id >= MODEL_COUNT) return "unknown";
	return kModelNames[id];
}

ModelRecipe BuildRecipe(int id)
{
	const Vec3 skin(0.52f, 0.46f, 0.41f);
	const Vec3 stone(0.40f, 0.40f, 0.38f);
	const Vec3 rust(0.45f, 0.30f, 0.20f);
	const Vec3 dark(0.20f, 0.20f, 0.18f);
	const Vec3 bark(0.22f, 0.19f, 0.16f);
	const Vec3 moss = kMoss;
	const Vec3 sporeGlow = kSporeGlow;
	const float halfPi = kPi * 0.5f;
	const int pickup = ANIM_SPIN | ANIM_HOVER;

	ModelRecipe r;
	switch (id)
	{
	case MODEL_PLAYER:
	{
		const Vec3 coat(0.30f, 0.36f, 0.36f);
		const Vec3 trousers(0.22f, 0.23f, 0.24f);
		const Vec3 boots(0.16f, 0.14f, 0.12f);
		const Vec3 hair(0.14f, 0.12f, 0.11f);
		const Vec3 pack(0.34f, 0.30f, 0.24f);
		const Vec3 scarf(0.42f, 0.52f, 0.46f);

		for (int i = 0; i < 2; ++i)
		{
			float side = (i == 0) ? -1.0f : 1.0f;
			r.Add(SHAPE_CYLINDER, Vec3(side * 0.13f, 0.28f, 0.0f), Vec3(0.17f, 0.50f, 0.17f), trousers, ANIM_BOB);
			r.Add(SHAPE_BOX, Vec3(side * 0.13f, 0.05f, 0.04f), Vec3(0.19f, 0.10f, 0.28f), boots, ANIM_BOB);
			r.Add(SHAPE_CYLINDER, Vec3(side * 0.33f, 0.82f, 0.0f), Vec3(0.14f, 0.50f, 0.14f), coat * 0.92f, ANIM_BOB)
				.Rotated(Vec3(0.0f, 0.0f, side * 0.12f));
			r.Add(SHAPE_ELLIPSOID, Vec3(side * 0.36f, 0.54f, 0.0f), Vec3(0.12f, 0.12f, 0.12f), skin, ANIM_BOB);
			r.Add(SHAPE_ELLIPSOID, Vec3(side * 0.08f, 1.34f, 0.19f), Vec3(0.05f, 0.06f, 0.03f), Vec3(0.08f, 0.08f, 0.08f), ANIM_BOB);
		}

		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.80f, 0.0f), Vec3(0.52f, 0.62f, 0.40f), coat, ANIM_BOB);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.56f, 0.0f), Vec3(0.54f, 0.07f, 0.42f), Vec3(0.20f, 0.17f, 0.14f), ANIM_BOB);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.10f, 0.0f), Vec3(0.46f, 0.12f, 0.40f), scarf, ANIM_BOB);
		r.Add(SHAPE_BOX, Vec3(0.12f, 0.96f, 0.20f), Vec3(0.10f, 0.26f, 0.04f), scarf * 0.9f, ANIM_BOB);

		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.33f, 0.0f), Vec3(0.40f, 0.42f, 0.40f), skin, ANIM_BOB);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.41f, -0.04f), Vec3(0.43f, 0.34f, 0.44f), hair, ANIM_BOB);

		r.Add(SHAPE_BOX, Vec3(0.0f, 0.78f, -0.28f), Vec3(0.42f, 0.46f, 0.20f), pack, ANIM_BOB);
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.03f, -0.28f), Vec3(0.44f, 0.08f, 0.22f), pack * 0.8f, ANIM_BOB);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.52f, -0.30f), Vec3(0.13f, 0.46f, 0.13f), scarf * 0.75f, ANIM_BOB)
			.Rotated(Vec3(0.0f, 0.0f, halfPi));
		break;
	}

	case MODEL_PIPE:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.08f, 0.0f), Vec3(0.09f, 0.16f, 0.09f), Vec3(0.20f, 0.18f, 0.16f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.50f, 0.0f), Vec3(0.07f, 0.84f, 0.07f), rust);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.14f, 0.0f), Vec3(0.085f, 0.20f, 0.085f), Vec3(0.30f, 0.34f, 0.32f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.62f, 0.0f), Vec3(0.075f, 0.10f, 0.075f), rust * 0.7f);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.90f, 0.0f), Vec3(0.11f, 0.08f, 0.11f), Vec3(0.35f, 0.25f, 0.18f));
		break;

	case MODEL_PIPE_PICKUP:
		r.Add(SHAPE_CYLINDER, Vec3(0.05f, 0.40f, 0.0f), Vec3(0.07f, 0.84f, 0.07f), rust, pickup).Rotated(Vec3(0.0f, 0.0f, halfPi));
		r.Add(SHAPE_CYLINDER, Vec3(-0.42f, 0.40f, 0.0f), Vec3(0.09f, 0.16f, 0.09f), Vec3(0.20f, 0.18f, 0.16f), pickup).Rotated(Vec3(0.0f, 0.0f, halfPi));
		r.Add(SHAPE_CYLINDER, Vec3(-0.30f, 0.40f, 0.0f), Vec3(0.085f, 0.20f, 0.085f), Vec3(0.30f, 0.34f, 0.32f), pickup).Rotated(Vec3(0.0f, 0.0f, halfPi));
		r.Add(SHAPE_DISC, Vec3(0.0f, 0.03f, 0.0f), Vec3(0.9f, 1.0f, 0.9f), Vec3(0.30f, 0.55f, 0.45f), ANIM_PULSE);
		break;

	case MODEL_SLEEPER:
		AddSleeper(r, false);
		break;

	case MODEL_SLEEPER_ELDER:
		AddSleeper(r, true);
		break;

	case MODEL_DREAM_MOTE:
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.05f, 0.2f), Vec3(0.12f, 0.12f, 0.12f), Vec3(0.60f, 0.95f, 0.82f), ANIM_HOVER | ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.05f, 0.2f), Vec3(0.07f, 0.07f, 0.07f), Vec3(0.95f, 1.00f, 0.92f), ANIM_HOVER | ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.05f, 0.2f), Vec3(0.24f, 0.04f, 0.24f), Vec3(0.40f, 0.80f, 0.70f), ANIM_HOVER | ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.05f, 0.2f), Vec3(0.04f, 0.30f, 0.04f), Vec3(0.40f, 0.80f, 0.70f), ANIM_HOVER | ANIM_PULSE);
		break;

	case MODEL_LETTER:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.015f, 0.0f), Vec3(0.42f, 0.03f, 0.30f), Vec3(0.86f, 0.84f, 0.76f), ANIM_PULSE);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.032f, 0.0f), Vec3(0.30f, 0.006f, 0.02f), Vec3(0.40f, 0.35f, 0.30f));
		r.Add(SHAPE_BOX, Vec3(-0.02f, 0.032f, 0.06f), Vec3(0.24f, 0.006f, 0.015f), Vec3(0.45f, 0.40f, 0.34f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.12f, 0.035f, -0.08f), Vec3(0.06f, 0.02f, 0.06f), Vec3(0.55f, 0.30f, 0.24f));
		break;

	case MODEL_HOUSE_A:
		AddHouse(r, Vec3(0.44f, 0.42f, 0.38f), Vec3(0.21f, 0.21f, 0.20f));
		break;

	case MODEL_HOUSE_B:
		AddHouse(r, Vec3(0.38f, 0.38f, 0.36f), Vec3(0.24f, 0.22f, 0.19f));
		break;

	case MODEL_TREE:
		r.Add(SHAPE_CONE, Vec3(0.0f, 0.25f, 0.0f), Vec3(0.95f, 0.5f, 0.95f), bark * 0.9f);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.3f, 0.0f), Vec3(0.42f, 2.6f, 0.42f), bark);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.12f, 0.9f, 0.18f), Vec3(0.14f, 0.9f, 0.10f), kDarkMoss);
		r.Add(SHAPE_CYLINDER, Vec3(0.40f, 2.3f, 0.0f), Vec3(0.12f, 1.0f, 0.12f), bark, ANIM_SWAY).Rotated(Vec3(0.0f, 0.0f, -0.7f));
		r.Add(SHAPE_CYLINDER, Vec3(-0.34f, 2.5f, 0.1f), Vec3(0.11f, 0.9f, 0.11f), bark, ANIM_SWAY).Rotated(Vec3(0.0f, 0.0f, 0.6f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 2.4f, -0.32f), Vec3(0.10f, 0.9f, 0.10f), bark, ANIM_SWAY).Rotated(Vec3(-0.6f, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 2.62f, 0.0f), Vec3(2.5f, 0.7f, 2.5f), Vec3(0.12f, 0.21f, 0.15f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 2.95f, 0.0f), Vec3(2.8f, 1.7f, 2.8f), Vec3(0.17f, 0.28f, 0.19f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.95f, 3.25f, 0.55f), Vec3(1.5f, 1.1f, 1.5f), Vec3(0.19f, 0.30f, 0.19f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.85f, 3.35f, -0.60f), Vec3(1.6f, 1.2f, 1.6f), Vec3(0.16f, 0.27f, 0.18f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.1f, 3.93f, -0.1f), Vec3(2.3f, 1.5f, 2.3f), Vec3(0.20f, 0.31f, 0.19f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.60f, 4.30f, 0.65f), Vec3(1.0f, 0.8f, 1.0f), Vec3(0.24f, 0.35f, 0.20f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.1f, 4.77f, 0.05f), Vec3(1.6f, 1.3f, 1.6f), Vec3(0.23f, 0.34f, 0.19f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.95f, 2.55f, 1.05f), Vec3(0.12f, 0.12f, 0.12f), sporeGlow, ANIM_SWAY | ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-1.10f, 3.00f, -0.55f), Vec3(0.10f, 0.10f, 0.10f), sporeGlow, ANIM_SWAY | ANIM_PULSE);
		break;

	case MODEL_PINE:
		r.Add(SHAPE_CONE, Vec3(0.0f, 0.18f, 0.0f), Vec3(0.70f, 0.36f, 0.70f), Vec3(0.21f, 0.17f, 0.13f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.9f, 0.0f), Vec3(0.33f, 1.8f, 0.33f), Vec3(0.24f, 0.19f, 0.15f));
		r.Add(SHAPE_CONE, Vec3(0.0f, 1.75f, 0.0f), Vec3(2.5f, 1.4f, 2.5f), Vec3(0.11f, 0.21f, 0.16f), ANIM_SWAY);
		r.Add(SHAPE_CONE, Vec3(0.0f, 2.35f, 0.0f), Vec3(2.2f, 1.6f, 2.2f), Vec3(0.13f, 0.24f, 0.18f), ANIM_SWAY);
		r.Add(SHAPE_CONE, Vec3(0.0f, 3.0f, 0.0f), Vec3(1.8f, 1.5f, 1.8f), Vec3(0.14f, 0.26f, 0.19f), ANIM_SWAY);
		r.Add(SHAPE_CONE, Vec3(0.0f, 3.6f, 0.0f), Vec3(1.4f, 1.4f, 1.4f), Vec3(0.15f, 0.28f, 0.19f), ANIM_SWAY);
		r.Add(SHAPE_CONE, Vec3(0.0f, 4.2f, 0.0f), Vec3(0.95f, 1.3f, 0.95f), Vec3(0.17f, 0.31f, 0.20f), ANIM_SWAY);
		r.Add(SHAPE_CONE, Vec3(0.0f, 4.75f, 0.0f), Vec3(0.45f, 0.8f, 0.45f), Vec3(0.20f, 0.34f, 0.22f), ANIM_SWAY);
		break;

	case MODEL_WELL:
	{
		const Vec3 wellStone(0.33f, 0.33f, 0.31f);
		const Vec3 wood(0.25f, 0.23f, 0.19f);

		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.425f, 0.0f), Vec3(1.9f, 0.85f, 1.9f), wellStone);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.28f, 0.0f), Vec3(1.94f, 0.07f, 1.94f), wellStone * 0.8f);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.58f, 0.0f), Vec3(1.93f, 0.06f, 1.93f), wellStone * 0.86f);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.88f, 0.0f), Vec3(2.05f, 0.12f, 2.05f), wellStone * 1.12f);
		r.Add(SHAPE_DISC, Vec3(0.0f, 0.945f, 0.0f), Vec3(1.5f, 1.0f, 1.5f), Vec3(0.08f, 0.13f, 0.15f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.7f, 0.2f, 0.6f), Vec3(0.6f, 0.3f, 0.5f), moss);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.8f, 0.9f, -0.4f), Vec3(0.5f, 0.12f, 0.4f), kDarkMoss, ANIM_PULSE);

		r.Add(SHAPE_CYLINDER, Vec3(-0.85f, 1.2f, 0.0f), Vec3(0.16f, 2.4f, 0.16f), wood);
		r.Add(SHAPE_CYLINDER, Vec3(0.85f, 1.2f, 0.0f), Vec3(0.16f, 2.4f, 0.16f), wood);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 2.05f, 0.0f), Vec3(0.10f, 1.84f, 0.10f), wood * 0.9f).Rotated(Vec3(0.0f, 0.0f, halfPi));
		r.Add(SHAPE_BOX, Vec3(1.0f, 1.9f, 0.0f), Vec3(0.05f, 0.30f, 0.05f), kMetal);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.55f, 0.0f), Vec3(0.025f, 1.0f, 0.025f), Vec3(0.45f, 0.40f, 0.32f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.0f, 0.0f), Vec3(0.28f, 0.26f, 0.28f), Vec3(0.30f, 0.25f, 0.18f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.12f, 0.0f), Vec3(0.30f, 0.03f, 0.30f), kMetal);

		r.Add(SHAPE_PRISM, Vec3(0.0f, 2.62f, 0.0f), Vec3(2.5f, 0.55f, 2.2f), Vec3(0.22f, 0.21f, 0.18f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 2.35f, 0.0f), Vec3(2.1f, 0.08f, 0.14f), wood * 0.85f);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.4f, 2.75f, 0.45f), Vec3(0.9f, 0.10f, 0.7f), moss, ANIM_PULSE).Rotated(Vec3(0.46f, 0.0f, 0.0f));
		break;
	}

	case MODEL_TRUCK:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.75f, 0.45f), Vec3(2.1f, 0.9f, 3.1f), Vec3(0.30f, 0.27f, 0.23f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.215f, 0.45f), Vec3(1.9f, 0.04f, 2.9f), Vec3(0.14f, 0.13f, 0.12f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.10f, -1.55f), Vec3(1.9f, 1.6f, 1.3f), Vec3(0.26f, 0.25f, 0.22f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.55f, -2.21f), Vec3(1.6f, 0.5f, 0.03f), kGlass);
		r.Add(SHAPE_BOX, Vec3(0.955f, 1.50f, -1.55f), Vec3(0.02f, 0.45f, 0.8f), kGlass);
		r.Add(SHAPE_BOX, Vec3(-0.955f, 1.50f, -1.55f), Vec3(0.02f, 0.45f, 0.8f), kGlass);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.80f, -2.215f), Vec3(0.8f, 0.32f, 0.02f), kMetal);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.45f, -2.26f), Vec3(2.0f, 0.18f, 0.12f), kMetal);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.72f, 0.82f, -2.22f), Vec3(0.22f, 0.22f, 0.06f), Vec3(0.55f, 0.55f, 0.48f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.72f, 0.82f, -2.22f), Vec3(0.22f, 0.22f, 0.06f), Vec3(0.55f, 0.55f, 0.48f));
		r.Add(SHAPE_BOX, Vec3(-1.055f, 0.80f, 0.8f), Vec3(0.02f, 0.40f, 0.9f), rust);
		r.Add(SHAPE_BOX, Vec3(1.055f, 0.65f, -0.2f), Vec3(0.02f, 0.30f, 0.6f), rust * 0.9f);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.2f, 1.25f, 0.6f), Vec3(1.8f, 0.35f, 2.4f), moss, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.92f, -1.5f), Vec3(1.4f, 0.18f, 1.0f), kDarkMoss, ANIM_PULSE);
		r.Add(SHAPE_BOX, Vec3(1.06f, 0.9f, 0.3f), Vec3(0.02f, 0.55f, 0.5f), kDarkMoss);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.4f, 1.35f, 1.3f), Vec3(0.12f, 0.12f, 0.12f), sporeGlow, ANIM_PULSE);
		AddWheel(r, 1.0f, 1.2f, 0.7f, 0.3f);
		AddWheel(r, -1.0f, 1.2f, 0.7f, 0.3f);
		AddWheel(r, 1.0f, -1.4f, 0.7f, 0.3f);
		AddWheel(r, -1.0f, -1.4f, 0.7f, 0.3f);
		break;

	case MODEL_FENCE_POST:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.5f, 0.0f), Vec3(0.14f, 1.0f, 0.14f), Vec3(0.26f, 0.24f, 0.20f));
		r.Add(SHAPE_CONE, Vec3(0.0f, 1.04f, 0.0f), Vec3(0.16f, 0.08f, 0.16f), Vec3(0.22f, 0.20f, 0.17f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.06f, 0.0f), Vec3(0.30f, 0.12f, 0.30f), kDarkMoss);
		break;

	case MODEL_FENCE_RAIL:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.5f, 0.0f), Vec3(1.0f, 1.0f, 1.0f), Vec3(0.24f, 0.22f, 0.19f));
		break;

	case MODEL_SIGN:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.15f, 0.0f), Vec3(0.16f, 2.3f, 0.16f), Vec3(0.27f, 0.25f, 0.21f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 2.075f, 0.0f), Vec3(1.9f, 0.75f, 0.10f), Vec3(0.42f, 0.44f, 0.40f)).Rotated(Vec3(0.0f, -0.5f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 2.075f, 0.0f), Vec3(2.0f, 0.85f, 0.08f), Vec3(0.30f, 0.32f, 0.30f)).Rotated(Vec3(0.0f, -0.5f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 2.075f, 0.0f), Vec3(1.5f, 0.12f, 0.11f), Vec3(0.82f, 0.82f, 0.76f)).Rotated(Vec3(0.0f, -0.5f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 2.47f, 0.0f), Vec3(1.2f, 0.10f, 0.14f), moss, ANIM_PULSE).Rotated(Vec3(0.0f, -0.5f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.08f, 0.0f), Vec3(0.5f, 0.16f, 0.5f), kDarkMoss);
		break;

	case MODEL_ROCK:
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.35f, 0.0f), Vec3(1.2f, 0.8f, 1.0f), Vec3(0.36f, 0.36f, 0.34f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.45f, 0.22f, 0.3f), Vec3(0.7f, 0.5f, 0.6f), Vec3(0.32f, 0.33f, 0.31f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.42f, 0.18f, -0.25f), Vec3(0.6f, 0.38f, 0.5f), Vec3(0.34f, 0.34f, 0.33f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.2f, 0.55f, -0.2f), Vec3(0.55f, 0.35f, 0.45f), Vec3(0.39f, 0.39f, 0.37f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.75f, 0.06f, -0.35f), Vec3(0.22f, 0.14f, 0.18f), Vec3(0.30f, 0.30f, 0.29f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.05f, 0.66f, -0.05f), Vec3(0.8f, 0.14f, 0.7f), moss, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.45f, 0.30f, 0.25f), Vec3(0.35f, 0.10f, 0.30f), kDarkMoss);
		break;

	case MODEL_BUSH:
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.12f, 0.0f), Vec3(1.2f, 0.3f, 1.1f), Vec3(0.13f, 0.22f, 0.15f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.38f, 0.0f), Vec3(1.1f, 0.8f, 1.0f), Vec3(0.19f, 0.30f, 0.20f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.40f, 0.30f, 0.22f), Vec3(0.7f, 0.6f, 0.7f), Vec3(0.22f, 0.33f, 0.21f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.35f, 0.28f, -0.18f), Vec3(0.6f, 0.5f, 0.6f), Vec3(0.17f, 0.27f, 0.19f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.20f, 0.36f, 0.35f), Vec3(0.5f, 0.45f, 0.5f), Vec3(0.21f, 0.32f, 0.22f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.10f, 0.66f, -0.05f), Vec3(0.5f, 0.35f, 0.5f), Vec3(0.25f, 0.36f, 0.23f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.30f, 0.62f, 0.30f), Vec3(0.07f, 0.07f, 0.07f), Vec3(0.88f, 0.86f, 0.70f), ANIM_SWAY | ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.35f, 0.52f, 0.10f), Vec3(0.07f, 0.07f, 0.07f), Vec3(0.78f, 0.70f, 0.88f), ANIM_SWAY | ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.05f, 0.40f, 0.52f), Vec3(0.06f, 0.06f, 0.06f), Vec3(0.88f, 0.86f, 0.70f), ANIM_SWAY | ANIM_PULSE);
		break;

	case MODEL_RUIN:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.9f, 0.0f), Vec3(3.0f, 1.8f, 0.35f), stone);
		r.Add(SHAPE_BOX, Vec3(0.55f, 1.0f, 0.176f), Vec3(0.60f, 0.50f, 0.01f), Vec3(0.12f, 0.12f, 0.12f));
		r.Add(SHAPE_BOX, Vec3(-0.8f, 0.9f, 0.176f), Vec3(0.02f, 1.2f, 0.01f), Vec3(0.26f, 0.26f, 0.25f)).Rotated(Vec3(0.0f, 0.0f, 0.3f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.06f, 0.0f), Vec3(3.1f, 0.12f, 0.45f), stone * 0.8f);
		r.Add(SHAPE_BOX, Vec3(1.9f, 0.45f, 0.15f), Vec3(0.9f, 0.9f, 0.35f), Vec3(0.37f, 0.37f, 0.35f)).Rotated(Vec3(0.0f, 0.35f, 0.12f));
		r.Add(SHAPE_BOX, Vec3(-1.3f, 0.12f, 0.75f), Vec3(0.45f, 0.24f, 0.35f), Vec3(0.35f, 0.35f, 0.33f)).Rotated(Vec3(0.0f, 0.6f, 0.1f));
		r.Add(SHAPE_BOX, Vec3(0.9f, 0.08f, 0.8f), Vec3(0.30f, 0.16f, 0.25f), Vec3(0.38f, 0.38f, 0.36f)).Rotated(Vec3(0.0f, -0.4f, 0.0f));
		r.Add(SHAPE_CYLINDER, Vec3(-0.8f, 2.0f, 0.0f), Vec3(0.05f, 0.5f, 0.05f), rust);
		r.Add(SHAPE_CYLINDER, Vec3(0.6f, 1.95f, 0.05f), Vec3(0.05f, 0.4f, 0.05f), rust).Rotated(Vec3(0.3f, 0.0f, 0.2f));
		r.Add(SHAPE_CYLINDER, Vec3(1.2f, 1.98f, -0.05f), Vec3(0.04f, 0.45f, 0.04f), rust * 0.85f).Rotated(Vec3(-0.2f, 0.0f, -0.35f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.84f, 0.0f), Vec3(2.4f, 0.08f, 0.45f), moss, ANIM_PULSE);
		r.Add(SHAPE_BOX, Vec3(-0.3f, 1.3f, 0.18f), Vec3(0.9f, 1.0f, 0.02f), kDarkMoss);
		r.Add(SHAPE_ELLIPSOID, Vec3(-1.0f, 0.2f, 0.4f), Vec3(1.1f, 0.4f, 0.7f), moss, ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(1.2f, 0.15f, -0.45f), Vec3(0.8f, 0.3f, 0.5f), kDarkMoss, ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.4f, 1.9f, 0.1f), Vec3(0.10f, 0.10f, 0.10f), sporeGlow, ANIM_PULSE);
		break;

	case MODEL_CAR:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.55f, 0.0f), Vec3(1.8f, 0.7f, 3.8f), Vec3(0.36f, 0.30f, 0.26f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.15f, -0.2f), Vec3(1.6f, 0.55f, 2.0f), Vec3(0.30f, 0.28f, 0.26f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.15f, -0.2f), Vec3(1.62f, 0.36f, 1.7f), kGlass);
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.15f, -0.2f), Vec3(1.64f, 0.36f, 0.08f), Vec3(0.30f, 0.28f, 0.26f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.34f, 1.93f), Vec3(1.84f, 0.16f, 0.10f), kMetal);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.34f, -1.93f), Vec3(1.84f, 0.16f, 0.10f), kMetal);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.62f, 0.66f, 1.90f), Vec3(0.28f, 0.14f, 0.06f), Vec3(0.55f, 0.55f, 0.48f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.62f, 0.66f, 1.90f), Vec3(0.28f, 0.14f, 0.06f), Vec3(0.55f, 0.55f, 0.48f));
		r.Add(SHAPE_BOX, Vec3(0.62f, 0.66f, -1.905f), Vec3(0.26f, 0.10f, 0.02f), Vec3(0.42f, 0.18f, 0.16f));
		r.Add(SHAPE_BOX, Vec3(-0.62f, 0.66f, -1.905f), Vec3(0.26f, 0.10f, 0.02f), Vec3(0.42f, 0.18f, 0.16f));
		r.Add(SHAPE_BOX, Vec3(0.905f, 0.60f, 0.9f), Vec3(0.02f, 0.30f, 0.8f), rust);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.46f, -0.2f), Vec3(1.3f, 0.2f, 1.7f), moss, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.3f, 0.92f, 1.3f), Vec3(1.0f, 0.12f, 0.9f), kDarkMoss);
		AddWheel(r, 0.9f, 1.2f, 0.6f, 0.25f);
		AddWheel(r, -0.9f, 1.2f, 0.6f, 0.25f);
		AddWheel(r, 0.9f, -1.2f, 0.6f, 0.25f);
		AddWheel(r, -0.9f, -1.2f, 0.6f, 0.25f);
		break;

	case MODEL_LANTERN:
	{
		const Vec3 light(0.95f, 0.85f, 0.55f);

		r.Add(SHAPE_BOX, Vec3(0.0f, 0.10f, 0.0f), Vec3(1.0f, 0.2f, 1.0f), stone * 0.85f);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.26f, 0.0f), Vec3(0.75f, 0.12f, 0.75f), stone);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.78f, 0.0f), Vec3(0.32f, 0.95f, 0.32f), stone);
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.22f, 0.0f), Vec3(0.72f, 0.08f, 0.72f), stone * 0.9f);
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.45f, 0.0f), Vec3(0.52f, 0.36f, 0.52f), light, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.45f, 0.0f), Vec3(0.30f, 0.30f, 0.30f), Vec3(1.0f, 0.95f, 0.75f), ANIM_PULSE);
		for (int i = 0; i < 4; ++i)
		{
			float x = (i % 2 == 0) ? -0.29f : 0.29f;
			float z = (i < 2) ? -0.29f : 0.29f;
			r.Add(SHAPE_BOX, Vec3(x, 1.45f, z), Vec3(0.08f, 0.40f, 0.08f), stone * 0.8f);
		}
		r.Add(SHAPE_CONE, Vec3(0.0f, 1.83f, 0.0f), Vec3(1.05f, 0.40f, 1.05f), Vec3(0.34f, 0.34f, 0.32f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.66f, 0.0f), Vec3(1.05f, 0.05f, 1.05f), Vec3(0.30f, 0.30f, 0.28f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 2.08f, 0.0f), Vec3(0.14f, 0.18f, 0.14f), stone * 0.9f);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.25f, 1.78f, 0.2f), Vec3(0.45f, 0.08f, 0.4f), moss, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.3f, 0.2f, 0.35f), Vec3(0.45f, 0.14f, 0.35f), kDarkMoss);
		break;
	}

	case MODEL_MITE:
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.32f, -0.10f), Vec3(0.7f, 0.45f, 0.70f), Vec3(0.36f, 0.42f, 0.30f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.30f, 0.28f), Vec3(0.44f, 0.32f, 0.36f), Vec3(0.32f, 0.38f, 0.27f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.44f, -0.12f), Vec3(0.62f, 0.14f, 0.60f), Vec3(0.28f, 0.34f, 0.24f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.55f, -0.12f), Vec3(0.38f, 0.30f, 0.38f), sporeGlow, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.18f, 0.50f, -0.30f), Vec3(0.14f, 0.12f, 0.14f), sporeGlow, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.12f, 0.38f, 0.42f), Vec3(0.09f, 0.09f, 0.09f), Vec3(0.95f, 0.85f, 0.40f), ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.12f, 0.38f, 0.42f), Vec3(0.09f, 0.09f, 0.09f), Vec3(0.95f, 0.85f, 0.40f), ANIM_PULSE);
		r.Add(SHAPE_CYLINDER, Vec3(-0.10f, 0.52f, 0.50f), Vec3(0.02f, 0.30f, 0.02f), dark).Rotated(Vec3(0.7f, 0.0f, 0.3f));
		r.Add(SHAPE_CYLINDER, Vec3(0.10f, 0.52f, 0.50f), Vec3(0.02f, 0.30f, 0.02f), dark).Rotated(Vec3(0.7f, 0.0f, -0.3f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.15f, 0.64f, 0.60f), Vec3(0.05f, 0.05f, 0.05f), sporeGlow, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.15f, 0.64f, 0.60f), Vec3(0.05f, 0.05f, 0.05f), sporeGlow, ANIM_PULSE);
		for (int leg = 0; leg < 6; ++leg)
		{
			float side = (leg % 2 == 0) ? 1.0f : -1.0f;
			float front = 0.26f - 0.24f * (float)(leg / 2);
			r.Add(SHAPE_CYLINDER, Vec3(side * 0.36f, 0.14f, front), Vec3(0.05f, 0.36f, 0.05f), dark)
				.Rotated(Vec3(0.0f, 0.0f, side * 0.9f));
		}
		break;

	case MODEL_HUSK:
	{
		const Vec3 body(0.30f, 0.32f, 0.28f);
		const Vec3 limb(0.28f, 0.30f, 0.26f);

		r.Add(SHAPE_CYLINDER, Vec3(-0.14f, 0.32f, 0.0f), Vec3(0.18f, 0.64f, 0.18f), limb);
		r.Add(SHAPE_CYLINDER, Vec3(0.14f, 0.32f, 0.02f), Vec3(0.18f, 0.64f, 0.18f), limb).Rotated(Vec3(0.12f, 0.0f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.95f, 0.0f), Vec3(0.6f, 0.75f, 0.45f), body).Rotated(Vec3(0.15f, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.50f, 0.10f), Vec3(0.42f, 0.46f, 0.42f), Vec3(0.35f, 0.38f, 0.32f)).Rotated(Vec3(0.3f, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.12f, -0.05f), Vec3(0.78f, 0.9f, 0.62f), moss, ANIM_BREATHE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.62f, 0.02f), Vec3(0.46f, 0.26f, 0.46f), kDarkMoss, ANIM_BREATHE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.09f, 1.52f, 0.30f), Vec3(0.07f, 0.07f, 0.07f), sporeGlow, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.09f, 1.52f, 0.30f), Vec3(0.07f, 0.07f, 0.07f), sporeGlow, ANIM_PULSE);
		r.Add(SHAPE_CYLINDER, Vec3(-0.40f, 0.78f, 0.10f), Vec3(0.14f, 0.9f, 0.14f), limb).Rotated(Vec3(-0.25f, 0.0f, 0.1f));
		r.Add(SHAPE_CYLINDER, Vec3(0.40f, 0.78f, 0.10f), Vec3(0.14f, 0.9f, 0.14f), limb).Rotated(Vec3(-0.25f, 0.0f, -0.1f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.44f, 0.32f, 0.20f), Vec3(0.14f, 0.16f, 0.14f), limb * 1.1f);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.44f, 0.32f, 0.20f), Vec3(0.14f, 0.16f, 0.14f), limb * 1.1f);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.30f, 1.36f, -0.10f), Vec3(0.20f, 0.20f, 0.20f), sporeGlow, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.26f, 1.30f, -0.16f), Vec3(0.14f, 0.14f, 0.14f), sporeGlow, ANIM_PULSE);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.70f, -0.26f), Vec3(0.5f, 0.9f, 0.04f), kDarkMoss, ANIM_SWAY);
		break;
	}

	case MODEL_BOAR:
	{
		const Vec3 hide(0.34f, 0.27f, 0.22f);

		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.78f, 0.0f), Vec3(1.3f, 1.1f, 2.1f), hide);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.95f, 0.45f), Vec3(1.2f, 1.1f, 1.0f), hide * 0.92f);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.74f, 1.05f), Vec3(0.8f, 0.75f, 0.9f), Vec3(0.30f, 0.24f, 0.20f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.64f, 1.52f), Vec3(0.34f, 0.25f, 0.34f), Vec3(0.40f, 0.30f, 0.28f)).Rotated(Vec3(halfPi, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.07f, 0.66f, 1.645f), Vec3(0.06f, 0.08f, 0.02f), Vec3(0.12f, 0.08f, 0.08f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.07f, 0.66f, 1.645f), Vec3(0.06f, 0.08f, 0.02f), Vec3(0.12f, 0.08f, 0.08f));
		r.Add(SHAPE_CONE, Vec3(-0.22f, 0.56f, 1.46f), Vec3(0.10f, 0.36f, 0.10f), Vec3(0.85f, 0.82f, 0.72f)).Rotated(Vec3(1.0f, 0.0f, 0.0f));
		r.Add(SHAPE_CONE, Vec3(0.22f, 0.56f, 1.46f), Vec3(0.10f, 0.36f, 0.10f), Vec3(0.85f, 0.82f, 0.72f)).Rotated(Vec3(1.0f, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.22f, 0.92f, 1.38f), Vec3(0.08f, 0.08f, 0.08f), Vec3(0.95f, 0.60f, 0.30f), ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.22f, 0.92f, 1.38f), Vec3(0.08f, 0.08f, 0.08f), Vec3(0.95f, 0.60f, 0.30f), ANIM_PULSE);
		r.Add(SHAPE_CONE, Vec3(-0.30f, 1.12f, 1.05f), Vec3(0.16f, 0.26f, 0.10f), hide * 0.85f).Rotated(Vec3(-0.3f, 0.0f, 0.5f));
		r.Add(SHAPE_CONE, Vec3(0.30f, 1.12f, 1.05f), Vec3(0.16f, 0.26f, 0.10f), hide * 0.85f).Rotated(Vec3(-0.3f, 0.0f, -0.5f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.95f, -1.10f), Vec3(0.05f, 0.35f, 0.05f), hide * 0.8f).Rotated(Vec3(-0.8f, 0.0f, 0.0f));
		for (int leg = 0; leg < 4; ++leg)
		{
			float side = (leg % 2 == 0) ? 1.0f : -1.0f;
			float front = (leg < 2) ? 1.0f : -1.0f;
			r.Add(SHAPE_CYLINDER, Vec3(side * 0.42f, 0.25f, front * 0.62f), Vec3(0.24f, 0.5f, 0.24f), Vec3(0.22f, 0.18f, 0.15f));
			r.Add(SHAPE_CYLINDER, Vec3(side * 0.42f, 0.04f, front * 0.62f), Vec3(0.26f, 0.08f, 0.26f), Vec3(0.12f, 0.10f, 0.09f));
		}
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.26f, -0.1f), Vec3(0.95f, 0.36f, 1.6f), moss, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.40f, 0.35f), Vec3(0.30f, 0.30f, 0.80f), kDarkMoss, ANIM_SWAY);
		r.Add(SHAPE_CYLINDER, Vec3(0.22f, 1.42f, 0.2f), Vec3(0.07f, 0.18f, 0.07f), Vec3(0.85f, 0.80f, 0.66f));
		r.Add(SHAPE_CONE, Vec3(0.22f, 1.56f, 0.2f), Vec3(0.32f, 0.16f, 0.32f), Vec3(0.78f, 0.52f, 0.32f));
		r.Add(SHAPE_CYLINDER, Vec3(-0.18f, 1.40f, -0.35f), Vec3(0.05f, 0.14f, 0.05f), Vec3(0.85f, 0.80f, 0.66f));
		r.Add(SHAPE_CONE, Vec3(-0.18f, 1.50f, -0.35f), Vec3(0.22f, 0.12f, 0.22f), Vec3(0.72f, 0.56f, 0.36f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.30f, 1.30f, -0.60f), Vec3(0.12f, 0.12f, 0.12f), sporeGlow, ANIM_PULSE);
		break;
	}

	case MODEL_HERB:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.25f, 0.0f), Vec3(0.05f, 0.5f, 0.05f), Vec3(0.30f, 0.55f, 0.30f), pickup);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.12f, 0.30f, 0.0f), Vec3(0.24f, 0.07f, 0.12f), Vec3(0.40f, 0.75f, 0.40f), pickup).Rotated(Vec3(0.0f, 0.0f, -0.45f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.12f, 0.36f, 0.0f), Vec3(0.24f, 0.07f, 0.12f), Vec3(0.40f, 0.75f, 0.40f), pickup).Rotated(Vec3(0.0f, 0.0f, 0.45f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.20f, 0.11f), Vec3(0.12f, 0.06f, 0.22f), Vec3(0.34f, 0.66f, 0.36f), pickup).Rotated(Vec3(0.45f, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.42f, -0.10f), Vec3(0.12f, 0.06f, 0.20f), Vec3(0.44f, 0.78f, 0.42f), pickup).Rotated(Vec3(-0.45f, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.55f, 0.0f), Vec3(0.15f, 0.15f, 0.15f), Vec3(0.85f, 0.95f, 0.60f), pickup | ANIM_PULSE);
		break;

	case MODEL_WATER_FLASK:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.30f, 0.0f), Vec3(0.30f, 0.46f, 0.30f), Vec3(0.45f, 0.62f, 0.70f), pickup | ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.53f, 0.0f), Vec3(0.30f, 0.10f, 0.30f), Vec3(0.45f, 0.62f, 0.70f), pickup | ANIM_PULSE);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.60f, 0.0f), Vec3(0.14f, 0.10f, 0.14f), Vec3(0.30f, 0.25f, 0.20f), pickup);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.30f, 0.0f), Vec3(0.32f, 0.08f, 0.32f), Vec3(0.35f, 0.30f, 0.25f), pickup);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.14f, 0.0f), Vec3(0.31f, 0.04f, 0.31f), Vec3(0.35f, 0.30f, 0.25f), pickup);
		break;

	case MODEL_RELIC:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.32f, 0.0f), Vec3(0.34f, 0.54f, 0.08f), Vec3(0.55f, 0.55f, 0.60f), pickup);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.34f, 0.045f), Vec3(0.27f, 0.38f, 0.01f), Vec3(0.20f, 0.60f, 0.70f), pickup | ANIM_PULSE);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.34f, -0.045f), Vec3(0.20f, 0.30f, 0.01f), Vec3(0.40f, 0.40f, 0.44f), pickup);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.62f, 0.0f), Vec3(0.08f, 0.06f, 0.08f), Vec3(0.45f, 0.45f, 0.50f), pickup);
		break;

	case MODEL_SHADOW:
		r.Add(SHAPE_DISC, Vec3(0.0f, 0.02f, 0.0f), Vec3(1.0f, 1.0f, 1.0f), Vec3(0.0f, 0.0f, 0.0f));
		break;

	case MODEL_GROUND:
		r.Add(SHAPE_PLANE, Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 1.0f), Vec3(1.0f, 1.0f, 1.0f));
		break;

	case MODEL_GRANDMA:
	{
		const Vec3 skirt(0.30f, 0.26f, 0.30f);
		const Vec3 cardigan(0.52f, 0.44f, 0.36f);
		const Vec3 apron(0.62f, 0.60f, 0.54f);
		const Vec3 hair(0.80f, 0.79f, 0.76f);

		r.Add(SHAPE_BOX, Vec3(-0.10f, 0.05f, 0.05f), Vec3(0.14f, 0.10f, 0.24f), Vec3(0.20f, 0.17f, 0.15f));
		r.Add(SHAPE_BOX, Vec3(0.10f, 0.05f, 0.05f), Vec3(0.14f, 0.10f, 0.24f), Vec3(0.20f, 0.17f, 0.15f));
		r.Add(SHAPE_CONE, Vec3(0.0f, 0.40f, 0.0f), Vec3(0.62f, 0.70f, 0.52f), skirt, ANIM_BREATHE);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.86f, -0.03f), Vec3(0.46f, 0.46f, 0.38f), cardigan, ANIM_BREATHE)
			.Rotated(Vec3(0.18f, 0.0f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.55f, 0.20f), Vec3(0.36f, 0.55f, 0.03f), apron, ANIM_BREATHE);
		for (int i = 0; i < 2; ++i)
		{
			float side = (i == 0) ? -1.0f : 1.0f;
			r.Add(SHAPE_CYLINDER, Vec3(side * 0.20f, 0.78f, 0.12f), Vec3(0.12f, 0.36f, 0.12f), cardigan * 0.92f, ANIM_BREATHE)
				.Rotated(Vec3(0.9f, 0.0f, side * -0.2f));
			r.Add(SHAPE_ELLIPSOID, Vec3(side * 0.07f, 0.70f, 0.30f), Vec3(0.10f, 0.09f, 0.10f), skin, ANIM_BREATHE);
			r.Add(SHAPE_ELLIPSOID, Vec3(side * 0.07f, 1.20f, 0.20f), Vec3(0.04f, 0.03f, 0.02f), Vec3(0.10f, 0.08f, 0.08f), ANIM_BREATHE);
		}
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.17f, 0.08f), Vec3(0.34f, 0.36f, 0.34f), skin, ANIM_BREATHE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.25f, 0.02f), Vec3(0.37f, 0.28f, 0.37f), hair, ANIM_BREATHE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.30f, -0.17f), Vec3(0.18f, 0.18f, 0.18f), hair * 0.95f, ANIM_BREATHE);
		break;
	}

	case MODEL_SHED:
	{
		const Vec3 plank(0.33f, 0.27f, 0.21f);
		const Vec3 tin(0.36f, 0.30f, 0.26f);

		r.Add(SHAPE_BOX, Vec3(0.0f, 0.03f, 0.0f), Vec3(1.04f, 0.06f, 1.04f), Vec3(0.28f, 0.28f, 0.27f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.45f, 0.0f), Vec3(1.0f, 0.9f, 1.0f), plank);
		for (int i = 0; i < 5; ++i)
			r.Add(SHAPE_BOX, Vec3(-0.40f + 0.2f * (float)i, 0.45f, 0.502f), Vec3(0.015f, 0.9f, 0.01f), plank * 0.7f);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.33f, 0.505f), Vec3(0.34f, 0.66f, 0.02f), Vec3(0.07f, 0.06f, 0.05f));
		r.Add(SHAPE_BOX, Vec3(0.30f, 0.33f, 0.62f), Vec3(0.30f, 0.66f, 0.03f), plank * 0.85f).Rotated(Vec3(0.0f, -0.9f, 0.0f));
		r.Add(SHAPE_PRISM, Vec3(0.0f, 1.04f, 0.0f), Vec3(1.0f, 0.28f, 1.0f), plank * 0.9f);
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.07f, 0.26f), Vec3(1.12f, 0.03f, 0.64f), tin).Rotated(Vec3(0.51f, 0.0f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.07f, -0.26f), Vec3(1.12f, 0.03f, 0.64f), tin * 0.85f).Rotated(Vec3(-0.51f, 0.0f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(-0.35f, 1.10f, 0.20f), Vec3(0.30f, 0.035f, 0.30f), rust).Rotated(Vec3(0.51f, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.30f, 1.12f, -0.22f), Vec3(0.34f, 0.06f, 0.28f), moss, ANIM_PULSE).Rotated(Vec3(-0.51f, 0.0f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(-0.62f, 0.25f, 0.20f), Vec3(0.22f, 0.50f, 0.22f), Vec3(0.30f, 0.24f, 0.18f));
		r.Add(SHAPE_CYLINDER, Vec3(-0.62f, 0.55f, -0.25f), Vec3(0.26f, 0.44f, 0.26f), Vec3(0.26f, 0.30f, 0.30f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.55f, 0.06f, -0.45f), Vec3(0.30f, 0.14f, 0.40f), kDarkMoss);
		break;
	}

	case MODEL_GARDEN_BED:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.08f, 0.0f), Vec3(1.0f, 0.16f, 1.0f), Vec3(0.24f, 0.19f, 0.14f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.165f, 0.0f), Vec3(0.12f, 0.01f, 0.96f), Vec3(0.18f, 0.14f, 0.10f));
		r.Add(SHAPE_BOX, Vec3(-0.53f, 0.07f, 0.0f), Vec3(0.06f, 0.14f, 1.04f), Vec3(0.30f, 0.26f, 0.20f));
		r.Add(SHAPE_BOX, Vec3(0.53f, 0.07f, 0.0f), Vec3(0.06f, 0.14f, 1.04f), Vec3(0.30f, 0.26f, 0.20f));
		break;

	case MODEL_VEGETABLE:
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.05f, 0.0f), Vec3(0.22f, 0.22f, 0.22f), Vec3(0.86f, 0.84f, 0.78f));
		for (int i = 0; i < 5; ++i)
		{
			float a = (float)i * 1.2566f;
			r.Add(SHAPE_ELLIPSOID, Vec3(sinf(a) * 0.08f, 0.26f, cosf(a) * 0.08f), Vec3(0.09f, 0.34f, 0.05f), Vec3(0.30f, 0.52f, 0.28f), ANIM_SWAY)
				.Rotated(Vec3(cosf(a) * 0.4f, 0.0f, -sinf(a) * 0.4f));
		}
		break;

	case MODEL_BERRY_RED:
		AddBerryBush(r, Vec3(0.78f, 0.20f, 0.20f));
		break;

	case MODEL_BERRY_PALE:
		AddBerryBush(r, Vec3(0.86f, 0.86f, 0.80f));
		break;

	case MODEL_DEER:
	{
		const Vec3 coat(0.42f, 0.36f, 0.30f);
		const Vec3 glow(0.95f, 0.88f, 0.55f);

		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.05f, 0.0f), Vec3(0.55f, 0.55f, 1.25f), coat, ANIM_BREATHE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.00f, 0.0f), Vec3(0.45f, 0.35f, 1.10f), coat * 1.25f, ANIM_BREATHE);
		for (int leg = 0; leg < 4; ++leg)
		{
			float side = (leg % 2 == 0) ? 1.0f : -1.0f;
			float front = (leg < 2) ? 1.0f : -1.0f;
			r.Add(SHAPE_CYLINDER, Vec3(side * 0.16f, 0.42f, front * 0.42f), Vec3(0.08f, 0.84f, 0.08f), coat * 0.8f);
		}
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.45f, 0.58f), Vec3(0.18f, 0.60f, 0.18f), coat).Rotated(Vec3(0.5f, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.78f, 0.80f), Vec3(0.22f, 0.24f, 0.40f), coat * 1.05f);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.72f, 1.00f), Vec3(0.10f, 0.08f, 0.06f), Vec3(0.10f, 0.09f, 0.08f));
		r.Add(SHAPE_CONE, Vec3(-0.12f, 1.92f, 0.72f), Vec3(0.08f, 0.16f, 0.05f), coat).Rotated(Vec3(0.0f, 0.0f, 0.6f));
		r.Add(SHAPE_CONE, Vec3(0.12f, 1.92f, 0.72f), Vec3(0.08f, 0.16f, 0.05f), coat).Rotated(Vec3(0.0f, 0.0f, -0.6f));
		for (int i = 0; i < 2; ++i)
		{
			float side = (i == 0) ? -1.0f : 1.0f;
			r.Add(SHAPE_CYLINDER, Vec3(side * 0.12f, 2.12f, 0.74f), Vec3(0.04f, 0.45f, 0.04f), Vec3(0.60f, 0.55f, 0.45f))
				.Rotated(Vec3(-0.3f, 0.0f, side * -0.4f));
			r.Add(SHAPE_CYLINDER, Vec3(side * 0.26f, 2.34f, 0.66f), Vec3(0.03f, 0.30f, 0.03f), Vec3(0.60f, 0.55f, 0.45f))
				.Rotated(Vec3(0.3f, 0.0f, side * -0.9f));
			r.Add(SHAPE_ELLIPSOID, Vec3(side * 0.22f, 2.34f, 0.70f), Vec3(0.09f, 0.09f, 0.09f), glow, ANIM_PULSE);
			r.Add(SHAPE_ELLIPSOID, Vec3(side * 0.38f, 2.44f, 0.62f), Vec3(0.08f, 0.08f, 0.08f), glow, ANIM_PULSE);
		}
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.18f, -0.62f), Vec3(0.12f, 0.16f, 0.10f), Vec3(0.86f, 0.84f, 0.78f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.32f, -0.10f), Vec3(0.30f, 0.10f, 0.60f), moss, ANIM_PULSE);
		break;
	}

	case MODEL_CANNED_FOOD:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.22f, 0.0f), Vec3(0.26f, 0.32f, 0.26f), Vec3(0.60f, 0.62f, 0.62f), pickup);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.22f, 0.0f), Vec3(0.265f, 0.20f, 0.265f), Vec3(0.85f, 0.55f, 0.30f), pickup | ANIM_PULSE);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.385f, 0.0f), Vec3(0.24f, 0.01f, 0.24f), Vec3(0.70f, 0.72f, 0.72f), pickup);
		break;

	case MODEL_BUS_STOP:
	{
		const Vec3 frame(0.30f, 0.34f, 0.34f);

		r.Add(SHAPE_BOX, Vec3(0.0f, 0.03f, 0.0f), Vec3(3.2f, 0.06f, 1.5f), Vec3(0.34f, 0.34f, 0.32f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.2f, -0.65f), Vec3(3.0f, 2.2f, 0.06f), Vec3(0.20f, 0.26f, 0.27f));
		r.Add(SHAPE_BOX, Vec3(-0.6f, 1.4f, -0.61f), Vec3(1.1f, 0.8f, 0.02f), Vec3(0.55f, 0.50f, 0.40f));
		r.Add(SHAPE_BOX, Vec3(-1.5f, 1.2f, 0.0f), Vec3(0.08f, 2.4f, 0.08f), frame);
		r.Add(SHAPE_BOX, Vec3(1.5f, 1.2f, 0.0f), Vec3(0.08f, 2.4f, 0.08f), frame);
		r.Add(SHAPE_BOX, Vec3(0.0f, 2.42f, -0.1f), Vec3(3.4f, 0.08f, 1.5f), frame * 0.8f).Rotated(Vec3(-0.06f, 0.0f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.45f, -0.40f), Vec3(2.4f, 0.08f, 0.40f), Vec3(0.34f, 0.27f, 0.20f));
		r.Add(SHAPE_BOX, Vec3(-1.0f, 0.22f, -0.40f), Vec3(0.08f, 0.44f, 0.36f), frame);
		r.Add(SHAPE_BOX, Vec3(1.0f, 0.22f, -0.40f), Vec3(0.08f, 0.44f, 0.36f), frame);
		r.Add(SHAPE_CYLINDER, Vec3(1.9f, 1.3f, 0.5f), Vec3(0.08f, 2.6f, 0.08f), frame);
		r.Add(SHAPE_CYLINDER, Vec3(1.9f, 2.5f, 0.5f), Vec3(0.55f, 0.06f, 0.55f), Vec3(0.30f, 0.45f, 0.55f)).Rotated(Vec3(kPi * 0.5f, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.3f, 2.5f, -0.1f), Vec3(2.0f, 0.2f, 1.2f), moss, ANIM_PULSE);
		r.Add(SHAPE_BOX, Vec3(0.9f, 1.4f, -0.61f), Vec3(1.0f, 1.6f, 0.02f), kDarkMoss);
		r.Add(SHAPE_ELLIPSOID, Vec3(-1.4f, 0.2f, 0.5f), Vec3(0.9f, 0.4f, 0.7f), moss, ANIM_SWAY);
		break;
	}

	case MODEL_GINKGO:
	{
		const Vec3 leafA(0.78f, 0.66f, 0.24f);
		const Vec3 leafB(0.86f, 0.74f, 0.30f);

		r.Add(SHAPE_CONE, Vec3(0.0f, 0.3f, 0.0f), Vec3(1.2f, 0.6f, 1.2f), bark * 0.9f);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.8f, 0.0f), Vec3(0.55f, 3.6f, 0.55f), bark);
		r.Add(SHAPE_CYLINDER, Vec3(0.5f, 3.2f, 0.1f), Vec3(0.16f, 1.4f, 0.16f), bark, ANIM_SWAY).Rotated(Vec3(0.0f, 0.0f, -0.6f));
		r.Add(SHAPE_CYLINDER, Vec3(-0.45f, 3.5f, -0.1f), Vec3(0.15f, 1.3f, 0.15f), bark, ANIM_SWAY).Rotated(Vec3(0.0f, 0.0f, 0.6f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 4.2f, 0.0f), Vec3(3.4f, 2.4f, 3.4f), leafA, ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.9f, 4.8f, 0.5f), Vec3(2.0f, 1.6f, 2.0f), leafB, ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.8f, 5.0f, -0.6f), Vec3(2.1f, 1.7f, 2.1f), leafA * 0.92f, ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 5.9f, 0.1f), Vec3(1.8f, 1.5f, 1.8f), leafB, ANIM_SWAY);
		r.Add(SHAPE_DISC, Vec3(0.0f, 0.02f, 0.0f), Vec3(5.0f, 1.0f, 5.0f), leafA * 0.85f);
		break;
	}

	case MODEL_POLE:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 2.8f, 0.0f), Vec3(0.24f, 5.6f, 0.24f), Vec3(0.30f, 0.26f, 0.21f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 5.2f, 0.0f), Vec3(1.8f, 0.10f, 0.12f), Vec3(0.26f, 0.22f, 0.18f));
		r.Add(SHAPE_CYLINDER, Vec3(-0.7f, 5.32f, 0.0f), Vec3(0.08f, 0.14f, 0.08f), Vec3(0.55f, 0.58f, 0.55f));
		r.Add(SHAPE_CYLINDER, Vec3(0.7f, 5.32f, 0.0f), Vec3(0.08f, 0.14f, 0.08f), Vec3(0.55f, 0.58f, 0.55f));
		r.Add(SHAPE_CYLINDER, Vec3(0.7f, 3.9f, 0.3f), Vec3(0.025f, 2.9f, 0.025f), kMetal).Rotated(Vec3(0.2f, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.2f, 0.0f), Vec3(0.40f, 2.4f, 0.40f), kDarkMoss, ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.1f, 0.0f), Vec3(0.9f, 0.3f, 0.9f), moss);
		break;

	default:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.5f, 0.0f), Vec3(1.0f, 1.0f, 1.0f), Vec3(1.0f, 0.0f, 1.0f));
		break;
	}
	return r;
}

void LoadModelMeshes(std::vector<MeshData>& meshes, int& builtCount)
{
	_mkdir("Cache");
	_mkdir("Cache/Models");

	meshes.clear();
	meshes.resize(MODEL_COUNT);
	builtCount = 0;

	for (int id = 0; id < MODEL_COUNT; ++id)
	{
		ModelRecipe recipe = BuildRecipe(id);
		uint64_t hash = recipe.Hash();

		char path[128];
		sprintf_s(path, sizeof(path), "Cache/Models/%s.mdl", ModelName(id));

		if (LoadMeshCache(path, hash, meshes[id]))
			continue;

		meshes[id] = BuildMesh(recipe);
		SaveMeshCache(path, hash, meshes[id]);
		++builtCount;
	}
}
