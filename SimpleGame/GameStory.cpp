#include "stdafx.h"
#include "Game.h"

#include <cstdio>

#include "Models.h"

// The road's story comes from three sides of one family (ContentDesign.md 5): grandmother's notes on the map,
// opened by reaching their place; Hayul's own memories, opened by growing; and the owners of what is found.
// Hayul's memories and grandmother's remembered words are said over play; notes, relics and dreams stop the road.

namespace
{
	const float kStoryCalm = 1.5f;       // quiet seconds before a waiting beat opens
	const float kLineFadeIn = 0.35f;
	const float kLineFadeOut = 0.45f;
	const float kRoadMemoDelay = 6.0f;   // the first note waits for the title card to clear
	const int   kLastRecall = 7;
	const float kSleeperReach = 1.8f;
	const float kGuardReach = 6.0f;      // a live creature this close to a sleeper keeps you off

	const char* kMemoSpeaker = "할머니의 지도 메모";

	// The colour of the pencil each place is marked in on her map, and of its ring on screen while the note is
	// read: red around the bus stop, blue for the water, gold for the ginkgo trees. The lone tree is graphite on
	// the paper; on screen its ring is pale, or it would vanish against the leaves.
	const Vec3 kMemoMark[MEMO_COUNT] =
	{
		Vec3(0.35f, 0.33f, 0.30f),
		Vec3(0.95f, 0.32f, 0.24f),
		Vec3(0.32f, 0.62f, 1.00f),
		Vec3(1.00f, 0.80f, 0.26f),
		Vec3(0.96f, 0.93f, 0.80f),
	};

	// The paper covers 80 m across and the road from past the village to past the lone tree. It is much narrower
	// than it is long, so the width is stretched: the road's bends and the water off to the east both show. The
	// village is its own place; on her map it sits north of where the road starts.
	const float kMapX0 = -40.0f;
	const float kMapX1 = 40.0f;
	const float kMapZ0 = -75.0f;
	const float kMapZ1 = 262.0f;
	const Vec3  kMapVillage(0.0f, 0.0f, -62.0f);
	const float kMapRoadStart = -55.0f;

	// The names she wrote, and the few words beside each place once its note has been read; the lone tree has none.
	const char* kMapNames[MEMO_COUNT] = { "32번 국도", "버스 정류장", "저수지", "은행나무읍", nullptr };
	const char* kMapNotes[MEMO_COUNT] =
	{
		"네 할아버지 버스가 다니던 길",
		"여기서 탔다",
		"물이 줄면 안개도 준다",
		"네 엄마 학교가 있다",
		nullptr,
	};
	const Vec3 kMapInk(0.27f, 0.24f, 0.21f);      // her pencil
	const Vec3 kMapNoteInk(0.20f, 0.24f, 0.42f);  // the blue-black she wrote the notes in

	struct MemoText
	{
		const char* title;
		const char* lines[3];
	};

	const MemoText kMemos[MEMO_COUNT] =
	{
		{ "32번 국도", {
			"(지도 맨 위, 마을에서 남쪽으로 그은 선 옆에 할머니 글씨가 있다.)",
			"「32번 국도. 네 할아버지가 탄 버스가 다니던 길.」",
			"「손전등은 그이 것이다. 아껴 써라.」" } },
		{ "버스 정류장", {
			"(정류장 자리에 동그라미가 쳐져 있다.)",
			"「여기서 탔다. 돌아오는 버스 시간표는 끝내 붙지 않았지.」",
			"(줄 선 허물들 위로 그 글씨가 겹쳐 보인다.)" } },
		{ "저수지", {
			"(지도 동쪽 귀퉁이, 파란 색연필로 칠한 자리.)",
			"「가물기 전엔 물이 여기까지 찼다. 물안개가 마을을 지켜 준 건 이 물 덕이다.」",
			"「물이 줄면 안개도 준다. 그러면 포자가 넘어오겠지.」" } },
		{ "은행나무읍", {
			"(은행나무 두 그루가 그려져 있다.)",
			"「은행나무읍. 네 엄마가 다니던 학교가 있다.」",
			"「네 부모는 약을 구하러 이 길로 갔다. 여기까지는 소식이 왔었다.」" } },
		{ "남쪽 끝의 나무", {
			"(지도 남쪽 끝, 길이 끊긴 자리에 나무 하나가 그려져 있다.)",
			"(글씨는 없다. 할머니도 거기까지는 몰랐던 모양이다.)",
			"(그래도 연필 자국이 여러 번 덧그려져 있다.)" } },
	};

