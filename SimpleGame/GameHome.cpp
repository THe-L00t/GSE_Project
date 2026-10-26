#include "stdafx.h"
#include "Game.h"

#include <cstdio>

#include "Models.h"

// Past Ginkgo Town (ContentDesign.md 6), all on the one growing map: the mother's words on the school wall, the
// pylon ridge from where the great tree shows, the sleeping city round it where the spores are thickest, and
// under the tree the clear air and the home the parents began. Finishing that home and filling its jars is the
// end of the story; the world goes on after it.

namespace
{
	const int   kLateWall = HOME_SPOT_COUNT;
	const int   kLatePylon = HOME_SPOT_COUNT + 1;
	const int   kLateNeighbor = HOME_SPOT_COUNT + 2;   // and the one after it: Dohyeon, Yeoul
	const float kLateReach = 2.2f;      // from the spot one stands at to use it
	const float kSettleCard = 8.0f;     // how long the closing title shows

	// What each part of the home takes.
	const int kHomeScrap[kHomeProjects] = { 6, 4, 2, 3 };
	const int kHomeSeeds[kHomeProjects] = { 0, 0, 3, 0 };

	const float kGardenGrow = 45.0f;    // game seconds for each radish to come up
	const int   kGardenRows = 6;
	const float kWellFill = 30.0f;      // game seconds for a flask's worth to gather in the jars
	const int   kWellHold = 20;

	// Food the bag keeps for the road when the rest goes into the jars, best first.
	const int kFoodKeep[] = { ITEM_COOKED_MEAT, ITEM_CANNED_FOOD, ITEM_VEGETABLE, ITEM_BERRIES, ITEM_MEAT };
	const int kFoodKeepCount = (int)(sizeof(kFoodKeep) / sizeof(kFoodKeep[0]));

	const Vec3 kFront(0.70710678f, 0.0f, 0.70710678f);   // toward the camera

	const char* kBuildNeeds[kHomeProjects] = { "쉼터를 고치려면", "우물을 다시 파려면", "밭을 일구려면", "석등을 세우려면" };

	const char* kBuiltWords[kHomeProjects] =
	{
		"무너진 벽을 세우고 지붕을 얹었다. 비가 새지 않는 집이 됐다.",
		"메워진 우물을 다시 팠다. 나무뿌리 사이로 맑은 물이 고인다.",
		"풀을 걷고 밭을 일궈 씨를 뿌렸다. 곧 무가 올라올 것이다.",
		"쓰러진 석등을 세우고 불을 붙였다. 집 앞이 따뜻해졌다.",
	};

	const char* kWallLines[] =
	{
		"(분필 글씨가 비에 씻겨 희미하다. 엄마 글씨다.)",
		"「어머님, 약은 도시 쪽에 있대요. 남쪽 송전탑에 오르면 큰 나무가 보인다고 해요.」",
		"「그 나무 밑은 숨쉬기가 편하대요. 자리 잡으면 하율이 데리러 갈게요.」",
	};

	const char* kCityWords[] =
	{
		"공기가 무겁다. 포자가 모두 한쪽으로, 나무 쪽으로 흘러간다.",
		"길가마다 잠든 사람들. 도시가 통째로 잠들어 있다. 물을 아껴 마셔야 한다.",
	};

	const char* kParentsLines[] =
	{
		"(손을 맞잡고 나란히 잠든 두 사람.)",
		"(아빠 손목엔 아직 가는 시계, 엄마 곁엔 금 간 전화기.)",
		"엄마. 아빠. 나무까지, 거의 다 왔었구나.",
	};

	const char* kRootLines[] =
	{
		"숨이 가볍다. 포자가 모두 나무 속으로 빨려 들어가고, 그늘 아래만 공기가 맑다.",
		"할머니 편지가 맞았다. 여기선 졸리지 않는다.",
	};

	const char* kHomeLines[] =
	{
		"나무 밑에 누가 살던 자리가 있다. 짓다 만 집, 메워진 우물, 풀에 묻힌 밭.",
		"장독 뚜껑에 엄마 글씨가 있다. 「하율이 오면」.",
		"여기서 살자. 엄마 아빠가 시작한 곳에서.",
	};

	const char* kSettleLines[] =
	{
		"장독이 그득하다. 우물엔 물이 고이고, 밭엔 무가 줄지어 났다.",
		"엄마 아빠가 짓다 만 집을, 내가 마저 지었다.",
	};

	const char* kNightLines[] =
	{
		"(그날 밤, 등불 사슴들이 나무 밑으로 모여들었다.)",
		"(뿔마다 실린 불빛이 하나씩 가지 끝으로 올라가 꽃이 된다. 유난히 따뜻한 빛이 하나 있다.)",
		"(도현 아저씨와 여울이도 나와, 나란히 나무를 올려다본다.)",
		"할머니, 여기서 살기로 했어요. 밥은 잘 먹고 있어요.",
	};

