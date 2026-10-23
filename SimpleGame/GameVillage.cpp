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
	// The spores have already reached her: she yawns, loses her words, and hides the stain on her hand.
	const char* kWakeLines[] =
	{
		"율아, 벌써 일어났냐? 밥은 먹었고? ...안 먹었지. 아이고, 뼈만 남았네.",
		"(할머니가 하품을 삼키며 소매를 끌어 손등을 덮는다. 푸른 얼룩이 언뜻 보인다.)",
		"우물에서 물 좀 떠 오너라, 우리 강아지. 그다음에 밥 먹이자.",
	};
	const char* kWellHintLines[] =
	{
		"우물은 저기 길가에 있다. 너무 들여다보진 말고.",
	};
	const char* kWaterLines[] =
	{
		"아이고 착하다. 너도 좀 마셔라, 어서. 얼굴이... 하암... 핼쑥하다.",
		"자, 이제 텃밭에서 무 몇 개 뽑고, 뒷산에서 열매 좀 따 오너라.",
		"빨간 것만이다! 허연 건 먹으면 배가 뒤틀린다.",
	};
	const char* kForageHintLines[] =
	{
		"울타리 옆 텃밭에서 무 세 개, 그리고 나무 너머 북쪽 뒷산에서 빨간 열매다.",
	};
	const char* kFoodLines[] =
	{
		"많이도 따 왔네. 이제 좀 먹어라, 할미가 보고 있다.",
	};
	const char* kEatHintLines[] =
	{
		"어서 뭐라도 먹어라. 비쩍 마르는 꼴은 못 본다.",
	};
	const char* kAteLines[] =
	{
		"봐라, 벌써 혈색이 돈다.",
		"하나만 더. 헛간에... 뭐였더라. 아, 개화 전에 쓰던 손전등이 있다.",
		"좀 가져오너라. 요즘 밤이 길다. 헛간은 동쪽, 김씨네 집 지나서다.",
	};
	const char* kTorchHintLines[] =
	{
		"헛간 말이다. 동쪽, 김씨네 지나서. 아마 헌 천막 밑에 있을 거다.",
	};
	const char* kTorchLines[] =
	{
		"그래, 그거다. 너희 할아버지 거다. 배터리 아껴 써라, 더는 없다.",
		"벌써 해가 진다. 할미는 요새 자꾸 눈이 감기는구나...",
		"오늘 밤은 집 가까이 있다가 어두워지기 전에 들어와라.",
	};
	const char* kDuskHintLines[] =
	{
		"멀리 가지 마라, 어두워진다. 오늘 밥은 넉넉히 먹었냐?",
		"우물이 말라서 할미가 새벽마다 저수지까지 다녔더니... 몸이 무겁구나. 걱정 마라.",
	};
	const char* kDeerLines[] =
	{
		"쉿... 저기 물가 봐라. 등불 사슴이다.",
		"오늘 밤엔 따라가지 마라.",
	};
	const char* kBedLines[] =
	{
		"배는 불렀냐? ...그럼 자거라. 할미도 눈 좀... 붙여야겠다.",
		"저 불빛들, 아침이면 없어질 거다.",
	};

	// The neighbours who lived nearer the water and fell asleep first. Their four dreams are
	// one day seen from four sides: the morning Grandfather took the Route 32 bus.
	struct Neighbor
	{
		const char* name;
		const char* note;
		const char* dream[2];
	};

	const Neighbor kNeighbors[4] =
	{
		{ "김씨 아저씨", "경운기 열쇠를 쥔 채 잠들었다.",
		  { "정류장 앞, 젊은 할아버지가 손을 흔든다. \"금방 다녀오마.\"",
			"경운기 소리에 대답이 묻힌다. 그날 버스는 사람으로 가득했다." } },
		{ "박씨 할머니", "빨래 바구니를 끌어안고 잠들었다.",
		  { "빨랫줄 너머, 젊은 순임이 버스표 한 장을 접었다 폈다 한다.",
			"끝내 따라 타지 않는다. 마을에 남을 사람이 있었으니까." } },
		{ "정미 아주머니", "라디오를 귀에 댄 채 잠들었다.",
		  { "라디오가 말한다. 도시로 가는 32번 국도 버스는 오늘이 마지막이라고.",
			"그 뒤로 라디오는 잡음뿐이었다. 순임은 그 잡음을 밤마다 들었다." } },
		{ "동수", "강아지 목줄을 손목에 감은 채 잠들었다.",
		  { "흙먼지를 일으키며 버스가 남쪽으로 멀어진다.",
			"창가의 할아버지가 손전등을 깜빡여 보인다. 한 번, 두 번, 그리고 길 끝." } },
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

	// One rail per span, so a weathered plank is never stretched along the whole fence.
	float span = len / (float)posts;
	for (int i = 0; i < posts; ++i)
	{
		Vec3 mid = LerpV(from, to, ((float)i + 0.5f) / (float)posts);
		AddProp(MODEL_FENCE_RAIL, Vec3(mid.x, 0.62f, mid.z), Vec3(0.08f, 0.12f, span), atan2f(d.x, d.z), 0.0f, 0.0f);
	}
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

	// Everyday things of a country yard: the jars by grandmother's house, the bench where neighbours sat,
	// a tiller left by the shed, a gate with no wall left to keep, and the bare hoops of a greenhouse.
	AddProp(MODEL_JANGDOK, Vec3(3.4f, 0.0f, -11.4f), kUnitScale, 0.0f, 0.9f, 0.6f);
	AddProp(MODEL_PYEONGSANG, Vec3(4.3f, 0.0f, -6.9f), kUnitScale, 0.2f, 0.9f, 0.7f);
	AddProp(MODEL_TILLER, Vec3(10.4f, 0.0f, -12.2f), kUnitScale, 0.6f, 0.6f, 0.9f);
	AddProp(MODEL_GATE, Vec3(9.5f, 0.0f, -2.4f), kUnitScale, 0.12f, 0.0f, 0.0f);
	for (int side = -1; side <= 1; side += 2)
	{
		// Only the wall stubs block; the open leaf lets you through.
		Actor* stub = levelNode->AddChild(new Actor(ACTOR_NODE));
		stub->SetPosition(Vec3(9.5f + 1.45f * (float)side, 0.0f, -2.4f - 0.17f * (float)side));
		stub->collider.shape = COLLIDER_BOX;
		stub->collider.halfX = 0.4f;
		stub->collider.halfZ = 0.2f;
	}
	AddProp(MODEL_GREENHOUSE, Vec3(17.6f, 0.0f, -8.2f), kUnitScale, 0.0f, 0.0f, 0.0f);
	AddProp(MODEL_JANGDOK, Vec3(-10.8f, 0.0f, 19.4f), Vec3(0.85f, 0.85f, 0.85f), 1.1f, 0.8f, 0.6f);

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
		AddSleeper(Vec3(others[i][0], 0.0f, others[i][1]), others[i][2], false, 1.3f * (float)(i + 1))->neighbor = i;

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
		float rate = 0.0196f * NatureGuard(natureInsight);
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
			ShowMessage("어두워진다. 할머니가 집으로 부르신다.", 3.0f);
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
		if (!deerWatched) ShowMessage("사슴이 갈대숲으로 사라진다.", 3.0f);
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
		ShowMessage("WASD로 우물까지 걸어가서, 옆에서 E를 눌러라.", 5.0f);
		break;

	case TUT_FORAGE:
		ShowMessage("텃밭은 집 서쪽 울타리 안에 있다. 뒷산은 북쪽이다.", 5.0f);
		break;

	case TUT_EAT:
		mealsBaseline = mealsEaten;
		ShowMessage("F를 눌러 먹어라.", 4.0f);
		break;

	case TUT_DUSK:
		timeScale = 12.0f;
		ShowMessage("해가 빠르게 진다. 어두워지기 전에 집으로 가라.", 4.0f);
		break;

	case TUT_BEDTIME:
		ShowMessage("잠자리에 들어라: 문 앞에서 E.", 4.0f);
		break;

	case TUT_FIND_GRANDMOTHER:
		ShowMessage("오늘 아침엔 할머니가 나오지 않으셨다.", 4.5f);
		break;

	case TUT_READ_LETTER:
		letter->visible = true;
		break;

	case TUT_PACK:
		inventory[ITEM_HERB] += 2;
		ShowMessage("편지 곁의 지도를 챙겼다(M으로 펼치기). 선반의 약초 두 묶음도 챙겨, 문 앞에서 배낭을 꾸려라.", 6.0f);
		break;

	case TUT_LEAVE:
		ShowMessage("길을 따라 남쪽으로, 마을 밖으로 나가라.", 5.0f);
		break;

	default:
		break;
	}
}