	// Levels 2 and 3 already speak through the growth lessons, so memories begin at 4.
	const char* kRecalls[kLastRecall - 3][2] =
	{
		{ "할머니는 물을 꼭 끓여 마시게 했다. 그땐 귀찮기만 했다.",
		  "끓인 물은 포자를 씻어 낸다. 할머니는 알고 있었다." },
		{ "할머니 손등의 푸른 얼룩. 소매로 가리던 게 언제부터였지.",
		  "새벽마다 물을 길으러 나가던 그 무렵부터였다." },
		{ "\"밥은 먹었냐.\" 할머니는 하루에도 몇 번씩 물었다.",
		  "혼자 남을 날을 준비시키던 말이었다는 걸, 이제야 안다." },
		{ "엄마 아빠 얘기가 나오면 할머니는 라디오를 껐다.",
		  "잡음 너머의 숨소리를, 나한테는 들려주고 싶지 않았던 걸까." },
	};

	const char* kOwnerLines[kRelicCount] =
	{
		"할아버지의 손전등. 할머니는 한 번도 버리지 않았다.",
		"정류장 의자 밑에 있었다. 버스를 기다리던 누군가가 아껴 둔 것이었을까.",
		"뒷면에 이름이 새겨져 있다. 아빠의 이름이다.",
		"할머니가 숨소리를 들었다던 라디오와 같은 것이다. 켜 보니, 잡음 아래로 무언가 숨 쉰다.",
		"할아버지가 떠난 해의 표다. 돌아오는 칸은 비어 있다.",
		"젊은 할머니와 할아버지, 엄마와 아빠. 내가 태어나기 전의 우리 집이다.",
		"꺼진 화면에 금이 가 있다. 뒷면 스티커의 글씨는 엄마 글씨다.",
		"은행나무읍 중학교. 엄마가 다니던 학교의 배지다.",
	};

	const char* kThreeRelics[] =
	{
		"옛 세상 사람들도 무언가를 기다렸다.",
		"버스를, 소식을, 돌아올 사람을.",
	};

	const char* kSixRelics[] =
	{
		"도감이 우리 집 이야기가 되어 간다.",
		"할머니가 말해 주지 않은 것들이, 물건마다 조금씩 남아 있다.",
	};

	// The day of the flowering, seen in pieces by the road. What began it is never shown.
	const int kRoadDreamCount = 5;
	const char* kRoadDreams[kRoadDreamCount][2] =
	{
		{ "(아침 출근길. 버스 창밖으로 노란 가루가 눈처럼 내린다.)",
		  "(사람들이 휴대폰으로 사진을 찍는다. 예쁘다고 웃는다.)" },
		{ "(정오. 라디오가 같은 말을 되풀이한다. 창문을 닫으십시오.)",
		  "(아무도 창문을 닫지 않았다. 날이 너무 좋았다.)" },
		{ "(오후. 운전기사가 하품을 한다. 버스가 길가에 선다.)",
		  "(아무도 내리지 않는다. 다들 조금만 쉬었다 가기로 한다.)" },
		{ "(저녁. 학교 운동장의 은행나무가 한꺼번에 노랗게 터진다.)",
		  "(아이들이 그 아래에서 잠든다. 웃는 얼굴로.)" },
		{ "(밤. 도시 한가운데서 무언가 자라는 소리가 난다.)",
		  "(그 소리는 꼭, 아주 큰 숨소리 같았다.)" },
	};

	const char* kHerbWords[] = { "\"쓴 풀이 몸엔 낫다. 아플 땐 아끼지 말고 씹어라.\"" };
	const char* kMeatWords[] = { "\"짐승한테 받은 건 불에 익혀 먹어라. 그게 예의다.\"" };
	const char* kBerryWords[] = { "\"빨간 건 먹고, 허연 건 두어라. 숲이 알려 주는 대로.\"" };

	// Long enough to read at an easy pace, counted in characters rather than bytes.
	float LineTime(const std::string& text)
	{
		int glyphs = 0;
		for (size_t i = 0; i < text.size(); ++i)
		{
			if (((unsigned char)text[i] & 0xC0) != 0x80) ++glyphs;
		}
		return Clampf(2.0f + 0.09f * (float)glyphs, 3.5f, 8.0f);
	}