	// The two who came to the tree first (GameConcept.md 2026-10-26, the models): where they stand, out from the
	// trunk toward the camera (front) and across that (side), as the parts of the home are.
	const float kNeighborFront[2] = { 11.0f, 18.0f };
	const float kNeighborSide[2] = { -14.0f, 15.0f };

	const char* kNeighborSeenLines[] =
	{
		"(나무 밑에 사람이 있다. 할머니 말고는 처음 보는, 깨어 있는 사람이.)",
	};

	// The dialog box draws each line as one row, so none is longer than the widest the village already has.
	const char* kDohyeonMeet[] =
	{
		"(안경을 밀어 올리고 한참을 본다.) ...깨어 있는 사람이 또 있었군.",
		"도현이다. 은행나무 읍내에 살았지. 아내가 저 도시 어딘가에 잠들어 있다.",
		"깨어나면 제일 먼저 보이는 데 있고 싶어서, 여기 와 있다.",
		"그동안 옛 세상 물건을 모은다. 시계, 사진, 차표 한 장까지.",
		"아내가 깨어났을 때 돌아올 세상이 남아 있어야 하니까.",
		"저 집 말이다. 젊은 내외가 짓다가, 아이를 데리러 간다며 떠났지. 돌아오지 않더구나.",
	};
	// Hayul's answer is told, not given its own dialog: under Hayul's name the screen turns to an old photograph.
	const char* kHayulAnswer[] =
	{
		"(하율이 겨우 말한다. ...우리 엄마, 아빠예요.)",
		"(도시 가장자리에서, 손을 잡고 잠들어 있었어요.)",
	};
	const char* kDohyeonMeetEnd[] =
	{
		"(도현이 한동안 말이 없다.) ...그랬구나. 그럼 저 집은 네 몫이다.",
		"모자란 건 잠든 도시에 있다. 건물 사이에도, 허물이 지닌 것 중에도.",
		"옛 사람들 물건이지만, 이제 쓸 사람이 써야지.",
	};
	const char* kDohyeonMore[] =
	{
		"여울이는 포자 속에서 뛰어논다. 나는 그게 무섭다. 돌아갈 데가 없다는 뜻 같아서.",
		"(도현이 수첩을 펼쳐 무언가를 적는다.) 오늘 바람, 오늘 하늘. 이것도 기록이지.",
	};

	const char* kYeoulMeet[] =
	{
		"(화관을 쓴 아이가 포자 사이를 뛰어와 하율을 빤히 올려다본다.)",
		"처음 보는 얼굴! 너도 안 졸려?",
		"나는 여울이야. 엄마 아빠는 내가 태어날 때부터 자고 있었어.",
		"그래서 난 포자가 하나도 안 졸려.",
		"(손바닥에 내려앉은 포자를 후 불어 날린다.)",
		"얘네는 나무한테 가는 길이야. 길만 안 막으면 괜찮아.",
	};
	// What she knows of living in the spores, each of it true of the world: water, rain, a held breath, a lantern.
	const char* kYeoulTips[] =
	{
		"도시에서 졸리면 물을 마셔. 포자가 씻겨 내려가.",
		"비 오는 날엔 포자가 덜 날려. 멀리 갈 거면 그런 날 가.",
		"구를 땐 숨 참는 거야! 그동안엔 포자가 안 들어와.",
		"석등 불빛 안은 나무 밑처럼 맑아. 짐승도 못 들어오고.",
	};

	template <int N>
	int LineCount(const char* (&)[N])
	{
		return N;
	}

	StoryBeat MakeBeat(const char* speaker, const char* const* lines, int count)
	{
		StoryBeat beat;
		beat.speaker = speaker;
		beat.lines.assign(lines, lines + count);
		return beat;
	}

	bool IsNight(float timeOfDay)
	{
		return timeOfDay < 0.22f || timeOfDay > 0.78f;
	}
}