void Game::Say(const char* const* lines, int count, int next, const char* speaker)
{
	dialogSpeaker = speaker;
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
	PlayPlayerAction("pickup");

	switch (f.forage)
	{
	case FORAGE_RADISH:
		f.picked = true;
		f.Destroy();
		++radishes;
		++inventory[ITEM_VEGETABLE];
		ShowMessage("무를 뽑아 흙을 턴다.  (I: 배낭)", 3.0f);
		break;

	case FORAGE_RED_BERRY:
		f.picked = true;
		f.model = MODEL_BUSH;
		++redBerries;
		++inventory[ITEM_BERRIES];
		if (redBerries == 1)
			AddInsight(1, "허연 것 말고 빨간 것: 이제 어떤 열매가 안전한지 안다.");
		else
			ShowMessage("빨간 열매를 한 줌 딴다.", 2.5f);
		break;

	default:
		ShowMessage("할머니가 허연 건 배가 뒤틀린다고 했다. 그냥 둔다.", 3.5f);
		break;
	}

	if (tutorial == TUT_FORAGE && radishes >= kForageGoal && redBerries >= kForageGoal)
	{
		SetTutorial(TUT_BRING_FOOD);
		ShowMessage("이만하면 충분하다. 할머니께 가져가자.", 4.0f);
	}
}