	Vec3 SpeakerColor(const std::string& speaker)
	{
		if (speaker == "기억 속 할머니") return Vec3(0.95f, 0.82f, 0.62f);
		return Vec3(0.78f, 0.86f, 0.95f);
	}
}

void Game::UpdateStory(float dt)
{
	if (levelTimer > kRoadMemoDelay) MarkMemo(MEMO_ROAD);

	while (nextRecall <= kLastRecall && stats.level >= nextRecall)
	{
		Narrate("하율", kRecalls[nextRecall - 4], 2);
		++nextRecall;
	}

	UpdateNarration(dt);

	// A beat never opens in the middle of a fight; it waits until the road has been quiet a moment. One that
	// follows the beat before it opens as soon as that one closes.
	storyCalm = (deathTimer < 0.0f && StoryCalm()) ? storyCalm + dt : 0.0f;
	if (storyQueue.empty() || DialogOpen()) return;
	if (storyCalm < kStoryCalm && !storyQueue.front().follows) return;

	StoryBeat beat = storyQueue.front();
	storyQueue.erase(storyQueue.begin());
	OpenBeat(beat);
	storyCalm = 0.0f;
}

void Game::OpenBeat(const StoryBeat& beat)
{
	dialogSpeaker = beat.speaker;
	dialogLines = beat.lines;
	dialogIndex = 0;
	dialogNext = -1;
	if (beat.relic >= 0)
	{
		showcaseRelic = beat.relic;
		showcaseFade = 0.0f;
	}
	if (beat.memo < 0) return;

	// A note is read off her map, so the map is held up beside it, on the side away from the place it names.
	memoMap = beat.memo;
	memoMapLeft = true;
	Vec3 at;
	switch (beat.memo)
	{
	case MEMO_BUS_STOP:  at = BusStopPos(); break;
	case MEMO_RESERVOIR: at = kReservoirCenter; break;
	case MEMO_TOWN:      at = TownGatePos(); break;
	case MEMO_TREE:      at = OldTreePos(); break;
	default:             return;
	}
	float sx, sy;
	if (renderer->WorldToScreen(at, sx, sy)) memoMapLeft = sx > (float)renderer->GetWidth() * 0.5f;
}

void Game::Narrate(const char* speaker, const char* const* lines, int count)
{
	for (int i = 0; i < count; ++i)
	{
		NarrationLine line;
		line.speaker = speaker;
		line.text = lines[i];
		narration.push_back(line);
	}
}

void Game::UpdateNarration(float dt)
{
	// A dialog takes the place on screen, and the line waits under it.
	if (narration.empty() || DialogOpen()) return;

	narrationTimer += dt;
	if (narrationTimer < LineTime(narration.front().text)) return;

	narration.erase(narration.begin());
	narrationTimer = 0.0f;
}

const char* Game::NarrationSpeaker() const
{
	if (narration.empty() || DialogOpen()) return "";
	return narration.front().speaker.c_str();
}

void Game::DrawNarration()
{
	if (narration.empty() || DialogOpen()) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();
	const NarrationLine& line = narration.front();

	float shown = LineTime(line.text);
	float alpha = Saturatef(narrationTimer / kLineFadeIn) * Saturatef((shown - narrationTimer) / kLineFadeOut);
	if (alpha <= 0.0f) return;

	// A subtitle over the road: no box to read through, only a band of shade under the words.
	int tw = renderer->TextWidth(line.text.c_str(), false);
	int sw = renderer->TextWidth(line.speaker.c_str(), false);
	int y = h - 236;
	renderer->DrawRectPx((float)(w / 2 - tw / 2 - 26), (float)(y - 46), (float)(tw + 52), 60.0f, Vec3(0.02f, 0.03f, 0.03f), 0.42f * alpha);
	renderer->DrawTexts(w / 2 - sw / 2, y - 24, line.speaker.c_str(), SpeakerColor(line.speaker) * (0.85f * alpha), false);
	renderer->DrawTexts(w / 2 - tw / 2, y, line.text.c_str(), kHudInk * alpha, false);
}