void Game::BuildHome()
{
	// The parts of the home are the level's own props, so they can change as they are built. Their fronts face
	// the camera, on the side of the trunk it looks from.
	const StorySites& s = routeMap.Sites();
	const float yaw = kPi * 0.25f;

	homeProps[HOME_SHELTER] = AddProp(MODEL_EMPTY_HOUSE, HomeSpotPos(s, HOME_SHELTER), Vec3(0.75f, 0.75f, 0.75f), yaw, 2.6f, 2.1f);
	homeProps[HOME_WELL] = AddProp(MODEL_RUIN, HomeSpotPos(s, HOME_WELL), Vec3(0.55f, 0.55f, 0.55f), yaw, 0.55f, 0.55f);
	homeProps[HOME_GARDEN] = AddProp(MODEL_BUSH, HomeSpotPos(s, HOME_GARDEN), Vec3(2.4f, 0.9f, 1.6f), yaw, 0.0f, 0.0f);
	homeProps[HOME_LANTERN] = AddProp(MODEL_ROCK, HomeSpotPos(s, HOME_LANTERN), Vec3(0.55f, 0.4f, 0.55f), yaw, 0.0f, 0.0f);
	homeProps[HOME_STORE] = AddProp(MODEL_JANGDOK, HomeSpotPos(s, HOME_STORE), Vec3(1.0f, 1.0f, 1.0f), yaw, 0.9f, 0.6f);

	// Radishes in two rows along the bed, shown as they come up.
	Vec3 along(0.70710678f, 0.0f, -0.70710678f);
	for (int i = 0; i < kGardenRows; ++i)
	{
		Vec3 at = HomeSpotPos(s, HOME_GARDEN) + along * (-1.1f + 1.1f * (float)(i % 3)) + kFront * (i < 3 ? -0.45f : 0.45f);
		gardenCrops[i] = AddProp(MODEL_VEGETABLE, at, Vec3(1.0f, 1.0f, 1.0f), (float)i * 1.3f, 0.0f, 0.0f);
	}

	homeLantern = levelNode->AddChild(new LanternActor());
	homeLantern->SetPosition(HomeSpotPos(s, HOME_LANTERN));
	RefreshHomeLooks();
	PlaceNeighbors();
}

void Game::PlaceNeighbors()
{
	// They were under the tree before Hayul came, on either side of the home, half turned toward it.
	const StorySites& s = routeMap.Sites();
	const int models[kNeighborCount] = { MODEL_DOHYEON, MODEL_YEOUL };
	const float turn[kNeighborCount] = { 0.35f, -0.5f };
	const float k = 0.70710678f;
	for (int i = 0; i < kNeighborCount; ++i)
	{
		NpcActor* n = levelNode->AddChild(new NpcActor());
		n->model = models[i];
		n->phase = 1.3f * (float)(i + 1);
		n->collider.radius = i == 0 ? 0.4f : 0.32f;
		n->SetPosition(s.root + Vec3((kNeighborFront[i] + kNeighborSide[i]) * k, 0.0f, (kNeighborFront[i] - kNeighborSide[i]) * k));
		n->SetYaw(kPi * 0.25f + turn[i]);
		neighborNpc[i] = n;
	}
}

const char* Game::NeighborName(int who)
{
	return who == 0 ? "도현" : "여울";
}

void Game::TalkToNeighbor(int who)
{
	NpcActor* n = neighborNpc[who];
	if (n == nullptr) return;
	Vec3 d = player->Position() - n->Position();
	n->SetYaw(atan2f(d.x, d.z));
	neighborsSeen = true;

	if (!neighborMet[who])
	{
		neighborMet[who] = true;
		if (who == 1)
		{
			OpenBeat(MakeBeat("여울", kYeoulMeet, LineCount(kYeoulMeet)));
			return;
		}

		// He saw Hayul's parents begin the home, and leave it to fetch their child.
		StoryBeat meet = MakeBeat("도현", kDohyeonMeet, LineCount(kDohyeonMeet));
		meet.lines.insert(meet.lines.end(), kHayulAnswer, kHayulAnswer + (parentsFound ? 2 : 1));
		meet.lines.insert(meet.lines.end(), kDohyeonMeetEnd, kDohyeonMeetEnd + LineCount(kDohyeonMeetEnd));
		OpenBeat(meet);
		return;
	}

	// After the first time: how the home stands, then one more thing, a different one each time.
	int turnIndex = neighborTalks[who]++;
	StoryBeat beat;
	beat.speaker = NeighborName(who);
	if (who == 0)
	{
		if (settled) beat.lines.push_back("아내가 깨어나면 여기로 데려와야겠다. 이웃이 있다고, 무가 난다고.");
		else if (HomeComplete()) beat.lines.push_back("집이 제법 집 같구나. 이제 장독만 채우면 겨울도 나겠다.");
		else beat.lines.push_back("집은 좀 되어 가냐? 자재는 잠든 도시에 있다. 건물 사이에도, 허물이 지닌 것 중에도.");

		if (turnIndex % 3 == 0)
		{
			int found = 0;
			for (int r = 0; r < kRelicCount; ++r) found += relicFound[r] ? 1 : 0;
			char buf[256];
			if (found >= kRelicCount)
				sprintf_s(buf, sizeof(buf), "옛 물건 여덟 개를 다 모았구나. 도감 한 권에 옛 세상이 다 들었다. ...고맙다.");
			else
				sprintf_s(buf, sizeof(buf), "옛 물건은 %d개 찾았구나. 나머지도 길 어딘가에 있을 거다. 도감(B)에 적어 두렴.", found);
			beat.lines.push_back(buf);
		}
		else
		{
			beat.lines.push_back(kDohyeonMore[turnIndex % 3 - 1]);
		}
	}
	else
	{
		beat.lines.push_back(kYeoulTips[turnIndex % LineCount(kYeoulTips)]);
		if (settled) beat.lines.push_back("이제 여기 우리 동네야. 도현 아저씨랑, 너랑, 나랑.");
		else if (homeBuilt[HOME_GARDEN]) beat.lines.push_back("(밭 쪽을 기웃거린다.) 무 나면 하나만 줘!");
		else beat.lines.push_back("밭 만들면 나도 무 하나 줘! 약속!");
	}
	OpenBeat(beat);
}