void Game::WakeNextMorning()
{
	day = 2;
	timeOfDay = 0.27f;
	AdvanceDay();
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
		prompt = "[E]  계속";
		return;
	}
	if (letterOpen)
	{
		targetSpot = SPOT_LETTER_OPEN;
		prompt = "[E]  닫기";
		return;
	}

	const Vec3& pos = player->Position();

	if (day == 1 && DistXZ(pos, grandmaNpc->Position()) < kInteractRange)
	{
		targetSpot = SPOT_GRANDMA;
		prompt = "[E]  할머니와 이야기하기";
		return;
	}

	// The letter wins over the sleeper it lies beside.
	if (tutorial == TUT_READ_LETTER && DistXZ(pos, letter->Position()) < kInteractRange)
	{
		targetSpot = SPOT_LETTER;
		prompt = "[E]  편지 읽기";
		return;
	}

	if (DistXZ(pos, homeDoor) < kDoorRange && (tutorial == TUT_BEDTIME || tutorial == TUT_PACK))
	{
		targetSpot = SPOT_DOOR;
		if (tutorial == TUT_BEDTIME) prompt = "[E]  잠자리에 들기";
		else prompt = riceEaten ? "[E]  배낭 꾸리기" : "[E]  할머니가 남긴 밥 먹기";
		return;
	}

	if (tutorial >= TUT_FETCH_WATER && DistXZ(pos, wellPos) < 2.2f)
	{
		targetSpot = SPOT_WELL;
		prompt = (tutorial == TUT_FETCH_WATER) ? "[E]  물 긷기" : "[E]  우물물 마시기";
		return;
	}

	if (tutorial == TUT_FETCH_TORCH && DistXZ(pos, shedDoor) < 2.2f)
	{
		targetSpot = SPOT_SHED;
		prompt = "[E]  헛간 뒤지기";
		return;
	}

	if (deer && tutorial == TUT_WATCH_DEER && !deerWatched && DistXZ(pos, deer->Position()) < kDeerWatchRange)
	{
		targetSpot = SPOT_DEER;
		prompt = "[E]  등불 사슴 지켜보기";
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
			if (targetForage->forage == FORAGE_RADISH) prompt = "[E]  무 뽑기";
			else if (targetForage->forage == FORAGE_RED_BERRY) prompt = "[E]  빨간 열매 따기";
			else prompt = "[E]  허연 열매 따기";
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
			prompt = (tutorial == TUT_FIND_GRANDMOTHER) ? "[E]  할머니 살펴보기" : "[E]  곁에 앉기";
		else if (!targetSleeper->visited && targetSleeper->neighbor >= 0)
			prompt = std::string("[E]  곁에서 쉬기 - ") + kNeighbors[targetSleeper->neighbor].name;

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
			ShowMessage("솥의 밥이 아직 따뜻하다. 한 톨도 남김없이 먹는다.", 4.0f);
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
			ShowMessage("찬물을 길어 실컷 마시고 물병 두 개를 채운다.  이제 아래에 물과 음식이 표시된다.", 6.0f);
			SetTutorial(TUT_BRING_WATER);
		}
		else
		{
			ShowMessage("차갑고 맑은 물.", 2.0f);
		}
		break;

	case SPOT_SHED:
		torchOwned = true;
		FindRelic(RELIC_TORCH);
		ShowMessage("천막 밑에서 옛 세상의 손전등을 찾았다. L로 켠다.  B로 유물 도감을 연다.", 6.5f);
		SetTutorial(TUT_BRING_TORCH);
		break;

	case SPOT_DEER:
		deerWatched = true;
		AddInsight(1, "뿔의 불빛이 느린 숨처럼 맥박친다.");
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
				ShowMessage("숨은 쉬고 계신다. 손끝에 이끼가 돋기 시작했다. 곁에 편지와 지도가 놓여 있다.", 6.5f);
				SetTutorial(TUT_READ_LETTER);
			}
			else
			{
				ShowMessage("숨은 쉬고 계신다. 깨어나지 않으신다.", 3.5f);
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
			AddInsight(1, "꿈 조각을 얻었다.");

			// The sleeper's memory, seen for a moment; the last one joins the four into one day.
			const Neighbor& n = kNeighbors[s.neighbor >= 0 ? s.neighbor : 0];
			char noteLine[256];
			sprintf_s(noteLine, sizeof(noteLine), "(%s. %s)", n.name, n.note);
			const char* lines[4] = { noteLine, n.dream[0], n.dream[1],
									 "(네 꿈이 하나로 이어진다. 할아버지가 32번 국도 버스를 타고 떠나던 날.)" };
			char speaker[64];
			sprintf_s(speaker, sizeof(speaker), "꿈 조각 · %s", n.name);
			Say(lines, fragments >= fragmentGoal ? 4 : 3, -1, speaker);
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
	ShowMessage("배낭을 멘다. 두고 가는 것은 할머니 곁에 남는다.", 4.0f);
	SetTutorial(TUT_LEAVE);
}