bool Game::StoryCalm() const
{
	std::vector<EnemyActor*> enemies = LiveEnemies();
	for (size_t i = 0; i < enemies.size(); ++i)
	{
		const EnemyActor& e = *enemies[i];
		if (e.state == ENEMY_DYING) continue;
		float d = DistXZ(e.Position(), player->Position());
		if (d < 6.0f || (e.state != ENEMY_IDLE && d < 14.0f)) return false;
	}
	return true;
}

void Game::MarkMemo(int memo)
{
	if (memoSeen[memo]) return;
	memoSeen[memo] = true;

	// A note stops the road and is read line by line. A note with a place opens the moment that place is in
	// sight, without waiting for the road to be quiet, or the place could leave the screen before the words
	// arrive. The road's own note has nothing to show and waits for quiet as before.
	StoryBeat beat;
	beat.speaker = kMemoSpeaker;
	beat.lines.assign(kMemos[memo].lines, kMemos[memo].lines + 3);
	beat.memo = memo;
	if (memo == MEMO_ROAD)
	{
		storyQueue.push_back(beat);
		mapUnread = true;
		return;
	}
	if (DialogOpen())
	{
		beat.follows = true;
		storyQueue.insert(storyQueue.begin(), beat);
	}
	else
	{
		OpenBeat(beat);
	}

	// Its place is ringed while the note is open. Starting just above zero keeps the ring from being let go
	// before it has faded in.
	if (outlineMemo >= 0 && outlineMemo != memo) OutlineMemoPlace(outlineMemo, 0.0f);
	outlineMemo = memo;
	outlineStrength = 0.01f;

	// The words now stand beside the place on her map too.
	mapUnread = true;
}

bool Game::PlaceInSight(const Vec3& at, float height) const
{
	float sx, sy;
	if (!renderer->WorldToScreen(at + Vec3(0.0f, height * 0.5f, 0.0f), sx, sy)) return false;
	float w = (float)renderer->GetWidth();
	float h = (float)renderer->GetHeight();
	return sx > w * 0.12f && sx < w * 0.88f && sy > h * 0.12f && sy < h * 0.78f;
}

void Game::OutlineMemoPlace(int memo, float strength)
{
	// The landmark props live in their chunk; a chunk built again later picks the ring up on the next frame.
	int cx = 0, cz = -1, model = -1;
	Vec3 around;
	float within = 1.0f;
	switch (memo)
	{
	case MEMO_BUS_STOP:
		cz = kBusStopChunkZ;
		model = MODEL_BUS_STOP;
		around = BusStopPos();
		break;
	case MEMO_TOWN:
		cz = kTownChunkZ;
		model = MODEL_GINKGO;
		around = TownGatePos();
		within = 9.0f;
		break;
	case MEMO_TREE:
		cz = kOldTreeChunkZ;
		model = MODEL_TREE;
		around = OldTreePos();
		break;
	case MEMO_RESERVOIR:
		if (reservoir)
		{
			reservoir->outline = strength;
			reservoir->outlineColor = kMemoMark[memo];
		}
		return;
	default:
		return;
	}

	std::unordered_map<uint64_t, ChunkActor*>::iterator found = chunkActors.find(ChunkMap::Key(cx, cz));
	if (found == chunkActors.end()) return;
	ChunkActor* chunk = found->second;
	for (size_t i = 0; i < chunk->ChildCount(); ++i)
	{
		Actor* child = chunk->Child(i);
		if (child->Kind() != ACTOR_PROP) continue;
		PropActor* prop = static_cast<PropActor*>(child);
		if (prop->model != model || DistXZ(prop->Position(), around) > within) continue;
		prop->outline = strength;
		prop->outlineColor = kMemoMark[memo];
	}
}

void Game::NoteFirstFind(int itemType)
{
	if (level != LEVEL_ROUTE || firstFound[itemType]) return;

	const char* const* words = nullptr;
	if (itemType == ITEM_HERB) words = kHerbWords;
	else if (itemType == ITEM_MEAT) words = kMeatWords;
	else if (itemType == ITEM_BERRIES) words = kBerryWords;
	if (!words) return;

	firstFound[itemType] = true;
	Narrate("기억 속 할머니", words, 1);
}