void Game::RefreshHomeLooks()
{
	PropActor* shelter = homeProps[HOME_SHELTER];
	if (shelter && homeBuilt[HOME_SHELTER] && shelter->model != MODEL_HOUSE_A)
	{
		shelter->model = MODEL_HOUSE_A;
		shelter->SetScale(Vec3(5.0f, 3.0f, 4.6f));
		shelter->collider.halfX = 2.5f;
		shelter->collider.halfZ = 2.3f;
	}

	PropActor* well = homeProps[HOME_WELL];
	if (well && homeBuilt[HOME_WELL] && well->model != MODEL_WELL)
	{
		well->model = MODEL_WELL;
		well->SetScale(Vec3(1.0f, 1.0f, 1.0f));
		well->collider.halfX = 0.95f;
		well->collider.halfZ = 0.95f;
	}

	PropActor* garden = homeProps[HOME_GARDEN];
	if (garden && homeBuilt[HOME_GARDEN] && garden->model != MODEL_GARDEN_BED)
	{
		garden->model = MODEL_GARDEN_BED;
		garden->SetScale(Vec3(3.4f, 1.0f, 1.6f));
	}

	if (homeProps[HOME_LANTERN]) homeProps[HOME_LANTERN]->visible = !homeBuilt[HOME_LANTERN];
	if (homeLantern) homeLantern->visible = homeBuilt[HOME_LANTERN];
	for (int i = 0; i < kGardenRows; ++i)
	{
		if (gardenCrops[i]) gardenCrops[i]->visible = homeBuilt[HOME_GARDEN] && i < gardenReady;
	}
}

