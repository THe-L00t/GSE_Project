#include "stdafx.h"
#include "Game.h"

#include <cstdio>

#include "Collision.h"
#include "Models.h"

namespace
{
	const float kExitZ = 26.0f;
	const float kDayHold = 0.62f;        // the first afternoon waits for the torch errand
	const float kDusk = 0.80f;
	const float kNightHold = 0.95f;      // the first night lasts until bed
	const float kSleepFade = 2.0f;       // seconds to black, and again back
	const float kLeaveFade = 1.6f;
	const float kDeerWatchRange = 10.0f;
	const float kDoorRange = 1.8f;
	const int   kForageGoal = 3;

	const Vec3 kUnitScale(1.0f, 1.0f, 1.0f);

	// Grandmother talks in a country dialect, and always about whether you have eaten.
	const char* kWakeLines[] =
	{
		"Up already, Yul? Did ye eat? ...Course ye didn't. Skin an' bones, this one.",
		"Fetch us some water from the well first, there's a dear. Then we'll see about feedin' ye.",
	};
	const char* kWellHintLines[] =
	{
		"Well's just there by the road, love. Mind ye don't lean in too far.",
	};
	const char* kWaterLines[] =
	{
		"Good child. Drink some yerself, go on. Ye look peaky.",
		"Now then. Pull a few radishes from the patch, an' pick berries up the back hill.",
		"Red'uns only, mind! Them pale ones'll twist yer belly.",
	};
	const char* kForageHintLines[] =
	{
		"Three radishes from the patch by the fence, an' red berries off the back hill, north past the trees.",
	};
	const char* kFoodLines[] =
	{
		"That's a good haul. Now eat a bit, I'm watchin' ye.",
	};
	const char* kEatHintLines[] =
	{
		"Go on, eat somethin'. I'll not have ye wastin' away.",
	};
	const char* kAteLines[] =
	{
		"There. Colour's comin' back already.",
		"One more thing, love. There's an old torch in the shed, from before the Bloom.",
		"Fetch it, would ye? Nights are long now. Shed's out east, past the Kims' place.",
	};
	const char* kTorchHintLines[] =
	{
		"The shed, love. East, past the Kims' place. Under the old tarp, I think.",
	};
	const char* kTorchLines[] =
	{
		"Ah, that's the one. Yer grandad's. Mind the battery, there's no more where that came from.",
		"Sun's goin' down already. Stay close to home tonight, an' come in 'fore it's dark.",
	};
	const char* kDuskHintLines[] =
	{
		"Stay close now, it's gettin' dark. Did ye eat enough today?",
	};
	const char* kDeerLines[] =
	{
		"Hush... look there, by the water. A lantern deer.",
		"Don't ye go followin' it tonight.",
	};
	const char* kBedLines[] =
	{
		"Did ye eat enough? ...Sleep, then.",
		"Them lights'll be gone by mornin'.",
	};

	const int kPackRows[] = { ITEM_CLEAN_WATER, ITEM_BERRIES, ITEM_VEGETABLE, ITEM_HERB };
	const int kPackItemRows = 4;

	template <int N>
	int LineCount(const char* (&)[N])
	{
		return N;
	}
}