void Game::TellRelicOwner(int relic)
{
	StoryBeat beat;
	beat.speaker = RelicName(relic);
	beat.lines.push_back(kOwnerLines[relic]);
	beat.relic = relic;

	// In the village nothing waits in line: the find is told the moment it is picked up.
	if (level != LEVEL_ROUTE)
	{
		OpenBeat(beat);
		return;
	}
	storyQueue.push_back(beat);

	// Hayul's thoughts on how many have been found are a dialog of their own, opening as the owner's closes.
	StoryBeat thought;
	thought.speaker = "하율";
	thought.follows = true;
	int found = 0;
	for (int i = 0; i < kRelicCount; ++i) found += relicFound[i] ? 1 : 0;
	if (found >= 3 && relicsTold < 3)
	{
		relicsTold = 3;
		thought.lines.assign(kThreeRelics, kThreeRelics + 2);
	}
	if (found >= 6 && relicsTold < 6)
	{
		relicsTold = 6;
		thought.lines.assign(kSixRelics, kSixRelics + 2);
	}
	if (!thought.lines.empty()) storyQueue.push_back(thought);
}

Vec3 Game::ShowcasePosition() const
{
	// The view is orthographic, so any point on the line from the view's centre toward the eye lands mid-screen.
	// Far enough along it, the find stands in front of every roof; a little up the screen, the dialog box
	// below leaves it clear.
	float yaw = camera->Yaw();
	float el = camera->elevation;
	Vec3 toEye(cosf(el) * sinf(yaw), sinf(el), cosf(el) * cosf(yaw));
	Vec3 up = Normalize(Vec3(0.0f, 1.0f, 0.0f) - toEye * toEye.y);
	return camera->Position() + toEye * 34.0f + up * (camera->orthoHeight * 0.06f);
}

void Game::DrawRelicShowcase()
{
	if (showcaseRelic < 0 || showcaseFade <= 0.0f) return;

	// About half a metre tall, it is held up to a fifth of the screen, tipped back a little so it turns to face
	// the eye; the mesh spins and hovers by itself. It gathers out of light and comes apart into it again.
	float size = camera->orthoHeight * 0.2f / 0.5f * (0.85f + 0.15f * showcaseFade);
	Mat4 place = Mul(Mul(MatTranslate(ShowcasePosition()), MatRotateY(camera->Yaw())), MatRotateX(-0.35f));
	Mat4 model = Mul(Mul(place, MatScale(Vec3(size, size, size))), MatTranslate(Vec3(0.0f, -0.3f, 0.0f)));

	DrawParams params;
	params.emissive = 0.3f;
	params.dissolve = 1.0f - showcaseFade;
	params.outline = showcaseFade;
	params.outlineColor = Vec3(1.00f, 0.86f, 0.52f);
	renderer->DrawModel(RelicModel(showcaseRelic), model, params);
}

bool Game::FindRouteSleeper()
{
	restIndex = -1;
	const Vec3& pos = player->Position();

	for (int dz = -1; dz <= 1; ++dz)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			int cx = playerChunkX + dx;
			int cz = playerChunkZ + dz;
			const Chunk& chunk = routeMap.Get(cx, cz);
			for (size_t i = 0; i < chunk.sleepers.size(); ++i)
			{
				const Vec3& at = chunk.sleepers[i].pos;
				if (DistXZ(pos, at) > kSleeperReach) continue;

				auto found = chunkStates.find(ChunkMap::Key(cx, cz));
				if (found != chunkStates.end())
				{
					const std::vector<bool>& rested = found->second.sleeperRested;
					if (i < rested.size() && rested[i]) continue;
				}

				// The boar at their side has to be dealt with first.
				std::vector<EnemyActor*> enemies = LiveEnemies();
				bool guarded = false;
				for (size_t j = 0; j < enemies.size() && !guarded; ++j)
					guarded = enemies[j]->state != ENEMY_DYING && DistXZ(enemies[j]->Position(), at) < kGuardReach;
				if (guarded) continue;

				restChunkX = cx;
				restChunkZ = cz;
				restIndex = (int)i;
				return true;
			}
		}
	}
	return false;
}