void Game::UpdateHome(float dt)
{
	const StorySites& s = routeMap.Sites();
	const Vec3& pos = player->Position();
	float toRoot = DistXZ(pos, s.root);
	float gameDt = dt * timeScale;

	// The two under the tree are drawn and moved only within sight of it.
	for (int i = 0; i < kNeighborCount; ++i)
	{
		if (neighborNpc[i]) neighborNpc[i]->visible = toRoot < kSanctuaryRadius + 80.0f;
	}

	// The spores drift thicker the nearer the tree, and are gone under it.
	float field = SporeField(pos);
	sporeVisual = Approach(sporeVisual, field < 0.0f ? 0.12f : 0.55f * field, 0.8f, dt);

	// Near it, what spores there are drift in toward the trunk and up into the branches.
	if (toRoot < kSanctuaryRadius + 24.0f)
	{
		treeMoteTimer += dt;
		while (treeMoteTimer > 0.12f)
		{
			treeMoteTimer -= 0.12f;
			float a = fxRng.Range(0.0f, 2.0f * kPi);
			float r = fxRng.Range(6.0f, 26.0f);
			float y = fxRng.Range(0.3f, 2.5f);
			Wisp w;
			w.pos = s.root + Vec3(cosf(a) * r, y, sinf(a) * r);
			w.vel = (s.root + Vec3(0.0f, 9.0f, 0.0f) - w.pos) * 0.14f;
			w.maxLife = fxRng.Range(4.0f, 6.5f);
			w.life = w.maxLife;
			w.tint = Vec3(0.55f, 0.95f, 0.80f);
			w.size = 0.45f;
			w.glow = 0.9f;
			w.drag = 0.0f;
			w.sway = 0.25f;
			w.plain = true;
			wisps.push_back(w);
		}
	}

	if (deathTimer < 0.0f)
	{
		if (!cityEntered && toRoot < (float)kCityChunks * kChunkSize)
		{
			cityEntered = true;
			Narrate("하율", kCityWords, 2);
		}

		if (!parentsFound && DistXZ(pos, s.parents) < 7.0f && PlaceInSight(s.parents, 0.6f))
		{
			parentsFound = true;
			QueueBeat(MakeBeat("하율", kParentsLines, 3));
		}

		if (!rootReached && toRoot < kSanctuaryRadius)
		{
			rootReached = true;
			QueueBeat(MakeBeat("하율", kRootLines, 2));
			StoryBeat home = MakeBeat("하율", kHomeLines, 3);
			home.follows = true;
			storyQueue.push_back(home);

			// From now on the long sleep ends at home.
			safePoint->SetPosition(HomeSpotFront(HOME_SHELTER) + kFront * 0.8f);
			AddInsight(2, "큰 나무는 포자를 들이쉰다. 그 그늘 아래는 맑다.");
		}

		// Once the tree's own words are said, the first sight of someone awake under it.
		if (rootReached && !neighborsSeen && !DialogOpen() && storyQueue.empty())
		{
			for (int i = 0; i < kNeighborCount; ++i)
			{
				NpcActor* n = neighborNpc[i];
				if (n == nullptr || DistXZ(pos, n->Position()) > 18.0f || !PlaceInSight(n->Position(), 1.6f)) continue;
				neighborsSeen = true;
				Narrate("하율", kNeighborSeenLines, 1);
				break;
			}
		}
	}

	// The home keeps going while you are away: radishes come up, and the well fills the jars.
	if (homeBuilt[HOME_GARDEN] && gardenReady < kGardenRows)
	{
		gardenTimer += gameDt;
		if (gardenTimer >= kGardenGrow)
		{
			gardenTimer -= kGardenGrow;
			++gardenReady;
			RefreshHomeLooks();
		}
	}
	if (homeBuilt[HOME_WELL] && store[ITEM_CLEAN_WATER] < kWellHold)
	{
		wellTimer += gameDt;
		if (wellTimer >= kWellFill)
		{
			wellTimer -= kWellFill;
			++store[ITEM_CLEAN_WATER];
		}
	}

	// A home made whole and jars full enough: the end of the story, told at home on the night it comes true.
	if (!settled && HomeComplete() && StoredFood() >= kStockFood && store[ITEM_CLEAN_WATER] >= kStockWater
		&& store[ITEM_HERB] >= kStockHerb && AtHome() && deathTimer < 0.0f)
	{
		settled = true;
		if (!IsNight(timeOfDay)) timeOfDay = 0.86f;
		QueueBeat(MakeBeat("하율", kSettleLines, 2));
		StoryBeat night = MakeBeat("하율", kNightLines, LineCount(kNightLines));
		night.follows = true;
		storyQueue.push_back(night);
		settleCardTimer = -2.0f;
	}
	if (settleCardTimer < -1.5f && !DialogOpen() && storyQueue.empty()) settleCardTimer = 0.0f;
	if (settleCardTimer >= 0.0f)
	{
		// The deer's lights go up into the branches and open there while the title shows.
		settleCardTimer += dt;
		if (settleCardTimer < kSettleCard - 2.0f && fxRng.Chance(Minf(dt * 14.0f, 1.0f)))
		{
			float a = fxRng.Range(0.0f, 2.0f * kPi);
			float r = fxRng.Range(2.0f, 12.0f);
			Wisp w;
			w.pos = s.root + Vec3(cosf(a) * r, 1.2f, sinf(a) * r);
			w.vel = Vec3(-cosf(a) * 0.4f, fxRng.Range(1.6f, 2.6f), -sinf(a) * 0.4f);
			w.maxLife = fxRng.Range(4.5f, 6.0f);
			w.life = w.maxLife;
			w.tint = fxRng.Chance(0.3f) ? Vec3(1.00f, 0.80f, 0.52f) : Vec3(0.70f, 1.00f, 0.86f);
			w.size = 0.9f;
			w.glow = 1.8f;
			w.drag = 0.05f;
			w.sway = 0.3f;
			w.plain = true;
			wisps.push_back(w);
		}
		if (settleCardTimer > kSettleCard) settleCardTimer = -1.0f;
	}
}

float Game::SporeField(const Vec3& pos) const
{
	// The tree breathes the spores in: they thicken across the city toward it, and under it they are gone.
	const StorySites& s = routeMap.Sites();
	float d = DistXZ(pos, s.root);
	if (d < kSanctuaryRadius) return -1.0f;
	float city = (float)kCityChunks * kChunkSize;
	if (d >= city) return 1.0f;
	return 1.0f + 0.9f * (1.0f - (d - kSanctuaryRadius) / (city - kSanctuaryRadius));
}

bool Game::InSanctuary(const Vec3& pos) const
{
	return level == LEVEL_ROUTE && DistXZ(pos, routeMap.Sites().root) < kSanctuaryRadius;
}

bool Game::AtHome() const
{
	return rootReached && InSanctuary(player->Position());
}

void Game::WayTo(const Vec3& to, char* buf, size_t size) const
{
	Vec3 d = to - player->Position();
	float dist = sqrtf(d.x * d.x + d.z * d.z);
	if (dist < 20.0f)
	{
		sprintf_s(buf, size, "바로 근처");
		return;
	}

	// South is +z and east +x, as on her map.
	const char* kWays[8] = { "남쪽", "남동쪽", "동쪽", "북동쪽", "북쪽", "북서쪽", "서쪽", "남서쪽" };
	float a = atan2f(d.x, d.z);
	int sector = (int)floorf(a / (kPi * 0.25f) + 0.5f) & 7;
	sprintf_s(buf, size, "%s 약 %dm", kWays[sector], ((int)dist / 10) * 10);
}