void Game::ObjectiveText(char* buf, size_t size) const
{
	switch (tutorial)
	{
	case TUT_WAKE:             sprintf_s(buf, size, "할머니와 이야기하라."); break;
	case TUT_FETCH_WATER:      sprintf_s(buf, size, "우물에서 물을 길어라."); break;
	case TUT_BRING_WATER:      sprintf_s(buf, size, "할머니께 물을 가져가라."); break;
	case TUT_FORAGE:
		sprintf_s(buf, size, "무 뽑기 (%d/%d), 빨간 열매 따기 (%d/%d).",
				  radishes < kForageGoal ? radishes : kForageGoal, kForageGoal,
				  redBerries < kForageGoal ? redBerries : kForageGoal, kForageGoal);
		break;
	case TUT_BRING_FOOD:       sprintf_s(buf, size, "할머니께 먹을 것을 가져가라."); break;
	case TUT_EAT:              sprintf_s(buf, size, "할머니 앞에서 뭐라도 먹어라. (F)"); break;
	case TUT_FETCH_TORCH:      sprintf_s(buf, size, "마을 동쪽 헛간에서 옛 손전등을 찾아라."); break;
	case TUT_BRING_TORCH:      sprintf_s(buf, size, "할머니께 손전등을 가져가라. (L로 켠다)"); break;
	case TUT_DUSK:             sprintf_s(buf, size, "저녁이 내린다. 집으로 가라."); break;
	case TUT_WATCH_DEER:       sprintf_s(buf, size, "물가의 등불 사슴을 지켜보라."); break;
	case TUT_BEDTIME:          sprintf_s(buf, size, "잠자리에 들어라. (문 앞에서 E)"); break;
	case TUT_FIND_GRANDMOTHER: sprintf_s(buf, size, "할머니를 찾아라."); break;
	case TUT_READ_LETTER:      sprintf_s(buf, size, "할머니 곁의 편지를 읽어라."); break;
	case TUT_PACK:             sprintf_s(buf, size, "문 앞에서 배낭을 꾸려라."); break;
	case TUT_LEAVE:            sprintf_s(buf, size, "마을을 떠나라. 길을 따라 남쪽으로."); break;
	default:                   sprintf_s(buf, size, "물안개 마을을 떠났다."); break;
	}
}