void Game::RestBySleeper()
{
	ChunkState& state = chunkStates[ChunkMap::Key(restChunkX, restChunkZ)];
	const Chunk& chunk = routeMap.Get(restChunkX, restChunkZ);
	if (state.sleeperRested.size() != chunk.sleepers.size()) state.sleeperRested.assign(chunk.sleepers.size(), false);
	state.sleeperRested[restIndex] = true;
	restIndex = -1;

	if (roadDreams >= kRoadDreamCount)
	{
		ShowMessage("고요히 잠들어 있다. 더 보이는 꿈은 없다.", 3.0f);
		return;
	}

	AddInsight(1, "꿈 조각을 얻었다.");
	const char* const* lines = kRoadDreams[roadDreams++];
	dialogSpeaker = "꿈 조각 · 길가의 잠든 이";
	dialogLines.assign(lines, lines + 2);
	dialogIndex = 0;
	dialogNext = -1;
}

void Game::DrawMemoPage(int x, int y, int maxWidth)
{
	bool any = false;
	for (int i = 0; i < MEMO_COUNT; ++i)
	{
		if (!memoSeen[i]) continue;
		any = true;
		renderer->DrawTexts(x, y, kMemos[i].title, Vec3(0.92f, 0.84f, 0.62f), false);
		int lines = DrawWrapped(x + 16, y + 20, kMemos[i].lines[1], maxWidth - 16, kHudInk);
		y += 30 + 20 * lines;
	}
	if (!any) DrawWrapped(x, y, "아직 펼쳐 본 메모가 없다. 길을 걸으면 지도가 말을 건다.", maxWidth, kHudDim);
}

int Game::DrawWrapped(int x, int y, const char* text, int maxWidth, const Vec3& color)
{
	// Breaks at spaces; a word too long for a whole line is left whole on its own.
	std::string rest = text;
	int lines = 0;
	while (!rest.empty())
	{
		size_t cut = rest.size();
		if (renderer->TextWidth(rest.c_str(), false) > maxWidth)
		{
			size_t fits = std::string::npos;
			for (size_t space = rest.find(' '); space != std::string::npos; space = rest.find(' ', space + 1))
			{
				if (renderer->TextWidth(rest.substr(0, space).c_str(), false) > maxWidth) break;
				fits = space;
			}
			if (fits == std::string::npos) fits = rest.find(' ');
			if (fits != std::string::npos) cut = fits;
		}
		renderer->DrawTexts(x, y + 20 * lines, rest.substr(0, cut).c_str(), color, false);
		++lines;
		rest = cut < rest.size() ? rest.substr(cut + 1) : std::string();
	}
	return lines;
}

bool Game::MapOwned() const
{
	return level == LEVEL_ROUTE || tutorial >= TUT_PACK;
}

bool Game::MemoMapRect(float& x, float& y, float& w, float& h) const
{
	if (memoMap < 0 || mapOpen || screenFx.memo <= 0.0f) return false;

	// Wide enough for her words, beside the dialog box rather than under it.
	float sh = (float)renderer->GetHeight();
	h = Minf(sh * 0.74f, 640.0f);
	w = h * 0.62f;
	x = memoMapLeft ? 32.0f : (float)renderer->GetWidth() - 32.0f - w;
	y = sh * 0.5f - h * 0.5f - 6.0f;
	return true;
}

void Game::DrawMemoMap()
{
	float x, y, w, h;
	if (!MemoMapRect(x, y, w, h)) return;

	// Brought in from its side of the screen as the note opens, and put away the same way.
	float shown = SmoothStep(0.0f, 1.0f, screenFx.memo);
	float away = (1.0f - shown) * (w + 60.0f);
	x += memoMapLeft ? -away : away;
	renderer->DrawRectPx(x + 7.0f, y + 9.0f, w, h, Vec3(0.0f, 0.0f, 0.0f), 0.35f * shown);
	DrawGrandmaMap(x, y, w, h, true, memoMap);
}