Vec3 Game::HomeSpotFront(int spot) const
{
	const float out[HOME_SPOT_COUNT] = { 3.2f, 1.4f, 1.6f, 0.9f, 1.3f };
	return HomeSpotPos(routeMap.Sites(), spot) + kFront * out[spot];
}

bool Game::FindLateSpot()
{
	lateSpot = -1;
	if (level != LEVEL_ROUTE || deathTimer >= 0.0f) return false;
	const Vec3& pos = player->Position();

	if (townReached && DistXZ(pos, TownWallPos()) < kLateReach)
	{
		lateSpot = kLateWall;
		prompt = townWallRead ? "[E]  엄마 글씨 다시 읽기" : "[E]  담벼락의 분필 글씨 읽기";
		return true;
	}

	const StorySites& s = routeMap.Sites();
	if (DistXZ(pos, s.ridge + kFront * 2.6f) < kLateReach + 0.6f)
	{
		lateSpot = kLatePylon;
		prompt = ridgeClimbed ? "[E]  송전탑에 올라 다시 보기" : "[E]  덩굴 감긴 송전탑에 오르기";
		return true;
	}

	if (!rootReached) return false;

	for (int i = 0; i < kNeighborCount; ++i)
	{
		NpcActor* n = neighborNpc[i];
		if (n == nullptr || DistXZ(pos, n->Position()) >= kInteractRange) continue;
		lateSpot = kLateNeighbor + i;
		prompt = i == 0 ? "[E]  도현과 이야기하기" : "[E]  여울과 이야기하기";
		return true;
	}

	char buf[192];
	for (int spot = 0; spot < HOME_SPOT_COUNT; ++spot)
	{
		if (DistXZ(pos, HomeSpotFront(spot)) >= kLateReach) continue;

		int scrap = HaveForHome(ITEM_SCRAP);
		int seeds = HaveForHome(ITEM_SEED);
		bool built = spot < kHomeProjects && homeBuilt[spot];
		switch (spot)
		{
		case HOME_SHELTER:
			if (built) sprintf_s(buf, sizeof(buf), IsNight(timeOfDay) ? "[E]  쉼터에서 아침까지 자기" : "[E]  쉼터에서 쉬기");
			else sprintf_s(buf, sizeof(buf), "[E]  무너진 쉼터 고치기  (자재 %d/%d)", scrap, kHomeScrap[spot]);
			break;
		case HOME_WELL:
			if (built) sprintf_s(buf, sizeof(buf), "[E]  우물물 마시고 물병 채우기  (장독에 고인 물 %d)", store[ITEM_CLEAN_WATER]);
			else sprintf_s(buf, sizeof(buf), "[E]  메워진 우물 다시 파기  (자재 %d/%d)", scrap, kHomeScrap[spot]);
			break;
		case HOME_GARDEN:
			if (built && gardenReady > 0) sprintf_s(buf, sizeof(buf), "[E]  무 거두기  (%d)", gardenReady);
			else if (built) sprintf_s(buf, sizeof(buf), "텃밭: 무가 자라는 중");
			else sprintf_s(buf, sizeof(buf), "[E]  풀에 묻힌 밭 일구기  (씨앗 %d/%d · 자재 %d/%d)", seeds, kHomeSeeds[spot], scrap, kHomeScrap[spot]);
			break;
		case HOME_LANTERN:
			// Once lit it is a lantern like any other: it roasts and it warms.
			if (built) continue;
			sprintf_s(buf, sizeof(buf), "[E]  쓰러진 석등 세우기  (자재 %d/%d)", scrap, kHomeScrap[spot]);
			break;
		default:
			sprintf_s(buf, sizeof(buf), "[E]  장독에 짐 풀기  (음식 %d · 물 %d · 약초 %d · 자재 %d · 씨앗 %d)",
					  StoredFood(), store[ITEM_CLEAN_WATER], store[ITEM_HERB], store[ITEM_SCRAP], store[ITEM_SEED]);
			break;
		}
		lateSpot = spot;
		prompt = buf;
		return true;
	}
	return false;
}

void Game::UseLateSpot()
{
	if (lateSpot == kLateWall) ReadTownWall();
	else if (lateSpot == kLatePylon) ClimbPylon();
	else if (lateSpot >= kLateNeighbor && lateSpot < kLateNeighbor + kNeighborCount) TalkToNeighbor(lateSpot - kLateNeighbor);
	else if (lateSpot >= 0 && lateSpot < HOME_SPOT_COUNT) UseHomeSpot(lateSpot);
}