PropActor* Game::AddProp(int model, const Vec3& pos, const Vec3& scale, float yaw, float halfX, float halfZ)
{
	PropActor* p = levelNode->AddChild(new PropActor(ACTOR_PROP, model));
	p->SetPosition(pos);
	p->SetYaw(yaw);
	p->SetScale(scale);
	p->phase = pos.x * 0.37f + pos.z * 0.21f;

	// A zero footprint takes no part in collision.
	if (halfX > 0.0f && halfZ > 0.0f)
	{
		p->collider.shape = COLLIDER_BOX;
		p->collider.halfX = halfX;
		p->collider.halfZ = halfZ;
	}
	return p;
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

SleeperActor* Game::AddSleeper(const Vec3& pos, float yaw, bool grandma, float phase)
{
	SleeperActor* s = levelNode->AddChild(new SleeperActor(grandma, phase));
	s->SetPosition(pos);
	s->SetYaw(yaw);
	return s;
}

void Game::AddForage(int kind, const Vec3& pos)
{
	ForageActor* f = levelNode->AddChild(new ForageActor(kind));
	f->SetPosition(pos);
	f->SetYaw(pos.x * 1.7f + pos.z * 0.9f);
	f->phase = pos.x * 0.37f + pos.z * 0.21f;
	if (kind != FORAGE_RADISH) f->SetScale(Vec3(1.1f, 1.1f, 1.1f));
}

void Game::BuildVillage()
{
	levelNode = worldNode->AddChild(new Actor(ACTOR_NODE));

	// Grandmother's house
	AddHouse(Vec3(-2.0f, 0.0f, -10.0f), 7.0f, 3.6f, 6.0f, 0.0f, MODEL_HOUSE_A);
	AddHouse(Vec3(9.5f, 0.0f, -7.0f), 5.5f, 3.2f, 5.0f, 0.12f, MODEL_HOUSE_B);
	AddHouse(Vec3(-10.5f, 0.0f, 3.0f), 5.0f, 3.0f, 4.6f, -0.15f, MODEL_HOUSE_A);
	AddHouse(Vec3(9.0f, 0.0f, 8.5f), 5.0f, 3.1f, 5.0f, 0.05f, MODEL_HOUSE_B);
	AddHouse(Vec3(-7.0f, 0.0f, 16.5f), 4.6f, 2.9f, 4.4f, 0.20f, MODEL_HOUSE_A);

	AddProp(MODEL_WELL, Vec3(2.5f, 0.0f, -2.0f), kUnitScale, 0.0f, 0.95f, 0.95f);
	AddProp(MODEL_SHED, Vec3(13.5f, 0.0f, -13.0f), Vec3(3.0f, 2.4f, 2.8f), 0.0f, 1.5f, 1.4f);

	homeDoor = Vec3(-2.0f, 0.0f, -6.7f);
	wellPos = Vec3(2.5f, 0.0f, -2.0f);
	shedDoor = Vec3(13.5f, 0.0f, -11.1f);

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

	// The back hill, north of the houses
	AddProp(MODEL_PINE, Vec3(-4.5f, 0.0f, -26.0f), Vec3(1.1f, 1.1f, 1.1f), 0.0f, 0.2f, 0.2f);
	AddProp(MODEL_PINE, Vec3(11.0f, 0.0f, -25.5f), Vec3(1.2f, 1.2f, 1.2f), 0.0f, 0.2f, 0.2f);
	AddProp(MODEL_PINE, Vec3(4.5f, 0.0f, -27.5f), Vec3(0.95f, 0.95f, 0.95f), 0.0f, 0.2f, 0.2f);
	AddProp(MODEL_ROCK, Vec3(9.8f, 0.0f, -22.4f), kUnitScale, 0.6f, 0.5f, 0.45f);
	AddProp(MODEL_ROCK, Vec3(-3.6f, 0.0f, -21.6f), Vec3(0.8f, 0.8f, 0.8f), 2.1f, 0.4f, 0.35f);

	AddForage(FORAGE_RED_BERRY, Vec3(2.5f, 0.0f, -21.0f));
	AddForage(FORAGE_RED_BERRY, Vec3(6.0f, 0.0f, -23.5f));
	AddForage(FORAGE_RED_BERRY, Vec3(-1.5f, 0.0f, -24.0f));
	AddForage(FORAGE_PALE_BERRY, Vec3(0.6f, 0.0f, -22.8f));
	AddForage(FORAGE_PALE_BERRY, Vec3(8.4f, 0.0f, -20.4f));

	// The vegetable patch inside the fence, west of the house front
	AddFence(Vec3(-6.0f, 0.0f, -5.5f), Vec3(-6.0f, 0.0f, 1.5f));
	AddFence(Vec3(-6.0f, 0.0f, 1.5f), Vec3(-1.0f, 0.0f, 1.5f));
	AddFence(Vec3(12.5f, 0.0f, 4.0f), Vec3(12.5f, 0.0f, 12.0f));

	for (int row = 0; row < 3; ++row)
	{
		float z = -3.8f + 1.6f * (float)row;
		AddProp(MODEL_GARDEN_BED, Vec3(-3.7f, 0.0f, z), Vec3(3.2f, 1.0f, 0.9f), 0.0f, 0.0f, 0.0f);
		AddForage(FORAGE_RADISH, Vec3(-4.7f, 0.14f, z));
		AddForage(FORAGE_RADISH, Vec3(-2.7f, 0.14f, z));
	}

	AddProp(MODEL_TRUCK, Vec3(11.5f, 0.0f, 15.0f), kUnitScale, 0.30f, 1.05f, 2.2f);

	// Road sign toward Route 32
	const float signZ = 24.5f;
	PropActor* sign = AddProp(MODEL_SIGN, Vec3(RoadCenter(signZ) + 3.2f, 0.0f, signZ), kUnitScale, 0.0f, 0.0f, 0.0f);
	sign->emissive = 0.10f;

	// After the props: the player is pushed out of the water last.
	water = levelNode->AddChild(new WaterActor(22.0f, 18.0f));
	water->SetPosition(Vec3(-15.0f, 0.03f, -14.0f));

	GroundParams ground;
	ground.stage = 0.0f;
	ground.dampCenter = water->Position();
	ground.dampStrength = 1.0f;
	levelNode->AddChild(new GroundActor(400.0f, ground));

	letter = levelNode->AddChild(new PropActor(ACTOR_LETTER, MODEL_LETTER));
	letter->SetPosition(Vec3(-2.9f, 0.0f, -5.2f));
	letter->SetYaw(0.4f);
	letter->emissive = 0.3f;
	letter->visible = false;

	grandmaNpc = levelNode->AddChild(new NpcActor());
	grandmaNpc->SetPosition(Vec3(0.4f, 0.0f, -6.0f));
	grandmaNpc->SetYaw(0.3f);

	// She lies on the porch only on the second morning.
	grandmaSleeper = AddSleeper(Vec3(-4.0f, 0.0f, -5.9f), 1.57f, true, 0.0f);
	grandmaSleeper->visible = false;

	// Neighbours who fell asleep last night, the first night the spores came.
	const float others[4][3] =
	{
		{ 8.5f, -3.5f, 1.20f },
		{ -8.5f, 6.0f, -0.60f },
		{ 6.5f, 11.0f, 2.10f },
		{ -4.5f, 17.5f, 0.80f },
	};
	for (int i = 0; i < 4; ++i)
		AddSleeper(Vec3(others[i][0], 0.0f, others[i][1]), others[i][2], false, 1.3f * (float)(i + 1));

	villageExit = levelNode->AddChild(new ExitActor());
	villageExit->SetPosition(Vec3(0.0f, 0.0f, kExitZ));

	player->SetPosition(Vec3(-0.6f, 0.0f, -4.4f));
	player->SetYaw(kPi);
	camera->SetPosition(player->Position());

	sporeVisual = 0.18f;
	sporeExposure = 0.05f;
}

void Game::UpdateVillage(float dt, const bool* keys)
{
	bool frozen = DialogOpen() || letterOpen || sleepTimer >= 0.0f || transitionTimer >= 0.0f;
	if (!frozen)
	{
		UpdatePlayer(dt, keys);
		ResolveVillageCollisions();

		// Clamp to the field, but leave the south open for the exit.
		Vec3 pos = player->Position();
		pos.x = Clampf(pos.x, -30.0f, 30.0f);
		pos.z = Clampf(pos.z, -28.0f, 32.0f);
		player->SetPosition(pos);
	}

	if (day == 1)
	{
		// The spores are only a pretty haze the first day; nothing builds up yet.
		sporeExposure = Approach(sporeExposure, 0.05f, 0.5f, dt);
	}
	else
	{
		// Nature Insight slows exposure; the water's edge clears it.
		float rate = 0.028f * NatureGuard(natureInsight);
		if (water->ShoreDistance(player->Position()) < water->clearRange) rate = -0.075f;

		// Rolling holds your breath.
		if (player->rollTimer > 0.0f) rate = Minf(rate, 0.0f);

		sporeExposure = Saturatef(sporeExposure + rate * dt);
	}

	UpdateDeer(dt);
	UpdateVillageStory(dt);
	UpdateInteractionTarget();

	// Last, because it tears the village down.
	if (transitionTimer >= kLeaveFade) StartRoute();
}

void Game::UpdateVillageStory(float dt)
{
	// The first day holds its afternoon until the errands are done, and its night until bed.
	if (day == 1 && tutorial < TUT_DUSK && timeOfDay > kDayHold) timeOfDay = kDayHold;
	if (day == 1 && tutorial >= TUT_DUSK && timeOfDay > kNightHold) timeOfDay = kNightHold;
	if (tutorial == TUT_DUSK && timeOfDay >= kDusk && timeScale > 1.5f) timeScale = 1.0f;

	float sporeTarget = (day == 1 && tutorial < TUT_DUSK) ? 0.18f : 0.55f;
	sporeVisual = Approach(sporeVisual, sporeTarget, 0.25f, dt);

	if (tutorial == TUT_WAKE && !DialogOpen() && levelTimer > 2.5f)
		Say(kWakeLines, LineCount(kWakeLines), TUT_FETCH_WATER);

	if (tutorial == TUT_EAT && mealsEaten > mealsBaseline && !DialogOpen())
		Say(kAteLines, LineCount(kAteLines), TUT_FETCH_TORCH);

	if (tutorial == TUT_DUSK && timeOfDay >= kDusk && !DialogOpen())
	{
		if (DistXZ(player->Position(), homeDoor) < 7.0f)
		{
			deer = levelNode->AddChild(new DeerActor());
			deer->SetPosition(Vec3(-25.0f, 0.0f, -2.9f));
			deer->SetYaw(kPi * 0.5f);
			deer->goal = Vec3(-5.0f, 0.0f, -2.9f);
			deer->speed = 1.1f;
			Say(kDeerLines, LineCount(kDeerLines), TUT_WATCH_DEER);
		}
		else if (messageTimer <= 0.0f)
		{
			ShowMessage("It is getting dark. Grandmother is calling you home.", 3.0f);
		}
	}

	if (sleepTimer >= 0.0f)
	{
		float before = sleepTimer;
		sleepTimer += dt;
		if (before < kSleepFade && sleepTimer >= kSleepFade) WakeNextMorning();
		if (sleepTimer >= kSleepFade * 2.0f) sleepTimer = -1.0f;
	}

	if (dayCardTimer >= 0.0f)
	{
		dayCardTimer += dt;
		if (dayCardTimer > 6.0f) dayCardTimer = -1.0f;
	}

	if (tutorial == TUT_LEAVE && transitionTimer < 0.0f && villageExit->Contains(player->Position()))
	{
		transitionTimer = 0.0f;
		tutorial = TUT_DONE;
	}
	if (transitionTimer >= 0.0f) transitionTimer += dt;
}

void Game::UpdateDeer(float dt)
{
	// It waits while Grandmother is speaking, so her words always come before it leaves.
	if (!deer || level != LEVEL_VILLAGE || DialogOpen()) return;

	Vec3 pos = deer->Position();
	Vec3 to(deer->goal.x - pos.x, 0.0f, deer->goal.z - pos.z);
	float dist = Length(to);

	if (dist > 0.1f)
	{
		float step = Minf(deer->speed * dt, dist);
		deer->SetPosition(pos + to * (step / dist));
		deer->SetYaw(atan2f(to.x, to.z));
		deer->stepPhase += dt * 4.0f;
		return;
	}

	// It reaches the reeds and is gone.
	deer->Destroy();
	deer = nullptr;
	if (tutorial == TUT_WATCH_DEER)
	{
		if (!deerWatched) ShowMessage("The deer slips away into the reeds.", 3.0f);
		SetTutorial(TUT_BEDTIME);
	}
}

void Game::ResolveVillageCollisions()
{
	// Axis-aligned pushes. Prop yaw is small enough that ignoring it reads fine.
	Vec3 pos = player->Position();
	for (size_t i = 0; i < levelNode->ChildCount(); ++i)
	{
		const Actor* a = levelNode->Child(i);
		if (a->IsDestroyed() || !a->visible) continue;

		if (a->collider.shape == COLLIDER_BOX)
			PushOutOfBox(pos, player->collider.radius, a->WorldPosition(), a->collider.halfX, a->collider.halfZ);
		else if (a->collider.shape == COLLIDER_CIRCLE)
			PushOutOfCircle(pos, player->collider.radius, a->WorldPosition(), a->collider.radius);
	}
	player->SetPosition(pos);
}

void Game::SetTutorial(int next)
{
	tutorial = next;

	switch (next)
	{
	case TUT_FETCH_WATER:
		ShowMessage("Walk to the well with WASD, then press E beside it.", 5.0f);
		break;

	case TUT_FORAGE:
		ShowMessage("The patch is inside the fence west of the house. The back hill is north.", 5.0f);
		break;

	case TUT_EAT:
		mealsBaseline = mealsEaten;
		ShowMessage("Press F to eat.", 4.0f);
		break;

	case TUT_DUSK:
		timeScale = 12.0f;
		ShowMessage("The sun sinks fast. Head home before dark.", 4.0f);
		break;

	case TUT_BEDTIME:
		ShowMessage("Go to bed: press E at the door.", 4.0f);
		break;

	case TUT_FIND_GRANDMOTHER:
		ShowMessage("Grandmother has not come out this morning.", 4.5f);
		break;

	case TUT_READ_LETTER:
		letter->visible = true;
		break;

	case TUT_PACK:
		inventory[ITEM_HERB] += 2;
		ShowMessage("On her shelf, two herbs wrapped in cloth. Pack your bag at the door.", 5.0f);
		break;

	case TUT_LEAVE:
		ShowMessage("Follow the road south, out of the village.", 5.0f);
		break;

	default:
		break;
	}
}

void Game::Say(const char* const* lines, int count, int next)
{
	dialogLines.assign(lines, lines + count);
	dialogIndex = 0;
	dialogNext = next;
	if (count <= 0) AdvanceDialog();
}

void Game::AdvanceDialog()
{
	++dialogIndex;
	if (DialogOpen()) return;

	dialogLines.clear();
	dialogIndex = 0;

	int next = dialogNext;
	dialogNext = -1;
	if (next >= 0) SetTutorial(next);

	// The bedtime words end the day.
	if (sleepAfterDialog)
	{
		sleepAfterDialog = false;
		sleepTimer = 0.0f;
	}
}

void Game::TalkToGrandmother()
{
	switch (tutorial)
	{
	case TUT_WAKE:        Say(kWakeLines, LineCount(kWakeLines), TUT_FETCH_WATER); break;
	case TUT_FETCH_WATER: Say(kWellHintLines, LineCount(kWellHintLines), -1); break;
	case TUT_BRING_WATER: Say(kWaterLines, LineCount(kWaterLines), TUT_FORAGE); break;
	case TUT_FORAGE:      Say(kForageHintLines, LineCount(kForageHintLines), -1); break;
	case TUT_BRING_FOOD:  Say(kFoodLines, LineCount(kFoodLines), TUT_EAT); break;
	case TUT_EAT:         Say(kEatHintLines, LineCount(kEatHintLines), -1); break;
	case TUT_FETCH_TORCH: Say(kTorchHintLines, LineCount(kTorchHintLines), -1); break;
	case TUT_BRING_TORCH: Say(kTorchLines, LineCount(kTorchLines), TUT_DUSK); break;
	default:              Say(kDuskHintLines, LineCount(kDuskHintLines), -1); break;
	}
}

void Game::Pick(ForageActor& f)
{
	if (f.picked) return;

	switch (f.forage)
	{
	case FORAGE_RADISH:
		f.picked = true;
		f.Destroy();
		++radishes;
		++inventory[ITEM_VEGETABLE];
		ShowMessage("You pull a radish and brush the soil off.  (I: bag)", 3.0f);
		break;

	case FORAGE_RED_BERRY:
		f.picked = true;
		f.model = MODEL_BUSH;
		++redBerries;
		++inventory[ITEM_BERRIES];
		if (redBerries == 1)
			AddInsight(1, "Red, not pale: you know which berries are safe now.");
		else
			ShowMessage("You pick a handful of red berries.", 2.5f);
		break;

	default:
		ShowMessage("Grandmother said the pale ones twist your belly. You leave them.", 3.5f);
		break;
	}

	if (tutorial == TUT_FORAGE && radishes >= kForageGoal && redBerries >= kForageGoal)
	{
		SetTutorial(TUT_BRING_FOOD);
		ShowMessage("That's plenty. Take it back to Grandmother.", 4.0f);
	}
}

void Game::WakeNextMorning()
{
	day = 2;
	timeOfDay = 0.27f;
	timeScale = 1.0f;
	torchOn = false;

	grandmaNpc->visible = false;
	grandmaNpc->collider.shape = COLLIDER_NONE;
	grandmaSleeper->visible = true;

	player->SetPosition(homeDoor + Vec3(0.8f, 0.0f, 1.6f));
	player->SetYaw(0.0f);
	camera->SetPosition(player->Position());

	sporeVisual = 0.55f;
	sporeExposure = 0.15f;
	dayCardTimer = 0.0f;
	SetTutorial(TUT_FIND_GRANDMOTHER);
}

void Game::UpdateInteractionTarget()
{
	targetSpot = SPOT_NONE;
	targetSleeper = nullptr;
	targetForage = nullptr;
	prompt.clear();

	if (tutorial == TUT_DONE || sleepTimer >= 0.0f || transitionTimer >= 0.0f || packOpen) return;

	if (DialogOpen())
	{
		targetSpot = SPOT_DIALOG;
		prompt = "[E]  Continue";
		return;
	}
	if (letterOpen)
	{
		targetSpot = SPOT_LETTER_OPEN;
		prompt = "[E]  Close";
		return;
	}

	const Vec3& pos = player->Position();

	if (day == 1 && DistXZ(pos, grandmaNpc->Position()) < kInteractRange)
	{
		targetSpot = SPOT_GRANDMA;
		prompt = "[E]  Talk to Grandmother";
		return;
	}

	// The letter wins over the sleeper it lies beside.
	if (tutorial == TUT_READ_LETTER && DistXZ(pos, letter->Position()) < kInteractRange)
	{
		targetSpot = SPOT_LETTER;
		prompt = "[E]  Read the letter";
		return;
	}

	if (DistXZ(pos, homeDoor) < kDoorRange && (tutorial == TUT_BEDTIME || tutorial == TUT_PACK))
	{
		targetSpot = SPOT_DOOR;
		if (tutorial == TUT_BEDTIME) prompt = "[E]  Go to bed";
		else prompt = riceEaten ? "[E]  Pack your bag" : "[E]  Eat the rice Grandmother left";
		return;
	}

	if (tutorial >= TUT_FETCH_WATER && DistXZ(pos, wellPos) < 2.2f)
	{
		targetSpot = SPOT_WELL;
		prompt = (tutorial == TUT_FETCH_WATER) ? "[E]  Draw water" : "[E]  Drink from the well";
		return;
	}

	if (tutorial == TUT_FETCH_TORCH && DistXZ(pos, shedDoor) < 2.2f)
	{
		targetSpot = SPOT_SHED;
		prompt = "[E]  Search the shed";
		return;
	}

	if (deer && tutorial == TUT_WATCH_DEER && !deerWatched && DistXZ(pos, deer->Position()) < kDeerWatchRange)
	{
		targetSpot = SPOT_DEER;
		prompt = "[E]  Watch the lantern deer";
		return;
	}

	if (tutorial >= TUT_FORAGE)
	{
		std::vector<ForageActor*> plants;
		SceneGraph::Collect(levelNode, ACTOR_FORAGE, plants);
		float best = kInteractRange;
		for (size_t i = 0; i < plants.size(); ++i)
		{
			if (plants[i]->picked) continue;
			float d = DistXZ(pos, plants[i]->Position());
			if (d < best)
			{
				best = d;
				targetForage = plants[i];
			}
		}

		if (targetForage)
		{
			targetSpot = SPOT_FORAGE;
			if (targetForage->forage == FORAGE_RADISH) prompt = "[E]  Pull the radish";
			else if (targetForage->forage == FORAGE_RED_BERRY) prompt = "[E]  Pick red berries";
			else prompt = "[E]  Pick pale berries";
			return;
		}
	}

	std::vector<SleeperActor*> sleepers;
	SceneGraph::Collect(levelNode, ACTOR_SLEEPER, sleepers);
	float best = kInteractRange;
	for (size_t i = 0; i < sleepers.size(); ++i)
	{
		if (!sleepers[i]->visible) continue;
		float d = DistXZ(pos, sleepers[i]->Position());
		if (d < best)
		{
			best = d;
			targetSleeper = sleepers[i];
		}
	}

	if (targetSleeper)
	{
		if (targetSleeper->isGrandma)
			prompt = (tutorial == TUT_FIND_GRANDMOTHER) ? "[E]  Look at Grandmother" : "[E]  Sit with her";
		else if (!targetSleeper->visited)
			prompt = "[E]  Rest beside them";

		targetSpot = prompt.empty() ? SPOT_NONE : SPOT_SLEEPER;
	}
}

void Game::TryInteract()
{
	switch (targetSpot)
	{
	case SPOT_DIALOG:
		AdvanceDialog();
		break;

	case SPOT_LETTER_OPEN:
		letterOpen = false;
		if (tutorial == TUT_READ_LETTER)
		{
			letter->visible = false;
			SetTutorial(TUT_PACK);
		}
		break;

	case SPOT_GRANDMA:
		TalkToGrandmother();
		break;

	case SPOT_LETTER:
		letterOpen = true;
		break;

	case SPOT_DOOR:
		if (tutorial == TUT_BEDTIME)
		{
			Say(kBedLines, LineCount(kBedLines), -1);
			sleepAfterDialog = true;
		}
		else if (!riceEaten)
		{
			riceEaten = true;
			foodMeter = 100.0f;
			ShowMessage("The rice in the pot is still warm. You eat every grain.", 4.0f);
		}
		else
		{
			OpenPack();
		}
		break;

	case SPOT_WELL:
		waterMeter = 100.0f;
		if (tutorial == TUT_FETCH_WATER)
		{
			inventory[ITEM_CLEAN_WATER] += 2;
			survivalShown = true;
			ShowMessage("You draw cold water, drink deep, and fill two flasks.  Water and food now show below.", 6.0f);
			SetTutorial(TUT_BRING_WATER);
		}
		else
		{
			ShowMessage("Cold, clean water.", 2.0f);
		}
		break;

	case SPOT_SHED:
		torchOwned = true;
		FindRelic(RELIC_TORCH);
		ShowMessage("Under a tarp: a hand torch from the old world. L switches it on.  B opens your Relic Journal.", 6.5f);
		SetTutorial(TUT_BRING_TORCH);
		break;

	case SPOT_DEER:
		deerWatched = true;
		AddInsight(1, "The lights on its antlers pulse like slow breathing.");
		break;

	case SPOT_FORAGE:
		if (targetForage) Pick(*targetForage);
		break;

	case SPOT_SLEEPER:
	{
		SleeperActor& s = *targetSleeper;
		if (s.isGrandma)
		{
			if (tutorial == TUT_FIND_GRANDMOTHER)
			{
				ShowMessage("She is breathing. Moss has started at her fingertips. A letter and a map lie beside her.", 6.5f);
				SetTutorial(TUT_READ_LETTER);
			}
			else
			{
				ShowMessage("She is breathing. She will not wake.", 3.5f);
			}
			break;
		}

		if (!s.visited)
		{
			s.visited = true;
			if (s.mote)
			{
				s.mote->Destroy();
				s.mote = nullptr;
			}
			++fragments;
			AddInsight(1, "A dream fragment: the first spring, seen from someone else's eyes.");
		}
		break;
	}

	default:
		break;
	}
}

void Game::OpenPack()
{
	packOpen = true;
	packCursor = 0;
	packTorch = torchOwned;

	for (int i = 0; i < ITEM_TYPE_COUNT; ++i)
	{
		packHave[i] = TakesBagSlot(i) ? inventory[i] : 0;
		packTake[i] = 0;
	}

	// A sensible bag to start from: water first, then food, then herbs.
	int room = kBagCapacity - (packTorch ? 1 : 0);
	for (int r = 0; r < kPackItemRows; ++r)
	{
		int type = kPackRows[r];
		int take = packHave[type] < room ? packHave[type] : room;
		packTake[type] = take;
		room -= take;
	}
}

void Game::HandlePackKey(unsigned char key)
{
	int rows = kPackItemRows + (torchOwned ? 1 : 0);
	int used = packTorch ? 1 : 0;
	for (int r = 0; r < kPackItemRows; ++r) used += packTake[kPackRows[r]];
	bool torchRow = packCursor == kPackItemRows;

	switch (key)
	{
	case 'w':
		packCursor = (packCursor + rows - 1) % rows;
		break;
	case 's':
		packCursor = (packCursor + 1) % rows;
		break;
	case 'd':
		if (used >= kBagCapacity) break;
		if (torchRow) packTorch = true;
		else if (packTake[kPackRows[packCursor]] < packHave[kPackRows[packCursor]]) ++packTake[kPackRows[packCursor]];
		break;
	case 'a':
		if (torchRow) packTorch = false;
		else if (packTake[kPackRows[packCursor]] > 0) --packTake[kPackRows[packCursor]];
		break;
	case 'e':
		ConfirmPack();
		break;
	case 27:
		packOpen = false;
		break;
	default:
		break;
	}
}

void Game::ConfirmPack()
{
	for (int r = 0; r < kPackItemRows; ++r)
		inventory[kPackRows[r]] = packTake[kPackRows[r]];

	if (!packTorch)
	{
		torchOwned = false;
		torchOn = false;
	}

	packOpen = false;
	ShowMessage("You shoulder the bag. What stays behind stays with her.", 4.0f);
	SetTutorial(TUT_LEAVE);
}

void Game::ObjectiveText(char* buf, size_t size) const
{
	switch (tutorial)
	{
	case TUT_WAKE:             sprintf_s(buf, size, "Talk to Grandmother."); break;
	case TUT_FETCH_WATER:      sprintf_s(buf, size, "Fetch water from the well."); break;
	case TUT_BRING_WATER:      sprintf_s(buf, size, "Bring the water to Grandmother."); break;
	case TUT_FORAGE:
		sprintf_s(buf, size, "Pull radishes (%d/%d) and pick red berries (%d/%d).",
				  radishes < kForageGoal ? radishes : kForageGoal, kForageGoal,
				  redBerries < kForageGoal ? redBerries : kForageGoal, kForageGoal);
		break;
	case TUT_BRING_FOOD:       sprintf_s(buf, size, "Take the food back to Grandmother."); break;
	case TUT_EAT:              sprintf_s(buf, size, "Eat something while she watches. (F)"); break;
	case TUT_FETCH_TORCH:      sprintf_s(buf, size, "Find the old torch in the shed, east of the village."); break;
	case TUT_BRING_TORCH:      sprintf_s(buf, size, "Bring the torch to Grandmother. (L switches it on)"); break;
	case TUT_DUSK:             sprintf_s(buf, size, "Evening is falling. Go home."); break;
	case TUT_WATCH_DEER:       sprintf_s(buf, size, "Watch the lantern deer by the water."); break;
	case TUT_BEDTIME:          sprintf_s(buf, size, "Go to bed. (E at the door)"); break;
	case TUT_FIND_GRANDMOTHER: sprintf_s(buf, size, "Find Grandmother."); break;
	case TUT_READ_LETTER:      sprintf_s(buf, size, "Read the letter beside her."); break;
	case TUT_PACK:             sprintf_s(buf, size, "Pack your bag at the door."); break;
	case TUT_LEAVE:            sprintf_s(buf, size, "Leave the village. Follow the road south."); break;
	default:                   sprintf_s(buf, size, "You left Mulangae Village."); break;
	}
}

void Game::VillageHelp(char* buf, size_t size) const
{
	// Keys appear as the day teaches them.
	sprintf_s(buf, size, "WASD move   SPACE roll   E interact%s%s   T time   F2 skip   ESC quit",
			  tutorial >= TUT_FORAGE ? "   I bag   F eat" : "",
			  torchOwned ? "   L torch   B journal" : "");
}

void Game::DrawVillageHud()
{
	const int w = renderer->GetWidth();

	char buf[128];
	ObjectiveText(buf, sizeof(buf));
	DrawObjective(buf);

	sprintf_s(buf, sizeof(buf), "Dream fragments  %d / %d", fragments, fragmentGoal);
	int tw = renderer->TextWidth(buf, false);
	renderer->DrawRectPx((float)(w - tw - 46), 18.0f, (float)(tw + 28), 38.0f, kHudPanel, 0.38f);
	renderer->DrawTexts(w - tw - 32, 42, buf, fragments >= fragmentGoal ? kHudAccent : kHudInk, false);

	DrawSurvivalHud();

	char help[160];
	VillageHelp(help, sizeof(help));
	DrawCommonHud(help);

	DrawDialog();
	DrawLetterPanel();
	DrawPackPanel();
	DrawBagPanel();
	DrawJournalPanel();
	if (day == 1) DrawTitleCard("MULANGAE VILLAGE", "the morning the spores arrived");
	DrawVillageFades();
}

void Game::DrawDialog()
{
	if (!DialogOpen()) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	float pw = 900.0f, ph = 96.0f;
	float px = (float)w * 0.5f - pw * 0.5f;
	float py = (float)h - 290.0f;
	renderer->DrawRectPx(px, py, pw, ph, Vec3(0.08f, 0.09f, 0.08f), 0.88f);
	renderer->DrawRectPx(px, py, 4.0f, ph, Vec3(0.85f, 0.70f, 0.45f), 0.9f);

	renderer->DrawTexts((int)px + 24, (int)py + 28, "GRANDMOTHER", Vec3(0.92f, 0.80f, 0.58f), false);
	renderer->DrawTexts((int)px + 24, (int)py + 58, dialogLines[dialogIndex].c_str(), kHudInk, false);

	char buf[16];
	sprintf_s(buf, sizeof(buf), "%d/%d", (int)dialogIndex + 1, (int)dialogLines.size());
	renderer->DrawTexts((int)(px + pw) - 60, (int)py + 28, buf, kHudDim, false);
}

void Game::DrawLetterPanel()
{
	if (!letterOpen) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	renderer->DrawFade(Vec3(0.02f, 0.03f, 0.04f), 0.55f);

	float pw = 620.0f, ph = 350.0f;
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
	renderer->DrawTexts((int)px + 40, y + 40, "P.S. There's rice in the pot. Eat before ye go, mind.", Vec3(0.78f, 0.74f, 0.62f), false);
	renderer->DrawTexts((int)px + 40, (int)(py + ph) - 18, "[E]  Close", kHudAccent, false);
}

void Game::DrawPackPanel()
{
	if (!packOpen) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	renderer->DrawFade(Vec3(0.02f, 0.03f, 0.04f), 0.55f);

	float pw = 600.0f, ph = 340.0f;
	float px = (float)w * 0.5f - pw * 0.5f;
	float py = (float)h * 0.5f - ph * 0.5f;
	renderer->DrawRectPx(px, py, pw, ph, Vec3(0.10f, 0.11f, 0.10f), 0.94f);
	renderer->DrawRectPx(px, py, pw, 3.0f, Vec3(0.45f, 0.60f, 0.50f), 0.8f);

	int used = packTorch ? 1 : 0;
	for (int r = 0; r < kPackItemRows; ++r) used += packTake[kPackRows[r]];

	int x = (int)px + 36;
	int y = (int)py + 44;
	char buf[128];

	renderer->DrawTexts(x, y, "PACK YOUR BAG", kHudInk, true);
	sprintf_s(buf, sizeof(buf), "%d / %d slots", used, kBagCapacity);
	renderer->DrawTexts(x + 380, y, buf, used >= kBagCapacity ? kHudAccent : kHudDim, false);
	y += 42;

	int rows = kPackItemRows + (torchOwned ? 1 : 0);
	for (int r = 0; r < rows; ++r)
	{
		bool selected = (r == packCursor);
		if (selected)
			renderer->DrawRectPx((float)x - 14.0f, (float)y - 18.0f, pw - 44.0f, 26.0f, Vec3(0.25f, 0.40f, 0.34f), 0.45f);

		if (r < kPackItemRows)
		{
			int type = kPackRows[r];
			sprintf_s(buf, sizeof(buf), "%s %-14s take %d of %d", selected ? ">" : " ", GetItemInfo(type).name, packTake[type], packHave[type]);
		}
		else
		{
			sprintf_s(buf, sizeof(buf), "%s %-14s %s", selected ? ">" : " ", "Hand torch", packTorch ? "take it" : "leave it");
		}
		renderer->DrawTexts(x, y, buf, selected ? kHudInk : kHudDim, false);
		y += 30;
	}

	renderer->DrawTexts(x, (int)(py + ph) - 48, "What you leave stays in the house with her.", kHudDim, false);
	renderer->DrawTexts(x, (int)(py + ph) - 22, "W/S select    D take    A leave    E done", kHudAccent, false);
}

void Game::DrawVillageFades()
{
	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	if (sleepTimer >= 0.0f)
	{
		float a = sleepTimer < kSleepFade ? sleepTimer / kSleepFade : 2.0f - sleepTimer / kSleepFade;
		renderer->DrawFade(Vec3(0.01f, 0.01f, 0.02f), Saturatef(a));
	}

	if (dayCardTimer >= 0.0f)
	{
		float a = Saturatef(Minf(dayCardTimer - 1.0f, 6.0f - dayCardTimer));
		const char* t1 = "THE NEXT MORNING";
		int w1 = renderer->TextWidth(t1, true);
		renderer->DrawTexts(w / 2 - w1 / 2, h / 2 - 60, t1, Vec3(0.92f, 0.95f, 0.92f) * a, true);
	}

	if (transitionTimer >= 0.0f)
		renderer->DrawFade(Vec3(0.62f, 0.72f, 0.66f), Saturatef(transitionTimer / kLeaveFade));
}