void Game::DrawGrandmaMap(float x, float y, float w, float h, bool notes, int focus)
{
	MapParams params;
	params.x0 = kMapX0;
	params.z0 = kMapZ0;
	params.x1 = kMapX1;
	params.z1 = kMapZ1;
	params.places[0] = kMapVillage;
	params.places[1] = BusStopPos();
	params.places[2] = kReservoirCenter;
	params.places[3] = TownGatePos();
	params.places[4] = OldTreePos();
	for (int i = 0; i < MEMO_COUNT; ++i) params.placeRead[i] = memoSeen[i];
	params.focus = focus;
	params.roadStartZ = kMapRoadStart;
	params.roadEndZ = kOldTreeZ;
	params.reservoirX = kReservoirSizeX;
	params.reservoirZ = kReservoirSizeZ;
	// Out on the road she is where she stands; still at home, she is in the village.
	params.player = level == LEVEL_ROUTE ? player->Position() : kMapVillage;
	params.playerShown = true;
	renderer->DrawMap(x, y, w, h, params);

	// The words on the paper, laid out in pixels from the places they name and kept inside the sheet: a name
	// starts at, ends at or is centred on its offset point, and the note sits under it the same way.
	struct Word
	{
		Vec3  at;
		float dx;
		float dy;
		int   align;     // 1 starts there, -1 ends there, 0 centred
	};
	const Word words[MEMO_COUNT] =
	{
		{ Vec3(RoadCenter(-34.0f), 0.0f, -34.0f), 12.0f, 4.0f, 1 },
		{ BusStopPos(), -20.0f, 5.0f, -1 },
		{ kReservoirCenter, 0.0f, 22.0f + kReservoirSizeZ * 0.58f * w / (kMapX1 - kMapX0), 0 },
		{ TownGatePos() + Vec3(6.5f, 0.0f, 0.0f), 14.0f, -2.0f, 1 },
		{ OldTreePos(), 0.0f, 0.0f, 1 },
	};

	// A sheet too small for her words only shows the drawing; the journal lists the notes beside it.
	if (w < 300.0f) return;

	float left = x + 10.0f;
	float right = x + w - 10.0f;
	float vx = x + (kMapVillage.x - kMapX0) / (kMapX1 - kMapX0) * w;
	float vy = y + (kMapVillage.z - kMapZ0) / (kMapZ1 - kMapZ0) * h;
	const char* village = "물안개 마을";
	int villageWidth = renderer->TextWidth(village, false);
	renderer->DrawTexts((int)Clampf(vx + 28.0f, left, right - (float)villageWidth), (int)vy + 6, village, kMapInk, false);

	for (int i = 0; i < MEMO_COUNT; ++i)
	{
		if (!kMapNames[i]) continue;
		const Word& word = words[i];
		float ax = x + (word.at.x - kMapX0) / (kMapX1 - kMapX0) * w + word.dx;
		float ay = y + (word.at.z - kMapZ0) / (kMapZ1 - kMapZ0) * h + word.dy;

		const char* lines[2] = { kMapNames[i], notes && memoSeen[i] ? kMapNotes[i] : nullptr };
		for (int k = 0; k < 2; ++k)
		{
			if (!lines[k]) continue;
			float tw = (float)renderer->TextWidth(lines[k], false);
			float tx = word.align > 0 ? ax : (word.align < 0 ? ax - tw : ax - tw * 0.5f);
			tx = Clampf(tx, left, right - tw);
			renderer->DrawTexts((int)tx, (int)ay + 19 * k, lines[k], k == 0 ? kMapInk : kMapNoteInk, false);
		}
	}
}

void Game::DrawMapPanel()
{
	if (!mapOpen) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();
	renderer->DrawFade(Vec3(0.02f, 0.03f, 0.03f), 0.30f);

	// Taller than wide, like the folded sheet she kept; its shadow falls a little down and to the right.
	float ph = Minf((float)h * 0.84f, 820.0f);
	float pw = ph * 0.62f;
	float px = (float)w * 0.5f - pw * 0.5f;
	float py = (float)h * 0.5f - ph * 0.5f + 10.0f;
	renderer->DrawRectPx(px + 7.0f, py + 9.0f, pw, ph, Vec3(0.0f, 0.0f, 0.0f), 0.35f);
	DrawGrandmaMap(px, py, pw, ph, true);

	int read = 0;
	for (int i = 0; i < MEMO_COUNT; ++i) read += memoSeen[i] ? 1 : 0;
	char buf[64];
	sprintf_s(buf, sizeof(buf), "읽은 메모 %d / %d", read, (int)MEMO_COUNT);
	renderer->DrawTexts((int)px, (int)py - 14, "할머니의 지도", kHudInk, true);
	renderer->DrawTexts((int)(px + pw) - renderer->TextWidth(buf, false), (int)py - 14, buf, kHudDim, false);
	renderer->DrawTexts((int)px, (int)(py + ph) + 28, "[M] 닫기", kHudAccent, false);
}