void Game::ReadTownWall()
{
	// The long sleep now ends at the town gate rather than back at the first lantern.
	if (!ridgeClimbed && !rootReached) safePoint->SetPosition(TownWallPos() + Vec3(-2.5f, 0.0f, 0.0f));
	townWallRead = true;
	OpenBeat(MakeBeat("학교 담벼락", kWallLines, 3));
}

void Game::ClimbPylon()
{
	const StorySites& s = routeMap.Sites();
	bool first = !ridgeClimbed;
	ridgeClimbed = true;
	if (!rootReached) safePoint->SetPosition(s.ridge + kFront * 3.2f);

	char way[64];
	WayTo(s.root, way, sizeof(way));
	char last[192];
	sprintf_s(last, sizeof(last), "(포자가 강물처럼 그 나무 쪽으로 흘러간다. %s.)", way);

	StoryBeat beat;
	beat.speaker = "송전탑 위에서";
	beat.lines.push_back("(덩굴을 붙잡고 송전탑에 올랐다. 남쪽으로 숲이 끝없이 이어진다.)");
	beat.lines.push_back("(그 한가운데 구름에 닿을 듯한 나무 한 그루. 둘레의 도시가 통째로 이끼에 잠겨 있다.)");
	beat.lines.push_back(last);
	OpenBeat(beat);

	if (first) AddInsight(1, "포자는 아무렇게나 떠다니지 않는다. 큰 나무로 흘러간다.");
}

void Game::UseHomeSpot(int spot)
{
	if (spot == HOME_STORE)
	{
		PackAtHome();
		return;
	}
	if (!homeBuilt[spot])
	{
		BuildHomePart(spot);
		return;
	}

	if (spot == HOME_SHELTER)
	{
		health = MaxHealth(stats);
		warmthMeter = 100.0f;
		sporeExposure = 0.0f;
		if (IsNight(timeOfDay))
		{
			if (timeOfDay > 0.5f) AdvanceDay();
			timeOfDay = 0.26f;
			waterMeter = Maxf(waterMeter - 10.0f, 0.0f);
			foodMeter = Maxf(foodMeter - 10.0f, 0.0f);
			ShowMessage("집에서 아침까지 푹 잤다. 몸이 가뿐하다.", 3.5f);
		}
		else
		{
			ShowMessage("쉼터에서 한숨 돌렸다. 몸이 가뿐하다.", 3.0f);
		}
		return;
	}

	if (spot == HOME_WELL)
	{
		waterMeter = 100.0f;
		sporeExposure = 0.0f;
		int room = kBagCapacity - BagCount();
		int want = 3 - inventory[ITEM_CLEAN_WATER];
		int fill = want < room ? want : room;
		if (fill < 0) fill = 0;
		inventory[ITEM_CLEAN_WATER] += fill;

		char buf[160];
		sprintf_s(buf, sizeof(buf), "우물물을 실컷 마시고 물병 %d개를 채웠다.", fill);
		ShowMessage(buf, 3.0f);
		return;
	}

	if (spot == HOME_GARDEN)
	{
		if (gardenReady <= 0)
		{
			ShowMessage("무가 아직 자라는 중이다.", 2.0f);
			return;
		}
		store[ITEM_VEGETABLE] += gardenReady;

		char buf[128];
		sprintf_s(buf, sizeof(buf), "무 %d개를 거둬 장독에 넣었다.", gardenReady);
		ShowMessage(buf, 3.0f);
		gardenReady = 0;
		RefreshHomeLooks();
		PlayPlayerAction("pickup");
	}
}

void Game::BuildHomePart(int spot)
{
	int scrap = kHomeScrap[spot];
	int seeds = kHomeSeeds[spot];
	if (HaveForHome(ITEM_SCRAP) < scrap || HaveForHome(ITEM_SEED) < seeds)
	{
		char buf[192];
		if (seeds > 0)
			sprintf_s(buf, sizeof(buf), "%s 자재 %d개와 씨앗 %d봉투가 든다. 잠든 도시에서 구해 오자.", kBuildNeeds[spot], scrap, seeds);
		else
			sprintf_s(buf, sizeof(buf), "%s 자재 %d개가 든다. 잠든 도시에서 구해 오자.", kBuildNeeds[spot], scrap);
		ShowMessage(buf, 3.5f);
		return;
	}

	SpendForHome(ITEM_SCRAP, scrap);
	SpendForHome(ITEM_SEED, seeds);
	homeBuilt[spot] = true;
	RefreshHomeLooks();
	ShowMessage(kBuiltWords[spot], 4.5f);
	PlayPlayerAction("pickup");
	GrantXp(20);
}