void Game::VillageHelp(char* buf, size_t size) const
{
	// Keys appear as the day teaches them.
	sprintf_s(buf, size, "WASD 이동   SPACE 구르기   E 상호작용%s%s%s   T 시간   F2 건너뛰기   F4 옵션   ESC 종료",
			  tutorial >= TUT_FORAGE ? "   I 배낭   F 먹기" : "",
			  torchOwned ? "   L 손전등   B 도감" : "",
			  tutorial >= TUT_PACK ? "   M 지도" : "");
}

void Game::DrawVillageHud()
{
	const int w = renderer->GetWidth();

	char buf[256];
	ObjectiveText(buf, sizeof(buf));
	DrawObjective(buf);

	sprintf_s(buf, sizeof(buf), "꿈 조각  %d / %d", fragments, fragmentGoal);
	int tw = renderer->TextWidth(buf, false);
	renderer->DrawRectPx((float)(w - tw - 46), 18.0f, (float)(tw + 28), 38.0f, kHudPanel, 0.38f);
	renderer->DrawTexts(w - tw - 32, 42, buf, fragments >= fragmentGoal ? kHudAccent : kHudInk, false);

	DrawSurvivalHud();

	char help[256];
	VillageHelp(help, sizeof(help));
	DrawCommonHud(help);

	DrawDialog();
	DrawLetterPanel();
	DrawPackPanel();
	DrawBagPanel();
	DrawJournalPanel();
	DrawMapPanel();
	if (day == 1) DrawTitleCard("물안개 마을", "포자가 찾아온 아침");
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

	// Beside her map, a note keeps to the rest of the screen.
	float mx, my, mw, mh;
	if (dialogSpeaker == "할머니의 지도 메모" && MemoMapRect(mx, my, mw, mh))
	{
		float from = memoMapLeft ? mx + mw + 24.0f : 24.0f;
		float to = memoMapLeft ? (float)w - 24.0f : mx - 24.0f;
		pw = Minf(pw, to - from);
		px = (from + to) * 0.5f - pw * 0.5f;
	}
	renderer->DrawRectPx(px, py, pw, ph, Vec3(0.08f, 0.09f, 0.08f), 0.88f);
	renderer->DrawRectPx(px, py, 4.0f, ph, Vec3(0.85f, 0.70f, 0.45f), 0.9f);

	renderer->DrawTexts((int)px + 24, (int)py + 28, dialogSpeaker.c_str(), Vec3(0.92f, 0.80f, 0.58f), false);
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
		"포자가 여기까지 왔으니, 마을은 곧 잠들 거다.",
		"도망치라는 말이 아니다.",
		"",
		"도시 한가운데에 나무가 하나 있단다.",
		"거기 사람들은 잠들지 않는다더구나. 깨어난다고.",
		"",
		"네 눈으로 직접 가서 보거라.",
		"그리고 어디서 살지는, 네가 정하거라.",
	};

	int y = (int)py + 52;
	for (int i = 0; i < 8; ++i)
	{
		renderer->DrawTexts((int)px + 40, y, lines[i], Vec3(0.88f, 0.90f, 0.86f), false);
		y += 26;
	}
	renderer->DrawTexts((int)(px + pw) - 130, y + 12, "- 순임", Vec3(0.70f, 0.76f, 0.72f), false);
	renderer->DrawTexts((int)px + 40, y + 40, "추신. 솥에 밥 있다. 꼭 먹고 가거라.", Vec3(0.78f, 0.74f, 0.62f), false);
	renderer->DrawTexts((int)px + 40, (int)(py + ph) - 18, "[E]  닫기", kHudAccent, false);
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

	renderer->DrawTexts(x, y, "배낭 꾸리기", kHudInk, true);
	sprintf_s(buf, sizeof(buf), "%d / %d칸", used, kBagCapacity);
	renderer->DrawTexts(x + 380, y, buf, used >= kBagCapacity ? kHudAccent : kHudDim, false);
	y += 42;

	int rows = kPackItemRows + (torchOwned ? 1 : 0);
	for (int r = 0; r < rows; ++r)
	{
		bool selected = (r == packCursor);
		if (selected)
			renderer->DrawRectPx((float)x - 14.0f, (float)y - 18.0f, pw - 44.0f, 26.0f, Vec3(0.25f, 0.40f, 0.34f), 0.45f);

		const char* name = "손전등";
		if (r < kPackItemRows)
		{
			int type = kPackRows[r];
			name = GetItemInfo(type).name;
			sprintf_s(buf, sizeof(buf), "%d개 중 %d개 챙김", packHave[type], packTake[type]);
		}
		else
		{
			sprintf_s(buf, sizeof(buf), "%s", packTorch ? "챙김" : "두고 감");
		}

		Vec3 color = selected ? kHudInk : kHudDim;
		if (selected) renderer->DrawTexts(x, y, ">", color, false);
		renderer->DrawTexts(x + 16, y, name, color, false);
		renderer->DrawTexts(x + 180, y, buf, color, false);
		y += 30;
	}

	renderer->DrawTexts(x, (int)(py + ph) - 48, "두고 가는 것은 할머니와 함께 집에 남는다.", kHudDim, false);
	renderer->DrawTexts(x, (int)(py + ph) - 22, "W/S 선택    D 넣기    A 빼기    E 완료", kHudAccent, false);
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
		const char* t1 = "다음 날 아침";
		int w1 = renderer->TextWidth(t1, true);
		renderer->DrawTexts(w / 2 - w1 / 2, h / 2 - 60, t1, Vec3(0.92f, 0.95f, 0.92f) * a, true);
	}

	if (transitionTimer >= 0.0f)
		renderer->DrawFade(Vec3(0.62f, 0.72f, 0.66f), Saturatef(transitionTimer / kLeaveFade));
}
