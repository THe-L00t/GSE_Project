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
	};

	void AddWheel(ModelRecipe& r, float x, float z, float diameter, float width)
	{
		r.Add(SHAPE_CYLINDER, Vec3(x, diameter * 0.5f, z), Vec3(diameter, width, diameter), Vec3(0.11f, 0.11f, 0.10f))
			.Rotated(Vec3(0.0f, 0.0f, kPi * 0.5f));
	}

	void AddSleeper(ModelRecipe& r, bool elder)
	{
		const Vec3 cloth = elder ? Vec3(0.40f, 0.36f, 0.34f) : Vec3(0.33f, 0.33f, 0.32f);
		const float mossLength = elder ? 1.30f : 0.95f;

		r.Add(SHAPE_BOX, Vec3(0.0f, 0.19f, 0.0f), Vec3(0.58f, 0.38f, 1.75f), cloth, ANIM_BREATHE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.18f, 0.98f), Vec3(0.34f, 0.32f, 0.38f), Vec3(0.46f, 0.42f, 0.38f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.415f, -0.15f), Vec3(0.52f, 0.07f, mossLength), Vec3(0.26f, 0.46f, 0.30f), ANIM_BREATHE | ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.12f, 0.44f, 0.40f), Vec3(0.18f, 0.10f, 0.18f), Vec3(0.55f, 0.95f, 0.80f), ANIM_BREATHE | ANIM_PULSE);
	}

	void AddHouse(ModelRecipe& r, const Vec3& wall, const Vec3& roof)
	{
		const Vec3 moss(0.24f, 0.38f, 0.24f);
		const Vec3 glass(0.12f, 0.16f, 0.18f);

		r.Add(SHAPE_BOX, Vec3(0.0f, 0.5f, 0.0f), Vec3(1.0f, 1.0f, 1.0f), wall);
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.0625f, 0.0f), Vec3(1.10f, 0.125f, 1.12f), roof);
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.18f, 0.0f), Vec3(0.62f, 0.11f, 0.62f), roof * 0.88f);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.24f, 0.505f), Vec3(0.16f, 0.48f, 0.02f), Vec3(0.16f, 0.14f, 0.12f));
		r.Add(SHAPE_BOX, Vec3(-0.28f, 0.58f, 0.505f), Vec3(0.18f, 0.18f, 0.02f), glass);
		r.Add(SHAPE_BOX, Vec3(0.28f, 0.58f, 0.505f), Vec3(0.18f, 0.18f, 0.02f), glass);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.30f, 1.10f, 0.25f), Vec3(0.35f, 0.10f, 0.30f), moss, ANIM_PULSE);
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
	const Vec3 moss(0.24f, 0.38f, 0.24f);
	const Vec3 sporeGlow(0.55f, 0.95f, 0.80f);
	const float halfPi = kPi * 0.5f;
	const int pickup = ANIM_SPIN | ANIM_HOVER;

	ModelRecipe r;
	switch (id)
	{
	case MODEL_PLAYER:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.56f, 0.0f), Vec3(0.55f, 1.12f, 0.42f), Vec3(0.30f, 0.36f, 0.36f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.32f, 0.0f), Vec3(0.40f, 0.42f, 0.40f), skin);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.71f, -0.26f), Vec3(0.44f, 0.46f, 0.24f), Vec3(0.34f, 0.30f, 0.24f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.08f, 0.0f), Vec3(0.46f, 0.10f, 0.40f), Vec3(0.42f, 0.52f, 0.46f));
		break;

	case MODEL_PIPE:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.08f, 0.0f), Vec3(0.09f, 0.16f, 0.09f), Vec3(0.20f, 0.18f, 0.16f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.50f, 0.0f), Vec3(0.07f, 0.84f, 0.07f), rust);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.90f, 0.0f), Vec3(0.11f, 0.08f, 0.11f), Vec3(0.35f, 0.25f, 0.18f));
		break;

	case MODEL_PIPE_PICKUP:
		r.Add(SHAPE_CYLINDER, Vec3(0.05f, 0.40f, 0.0f), Vec3(0.07f, 0.84f, 0.07f), rust, pickup).Rotated(Vec3(0.0f, 0.0f, halfPi));
		r.Add(SHAPE_CYLINDER, Vec3(-0.42f, 0.40f, 0.0f), Vec3(0.09f, 0.16f, 0.09f), Vec3(0.20f, 0.18f, 0.16f), pickup).Rotated(Vec3(0.0f, 0.0f, halfPi));
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
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.05f, 0.2f), Vec3(0.24f, 0.04f, 0.24f), Vec3(0.40f, 0.80f, 0.70f), ANIM_HOVER | ANIM_PULSE);
		break;

	case MODEL_LETTER:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.015f, 0.0f), Vec3(0.42f, 0.03f, 0.30f), Vec3(0.86f, 0.84f, 0.76f), ANIM_PULSE);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.032f, 0.0f), Vec3(0.30f, 0.006f, 0.02f), Vec3(0.40f, 0.35f, 0.30f));
		break;

	case MODEL_HOUSE_A:
		AddHouse(r, Vec3(0.44f, 0.42f, 0.38f), Vec3(0.21f, 0.21f, 0.20f));
		break;

	case MODEL_HOUSE_B:
		AddHouse(r, Vec3(0.38f, 0.38f, 0.36f), Vec3(0.24f, 0.22f, 0.19f));
		break;

	case MODEL_TREE:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.3f, 0.0f), Vec3(0.45f, 2.6f, 0.45f), Vec3(0.22f, 0.19f, 0.16f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 2.95f, 0.0f), Vec3(2.8f, 1.7f, 2.8f), Vec3(0.17f, 0.28f, 0.19f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.1f, 3.93f, -0.1f), Vec3(2.3f, 1.5f, 2.3f), Vec3(0.20f, 0.31f, 0.19f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.1f, 4.77f, 0.05f), Vec3(1.6f, 1.3f, 1.6f), Vec3(0.23f, 0.34f, 0.19f), ANIM_SWAY);
		break;

	case MODEL_PINE:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.9f, 0.0f), Vec3(0.35f, 1.8f, 0.35f), Vec3(0.24f, 0.19f, 0.15f));
		r.Add(SHAPE_CONE, Vec3(0.0f, 2.2f, 0.0f), Vec3(2.2f, 2.0f, 2.2f), Vec3(0.13f, 0.24f, 0.18f), ANIM_SWAY);
		r.Add(SHAPE_CONE, Vec3(0.0f, 3.2f, 0.0f), Vec3(1.7f, 1.8f, 1.7f), Vec3(0.15f, 0.27f, 0.19f), ANIM_SWAY);
		r.Add(SHAPE_CONE, Vec3(0.0f, 4.1f, 0.0f), Vec3(1.1f, 1.5f, 1.1f), Vec3(0.17f, 0.30f, 0.20f), ANIM_SWAY);
		break;

	case MODEL_WELL:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.425f, 0.0f), Vec3(1.9f, 0.85f, 1.9f), Vec3(0.33f, 0.33f, 0.31f));
		r.Add(SHAPE_DISC, Vec3(0.0f, 0.86f, 0.0f), Vec3(1.5f, 1.0f, 1.5f), Vec3(0.10f, 0.16f, 0.18f));
		r.Add(SHAPE_CYLINDER, Vec3(-0.85f, 1.2f, 0.0f), Vec3(0.16f, 2.4f, 0.16f), Vec3(0.25f, 0.23f, 0.19f));
		r.Add(SHAPE_CYLINDER, Vec3(0.85f, 1.2f, 0.0f), Vec3(0.16f, 2.4f, 0.16f), Vec3(0.25f, 0.23f, 0.19f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 2.54f, 0.0f), Vec3(2.6f, 0.28f, 2.2f), Vec3(0.22f, 0.21f, 0.18f));
		break;

	case MODEL_TRUCK:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.75f, 0.45f), Vec3(2.1f, 0.9f, 3.1f), Vec3(0.30f, 0.27f, 0.23f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.10f, -1.55f), Vec3(1.9f, 1.6f, 1.3f), Vec3(0.26f, 0.25f, 0.22f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.55f, -2.21f), Vec3(1.6f, 0.5f, 0.03f), Vec3(0.14f, 0.18f, 0.20f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.2f, 1.25f, 0.6f), Vec3(1.8f, 0.35f, 2.4f), Vec3(0.20f, 0.33f, 0.20f), ANIM_PULSE);
		AddWheel(r, 1.0f, 1.2f, 0.7f, 0.3f);
		AddWheel(r, -1.0f, 1.2f, 0.7f, 0.3f);
		AddWheel(r, 1.0f, -1.4f, 0.7f, 0.3f);
		AddWheel(r, -1.0f, -1.4f, 0.7f, 0.3f);
		break;

	case MODEL_FENCE_POST:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.5f, 0.0f), Vec3(0.14f, 1.0f, 0.14f), Vec3(0.26f, 0.24f, 0.20f));
		break;

	case MODEL_FENCE_RAIL:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.5f, 0.0f), Vec3(1.0f, 1.0f, 1.0f), Vec3(0.24f, 0.22f, 0.19f));
		break;

	case MODEL_SIGN:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 1.15f, 0.0f), Vec3(0.16f, 2.3f, 0.16f), Vec3(0.27f, 0.25f, 0.21f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 2.075f, 0.0f), Vec3(1.9f, 0.75f, 0.10f), Vec3(0.42f, 0.44f, 0.40f)).Rotated(Vec3(0.0f, -0.5f, 0.0f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 2.075f, 0.0f), Vec3(1.5f, 0.12f, 0.11f), Vec3(0.82f, 0.82f, 0.76f)).Rotated(Vec3(0.0f, -0.5f, 0.0f));
		break;

	case MODEL_ROCK:
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.35f, 0.0f), Vec3(1.2f, 0.8f, 1.0f), Vec3(0.36f, 0.36f, 0.34f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.45f, 0.22f, 0.3f), Vec3(0.7f, 0.5f, 0.6f), Vec3(0.32f, 0.33f, 0.31f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.05f, 0.66f, -0.05f), Vec3(0.8f, 0.14f, 0.7f), moss, ANIM_PULSE);
		break;

	case MODEL_BUSH:
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.38f, 0.0f), Vec3(1.1f, 0.8f, 1.0f), Vec3(0.19f, 0.30f, 0.20f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.40f, 0.30f, 0.22f), Vec3(0.7f, 0.6f, 0.7f), Vec3(0.22f, 0.33f, 0.21f), ANIM_SWAY);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.35f, 0.28f, -0.18f), Vec3(0.6f, 0.5f, 0.6f), Vec3(0.17f, 0.27f, 0.19f), ANIM_SWAY);
		break;

	case MODEL_RUIN:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.9f, 0.0f), Vec3(3.0f, 1.8f, 0.35f), stone);
		r.Add(SHAPE_BOX, Vec3(1.9f, 0.45f, 0.15f), Vec3(0.9f, 0.9f, 0.35f), Vec3(0.37f, 0.37f, 0.35f)).Rotated(Vec3(0.0f, 0.35f, 0.12f));
		r.Add(SHAPE_CYLINDER, Vec3(-0.8f, 2.0f, 0.0f), Vec3(0.05f, 0.5f, 0.05f), rust);
		r.Add(SHAPE_CYLINDER, Vec3(0.6f, 1.95f, 0.05f), Vec3(0.05f, 0.4f, 0.05f), rust).Rotated(Vec3(0.3f, 0.0f, 0.2f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.84f, 0.0f), Vec3(2.4f, 0.08f, 0.45f), moss, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-1.0f, 0.2f, 0.4f), Vec3(1.1f, 0.4f, 0.7f), moss, ANIM_SWAY);
		break;

	case MODEL_CAR:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.55f, 0.0f), Vec3(1.8f, 0.7f, 3.8f), Vec3(0.36f, 0.30f, 0.26f));
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.15f, -0.2f), Vec3(1.6f, 0.55f, 2.0f), Vec3(0.30f, 0.28f, 0.26f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.46f, -0.2f), Vec3(1.3f, 0.2f, 1.7f), moss, ANIM_PULSE);
		AddWheel(r, 0.9f, 1.2f, 0.6f, 0.25f);
		AddWheel(r, -0.9f, 1.2f, 0.6f, 0.25f);
		AddWheel(r, 0.9f, -1.2f, 0.6f, 0.25f);
		AddWheel(r, -0.9f, -1.2f, 0.6f, 0.25f);
		break;

	case MODEL_LANTERN:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.15f, 0.0f), Vec3(0.9f, 0.3f, 0.9f), stone);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.8f, 0.0f), Vec3(0.35f, 1.0f, 0.35f), stone);
		r.Add(SHAPE_BOX, Vec3(0.0f, 1.45f, 0.0f), Vec3(0.6f, 0.4f, 0.6f), Vec3(0.95f, 0.85f, 0.55f), ANIM_PULSE);
		r.Add(SHAPE_CONE, Vec3(0.0f, 1.85f, 0.0f), Vec3(0.95f, 0.4f, 0.95f), Vec3(0.34f, 0.34f, 0.32f));
		break;

	case MODEL_MITE:
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.32f, 0.0f), Vec3(0.7f, 0.45f, 0.85f), Vec3(0.36f, 0.42f, 0.30f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.55f, -0.12f), Vec3(0.38f, 0.30f, 0.38f), sporeGlow, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.12f, 0.38f, 0.38f), Vec3(0.09f, 0.09f, 0.09f), Vec3(0.95f, 0.85f, 0.40f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.12f, 0.38f, 0.38f), Vec3(0.09f, 0.09f, 0.09f), Vec3(0.95f, 0.85f, 0.40f));
		for (int leg = 0; leg < 4; ++leg)
		{
			float side = (leg % 2 == 0) ? 1.0f : -1.0f;
			float front = (leg < 2) ? 1.0f : -1.0f;
			r.Add(SHAPE_CYLINDER, Vec3(side * 0.36f, 0.14f, front * 0.22f), Vec3(0.06f, 0.36f, 0.06f), dark)
				.Rotated(Vec3(0.0f, 0.0f, side * 0.9f));
		}
		break;

	case MODEL_HUSK:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.65f, 0.0f), Vec3(0.6f, 1.3f, 0.45f), Vec3(0.30f, 0.32f, 0.28f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.52f, 0.0f), Vec3(0.42f, 0.46f, 0.42f), Vec3(0.35f, 0.38f, 0.32f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.12f, -0.05f), Vec3(0.78f, 0.9f, 0.62f), moss, ANIM_BREATHE);
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.09f, 1.55f, 0.19f), Vec3(0.07f, 0.07f, 0.07f), sporeGlow, ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.09f, 1.55f, 0.19f), Vec3(0.07f, 0.07f, 0.07f), sporeGlow, ANIM_PULSE);
		r.Add(SHAPE_CYLINDER, Vec3(-0.40f, 0.78f, 0.05f), Vec3(0.14f, 0.9f, 0.14f), Vec3(0.28f, 0.30f, 0.26f)).Rotated(Vec3(-0.25f, 0.0f, 0.0f));
		r.Add(SHAPE_CYLINDER, Vec3(0.40f, 0.78f, 0.05f), Vec3(0.14f, 0.9f, 0.14f), Vec3(0.28f, 0.30f, 0.26f)).Rotated(Vec3(-0.25f, 0.0f, 0.0f));
		break;

	case MODEL_BOAR:
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.78f, 0.0f), Vec3(1.3f, 1.1f, 2.1f), Vec3(0.34f, 0.27f, 0.22f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.74f, 1.05f), Vec3(0.8f, 0.75f, 0.9f), Vec3(0.30f, 0.24f, 0.20f));
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.64f, 1.52f), Vec3(0.34f, 0.25f, 0.34f), Vec3(0.40f, 0.30f, 0.28f)).Rotated(Vec3(halfPi, 0.0f, 0.0f));
		r.Add(SHAPE_CONE, Vec3(-0.22f, 0.56f, 1.46f), Vec3(0.10f, 0.36f, 0.10f), Vec3(0.85f, 0.82f, 0.72f)).Rotated(Vec3(1.0f, 0.0f, 0.0f));
		r.Add(SHAPE_CONE, Vec3(0.22f, 0.56f, 1.46f), Vec3(0.10f, 0.36f, 0.10f), Vec3(0.85f, 0.82f, 0.72f)).Rotated(Vec3(1.0f, 0.0f, 0.0f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.22f, 0.92f, 1.38f), Vec3(0.08f, 0.08f, 0.08f), Vec3(0.95f, 0.60f, 0.30f), ANIM_PULSE);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.22f, 0.92f, 1.38f), Vec3(0.08f, 0.08f, 0.08f), Vec3(0.95f, 0.60f, 0.30f), ANIM_PULSE);
		for (int leg = 0; leg < 4; ++leg)
		{
			float side = (leg % 2 == 0) ? 1.0f : -1.0f;
			float front = (leg < 2) ? 1.0f : -1.0f;
			r.Add(SHAPE_CYLINDER, Vec3(side * 0.42f, 0.25f, front * 0.62f), Vec3(0.24f, 0.5f, 0.24f), Vec3(0.22f, 0.18f, 0.15f));
		}
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 1.26f, -0.1f), Vec3(0.95f, 0.36f, 1.6f), moss, ANIM_PULSE);
		r.Add(SHAPE_CYLINDER, Vec3(0.22f, 1.42f, 0.2f), Vec3(0.07f, 0.18f, 0.07f), Vec3(0.85f, 0.80f, 0.66f));
		r.Add(SHAPE_CONE, Vec3(0.22f, 1.56f, 0.2f), Vec3(0.32f, 0.16f, 0.32f), Vec3(0.78f, 0.52f, 0.32f));
		break;

	case MODEL_HERB:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.25f, 0.0f), Vec3(0.05f, 0.5f, 0.05f), Vec3(0.30f, 0.55f, 0.30f), pickup);
		r.Add(SHAPE_ELLIPSOID, Vec3(0.12f, 0.30f, 0.0f), Vec3(0.24f, 0.07f, 0.12f), Vec3(0.40f, 0.75f, 0.40f), pickup).Rotated(Vec3(0.0f, 0.0f, -0.45f));
		r.Add(SHAPE_ELLIPSOID, Vec3(-0.12f, 0.36f, 0.0f), Vec3(0.24f, 0.07f, 0.12f), Vec3(0.40f, 0.75f, 0.40f), pickup).Rotated(Vec3(0.0f, 0.0f, 0.45f));
		r.Add(SHAPE_ELLIPSOID, Vec3(0.0f, 0.55f, 0.0f), Vec3(0.15f, 0.15f, 0.15f), Vec3(0.85f, 0.95f, 0.60f), pickup | ANIM_PULSE);
		break;

	case MODEL_WATER_FLASK:
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.30f, 0.0f), Vec3(0.30f, 0.46f, 0.30f), Vec3(0.45f, 0.62f, 0.70f), pickup | ANIM_PULSE);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.58f, 0.0f), Vec3(0.14f, 0.10f, 0.14f), Vec3(0.30f, 0.25f, 0.20f), pickup);
		r.Add(SHAPE_CYLINDER, Vec3(0.0f, 0.30f, 0.0f), Vec3(0.32f, 0.08f, 0.32f), Vec3(0.35f, 0.30f, 0.25f), pickup);
		break;

	case MODEL_RELIC:
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.32f, 0.0f), Vec3(0.34f, 0.54f, 0.08f), Vec3(0.55f, 0.55f, 0.60f), pickup);
		r.Add(SHAPE_BOX, Vec3(0.0f, 0.34f, 0.045f), Vec3(0.27f, 0.38f, 0.01f), Vec3(0.20f, 0.60f, 0.70f), pickup | ANIM_PULSE);
		break;

	case MODEL_SHADOW:
		r.Add(SHAPE_DISC, Vec3(0.0f, 0.02f, 0.0f), Vec3(1.0f, 1.0f, 1.0f), Vec3(0.0f, 0.0f, 0.0f));
		break;

	case MODEL_GROUND:
		r.Add(SHAPE_PLANE, Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 1.0f), Vec3(1.0f, 1.0f, 1.0f));
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