void Game::PackAtHome()
{
	// Everything goes into the jars but a share for the road, two flasks, two meals and a herb, made up from the
	// jars when the bag is short of them.
	const int keepType[2] = { ITEM_CLEAN_WATER, ITEM_HERB };
	const int keepCount[2] = { 2, 1 };
	for (int k = 0; k < 2; ++k)
	{
		int t = keepType[k];
		store[t] += inventory[t];
		inventory[t] = store[t] < keepCount[k] ? store[t] : keepCount[k];
		store[t] -= inventory[t];
	}

	for (int i = 0; i < kFoodKeepCount; ++i)
	{
		int t = kFoodKeep[i];
		store[t] += inventory[t];
		inventory[t] = 0;
	}
	int meals = 0;
	for (int i = 0; i < kFoodKeepCount && meals < 2; ++i)
	{
		int t = kFoodKeep[i];
		int take = store[t] < 2 - meals ? store[t] : 2 - meals;
		store[t] -= take;
		inventory[t] += take;
		meals += take;
	}

	store[ITEM_SCRAP] += inventory[ITEM_SCRAP];
	inventory[ITEM_SCRAP] = 0;
	store[ITEM_SEED] += inventory[ITEM_SEED];
	inventory[ITEM_SEED] = 0;

	char buf[192];
	sprintf_s(buf, sizeof(buf), "장독에 짐을 풀고, 길 떠날 몫만 챙겼다.  물 %d · 음식 %d · 약초 %d",
			  inventory[ITEM_CLEAN_WATER], meals, inventory[ITEM_HERB]);
	ShowMessage(buf, 3.5f);
	PlayPlayerAction("pickup");
}

int Game::StoredFood() const
{
	int count = 0;
	for (int t = 0; t < ITEM_TYPE_COUNT; ++t)
	{
		if (FoodValue(t) > 0.0f) count += store[t];
	}
	return count;
}

void Game::SpendForHome(int type, int count)
{
	int fromStore = count < store[type] ? count : store[type];
	store[type] -= fromStore;
	inventory[type] -= count - fromStore;
}

bool Game::HomeComplete() const
{
	for (int i = 0; i < kHomeProjects; ++i)
	{
		if (!homeBuilt[i]) return false;
	}
	return true;
}

void Game::QueueBeat(StoryBeat beat)
{
	if (DialogOpen())
	{
		beat.follows = true;
		storyQueue.insert(storyQueue.begin(), beat);
		return;
	}
	OpenBeat(beat);
}

void Game::SkipToNextSite()
{
	if (level != LEVEL_ROUTE || DialogOpen() || MenuOpen() || deathTimer >= 0.0f) return;

	// Every press goes on to the next place. A place this left Hayul at counts as passed when it is pressed again,
	// whether or not the wall was read or the pylon climbed, so pressing alone reaches the tree.
	const StorySites& s = routeMap.Sites();
	const Vec3& pos = player->Position();
	const Vec3 wallSpot = TownWallPos() + Vec3(1.4f, 0.0f, 0.2f);   // east of the chalk, so Hayul does not stand over it
	const Vec3 ridgeSpot = s.ridge + kFront * 3.0f;
	const Vec3 parentsSpot = s.parents + s.toTown * 6.0f;
	if (townReached && DistXZ(pos, wallSpot) < 4.0f) townWallRead = true;
	if (DistXZ(pos, ridgeSpot) < 4.0f) ridgeClimbed = true;
	if (DistXZ(pos, parentsSpot) < 4.0f) parentsFound = true;

	// The tree is reached inside its clear air, in front of the home, where the long sleep will end from then on.
	const Vec3 spots[5] = { TownGatePos() + Vec3(0.0f, 0.0f, 4.0f), wallSpot, ridgeSpot, parentsSpot, HomeSpotFront(HOME_SHELTER) + kFront * 0.8f };
	const char* names[5] = { "은행나무 읍내 입구로", "학교 담벼락으로", "송전탑 능선으로", "부모님 자리로", "큰 나무 밑, 부모님이 짓다 만 집 앞으로" };
	int next = !townReached ? 0 : !townWallRead ? 1 : !ridgeClimbed ? 2 : !parentsFound ? 3 : !rootReached ? 4 : 5;
	if (next >= 5)
	{
		ShowMessage("(시연용) 더 건너뛸 이야기 장소가 없다.", 2.0f);
		return;
	}

	// The road's own lessons are skipped with it, so the goal shown follows the places from here on.
	if (guide < GUIDE_REACH_TOWN) SetGuide(GUIDE_REACH_TOWN);

	player->SetPosition(spots[next]);
	camera->SetPosition(spots[next]);
	char buf[160];
	sprintf_s(buf, sizeof(buf), "(시연용) %s 건너뛰었다. F5 로 다음 장소.", names[next]);
	ShowMessage(buf, 3.0f);
}
