#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Math3D.h"
#include "Random.h"
#include "Renderer.h"
#include "ChunkMap.h"
#include "Rpg.h"
#include "SceneGraph.h"
#include "GameActors.h"

const float kInteractRange = 2.3f;
const float kLevelUpTime = 3.0f;    // seconds the level-up banner and its light last

const Vec3 kHudPanel(0.03f, 0.05f, 0.05f);
const Vec3 kHudInk(0.90f, 0.93f, 0.90f);
const Vec3 kHudDim(0.62f, 0.68f, 0.65f);
const Vec3 kHudAccent(0.60f, 0.92f, 0.80f);

// Mulangae Village over a day and a half. Comments give the step of the design (ContentDesign.md 2).
enum TutorialStep
{
	TUT_WAKE,             // 1 morning: grandmother's first words
	TUT_FETCH_WATER,      // 1-2 walk to the well and draw water
	TUT_BRING_WATER,      // 2 back to grandmother
	TUT_FORAGE,           // 3 radishes and red berries
	TUT_BRING_FOOD,       // 3
	TUT_EAT,              // 3 she watches you eat
	TUT_FETCH_TORCH,      // 4 the shed
	TUT_BRING_TORCH,      // 4
	TUT_DUSK,             // 5 evening falls, go home
	TUT_WATCH_DEER,       // 5
	TUT_BEDTIME,          // 5
	TUT_FIND_GRANDMOTHER, // 6 the next morning
	TUT_READ_LETTER,      // 6
	TUT_PACK,             // 7 choose what goes in the bag
	TUT_LEAVE,            // 7 walk south out of the village
	TUT_DONE
};

// What the interaction key would act on in the village.
enum VillageSpot
{
	SPOT_NONE,
	SPOT_DIALOG,
	SPOT_LETTER_OPEN,
	SPOT_GRANDMA,
	SPOT_LETTER,
	SPOT_DOOR,
	SPOT_WELL,
	SPOT_SHED,
	SPOT_DEER,
	SPOT_FORAGE,
	SPOT_SLEEPER
};

enum LevelId
{
	LEVEL_VILLAGE,
	LEVEL_ROUTE
};

// Route 32, the first outside: fight and grow, keep the body going, let nature show the way,
// carry the old world, and reach Ginkgo Town (ContentDesign.md 3). Then, on the same growing map, the way
// to the great tree and the home under it (ContentDesign.md 6).
enum RouteGuide
{
	GUIDE_TAKE_PIPE,
	GUIDE_FIGHT_MITES,
	GUIDE_ASSIGN_STATS,
	GUIDE_FIGHT_BOAR,
	GUIDE_ASSIGN_AGAIN,
	GUIDE_BUS_STOP,
	GUIDE_FIND_WATER,
	GUIDE_REACH_TOWN,
	GUIDE_EXPLORE,        // the story told: the world goes on
	GUIDE_TOWN_WALL,      // the empty town, and the mother's words on the school wall
	GUIDE_RIDGE,          // up the pylon on the ridge, to see the great tree
	GUIDE_ROOT,           // through the sleeping city to the tree
	GUIDE_HOME,           // finish the home the parents began under it
	GUIDE_STOCK           // fill its jars: a life that is enough
};

// Notes grandmother left in the margins of the map, each read on reaching its place (ContentDesign.md 5).
enum StoryMemo
{
	MEMO_ROAD,
	MEMO_BUS_STOP,
	MEMO_RESERVOIR,
	MEMO_TOWN,
	MEMO_TREE,
	MEMO_COUNT
};

// A dialog waiting for a quiet moment on the road.
struct StoryBeat
{
	std::string speaker;
	std::vector<std::string> lines;
	int  relic = -1;        // the find shown in the middle of the screen while it is told
	int  memo = -1;         // the note on her map it reads; the map is held up beside it
	bool follows = false;   // opens as soon as the dialog before it closes, without waiting for quiet
};

// A line said over play, without stopping it.
struct NarrationLine
{
	std::string speaker;
	std::string text;
};

enum Season
{
	SEASON_SPRING,
	SEASON_SUMMER,
	SEASON_AUTUMN,
	SEASON_WINTER,
	SEASON_COUNT
};

enum Weather
{
	WEATHER_CLEAR,
	WEATHER_RAIN,
	WEATHER_SNOW,
	WEATHER_WIND,     // a gale; in autumn it carries the leaves
	WEATHER_COUNT
};

// Rows of the graphics options panel (F4).
enum OptionRow
{
	OPT_PRESET,
	OPT_SCALE,
	OPT_MSAA,
	OPT_SHADOW,
	OPT_VOLUMETRIC,
	OPT_PARTICLES,
	OPT_SSAO,
	OPT_VSYNC,
	OPT_FPS,
	OPT_COUNT
};

enum QualityPreset
{
	PRESET_LOW,
	PRESET_MEDIUM,
	PRESET_HIGH,
	PRESET_ULTRA,
	PRESET_COUNT      // one past the presets: "custom"
};

// Frame timing gathered by the main loop, once a second.
struct PerfStats
{
	float    fps = 0.0f;
	float    lowFps = 0.0f;      // 1% low: the frame rate of the slowest frames
	float    worstMs = 0.0f;
	float    updateMs = 0.0f;    // CPU, averaged
	float    renderMs = 0.0f;
	GpuTimes gpu;
	int      drawCalls = 0;
	int      spikes = 0;         // frames over 33 ms
};

struct Popup
{
	Vec3  pos;
	Vec3  color;
	float timer = 0.0f;
	char  text[32] = "";
};

// A mote of light let go when a creature is released; it drifts up and shrinks away. The same mote is
// a relic's glint, a find drawn into the player, an ember, a trail of light, or dust when it does not glow.
struct Wisp
{
	Vec3  pos;
	Vec3  vel;
	float life = 0.0f;
	float maxLife = 1.0f;
	Vec3  tint{ 1.0f, 1.0f, 1.0f };
	float size = 1.3f;
	float glow = 1.0f;        // 0 for dust, lit like everything else instead of shining
	float drag = 0.9f;
	float lift = 0.0f;        // upward acceleration; negative lets it settle
	float sway = 0.35f;
	float seek = 0.0f;        // > 0 draws it into the player
	bool  plain = false;      // a round mote, without the released spore's streak
};

// Pressed into snow or wet ground, fading over a while.
struct Footprint
{
	Vec3  pos;
	float yaw = 0.0f;
	float born = 0.0f;
	float strength = 0.0f;
	Vec3  color;
};

// What has happened to a generated chunk's creatures and finds since it was first visited.
struct ChunkState
{
	bool  active = false;
	int   cx = 0;
	int   cz = 0;
	std::vector<float> respawnAt;   // game time when each spawn may appear again
	std::vector<bool>  itemTaken;
	std::vector<bool>  swarmCleared;   // a car's mites driven off once leaves its find behind for good
	std::vector<float> bushRegrowAt;   // game time when each picked bush fruits again
	std::vector<bool>  sleeperRested;  // a dream already seen beside this sleeper
};

class Game
{
public:
	explicit Game(Renderer* r);

	void Update(float dt, const bool* keys);
	void Render();

	// Edge-triggered: fires once per physical press.
	void OnKeyDown(unsigned char key);
	void OnMouseDown();

	// Positive steps pull the view in toward the player, negative ones back out.
	void ZoomBy(float steps);

	// Demo aid: jump straight to the level after the tutorial.
	void SkipToRoute();
	// Demo aid: on the road, jump to the next place the story waits at.
	void SkipToNextSite();

	bool WantsQuit() const { return quit; }

	// GameMonitor.cpp: a read-only snapshot for the browser monitor
	std::string MonitorJson() const;
	void SetPerf(const PerfStats& stats) { perf = stats; }

	// GameOptions.cpp: graphics options, saved to Options.txt when the panel closes
	void ToggleOptions();

private:
	// Game.cpp: shared by every level
	void UpdatePlayer(float dt, const bool* keys);
	void UpdateCamera(float dt);
	void DrawObjective(const char* text);
	void DrawHudLine(const char* text, const Vec3& color);     // the next line down the top-left column
	bool UnderMemoMap(float x, float y, float w, float h) const;   // her map, held beside a note, covers it
	void DialogRect(float& x, float& y, float& w, float& h) const;
	void DrawCommonHud(const char* help);
	void DrawTitleCard(const char* title, const char* subtitle);
	void DrawTitleCardAt(const char* title, const char* subtitle, float t);
	void ShowMessage(const char* text, float seconds);
	void GatherLights(const Vec3& view);

	// GameVillage.cpp: Mulangae Village, the tutorial
	void BuildVillage();
	PropActor* AddProp(int model, const Vec3& pos, const Vec3& scale, float yaw, float halfX, float halfZ);
	void AddHouse(const Vec3& pos, float w, float h, float d, float yaw, int model);
	void AddTree(const Vec3& pos, float scale);
	void AddFence(const Vec3& from, const Vec3& to);
	SleeperActor* AddSleeper(const Vec3& pos, float yaw, bool grandma, float phase);
	void AddForage(int kind, const Vec3& pos);
	void UpdateVillage(float dt, const bool* keys);
	void UpdateVillageStory(float dt);
	void UpdateDeer(float dt);
	void ResolveVillageCollisions();
	void UpdateInteractionTarget();
	void TryInteract();
	void TalkToGrandmother();
	void SetTutorial(int next);
	void Say(const char* const* lines, int count, int next, const char* speaker = "할머니");
	void AdvanceDialog();
	bool DialogOpen() const { return dialogIndex < dialogLines.size(); }
	void Pick(ForageActor& f);
	void WakeNextMorning();
	void OpenPack();
	void HandlePackKey(unsigned char key);
	void ConfirmPack();
	void DrawVillageHud();
	void DrawDialog();
	void DrawLetterPanel();
	void DrawPackPanel();
	void DrawVillageFades();
	void ObjectiveText(char* buf, size_t size) const;
	void VillageHelp(char* buf, size_t size) const;

	// GameRoute.cpp: Route 32, grown chunk by chunk from the world seed
	void StartRoute();
	void UpdateRoute(float dt, const bool* keys);
	void SetGuide(int next);
	void UpdateGuide();
	void GuideText(char* buf, size_t size) const;
	void StreamChunks();
	void UpdateChunkActivation();
	void ActivateChunk(int cx, int cz);
	void DeactivateChunk(ChunkState& state);
	bool HasChunk(int cx, int cz) const;
	ChunkActor* EnsureChunk(int cx, int cz);
	void PrepareChunkView();
	bool ChunkInView(int cx, int cz, float reach, float top) const;
	void ResolveRouteCollisions(Vec3& pos, float radius);
	void UpdateLandmarks(float dt);
	void UpdateRouteDeer(float dt);
	void TryRouteInteract();
	bool NearLanternFire() const;   // close enough to roast, with raw meat in the bag
	bool NearLantern(const Vec3& pos, float range, Vec3* at = nullptr) const;   // the road's first lantern, or one lit at a rest
	void UpdateRouteForage();
	void PickRouteBush(ForageActor& bush);
	void DrawRouteHud();
	void DrawGuideCard();

	// GameHome.cpp: past Ginkgo Town, on the same growing map: the way to the great tree and the home under it
	void BuildHome();
	void UpdateHome(float dt);
	float SporeField(const Vec3& pos) const;   // 1 on the road, up to about 2 in the sleeping city, below 0 under the tree
	bool InSanctuary(const Vec3& pos) const;
	bool AtHome() const;                       // under the tree once the home has been found
	void WayTo(const Vec3& to, char* buf, size_t size) const;   // which way and how far, in words
	Vec3 HomeSpotFront(int spot) const;        // where to stand to use a part of the home
	bool FindLateSpot();                       // the school wall, the pylon or a part of the home close by
	void UseLateSpot();
	void ReadTownWall();
	void ClimbPylon();
	void UseHomeSpot(int spot);
	void BuildHomePart(int spot);
	void RefreshHomeLooks();
	void PackAtHome();
	int  StoredFood() const;
	int  HaveForHome(int type) const { return inventory[type] + store[type]; }
	void SpendForHome(int type, int count);
	bool HomeComplete() const;
	void QueueBeat(StoryBeat beat);            // opens now, or as soon as the dialog on screen closes
	void PlaceNeighbors();                     // Dohyeon and Yeoul, who came to the tree before Hayul
	void TalkToNeighbor(int who);
	static const char* NeighborName(int who);

	// GameCombat.cpp: attacks, creatures, items and growth
	std::vector<EnemyActor*> LiveEnemies() const;
	std::vector<ItemActor*>  LiveItems() const;
	void Attack();
	void DamageEnemy(EnemyActor& e, float amount, const Vec3& push);
	void KillEnemy(EnemyActor& e);
	void DamagePlayer(float amount, const Vec3& push);
	void FallAsleep(const char* reason);
	void WakeAtSafePoint();
	void UpdateEnemies(float dt);
	void UpdateEnemy(EnemyActor& e, float dt);
	void KeepOutOfLight(Vec3& pos, float radius) const;   // a creature pushed back out of a lantern's light
	void UpdateItems();
	void UpdateCombatTimers(float dt);
	EnemyActor* SpawnEnemy(int type, int enemyLevel, const Vec3& pos, int chunkX, int chunkZ, int spawnIndex);
	ItemActor* DropItem(int type, const Vec3& pos, int chunkX, int chunkZ, int spawnIndex);
	void PickUp(ItemActor& item);
	void EquipWeapon();
	void UseHerb();
	void GrantXp(int amount);
	void AddPopup(const Vec3& pos, const char* text, const Vec3& color);
	void OpenStatPanel();
	void CloseStatPanel();
	void HandleStatPanelKey(unsigned char key);
	void ConfirmStats();
	void DrawCombatHud();
	void DrawPopups();
	void DrawWisps();
	void DrawStatPanel();

	// GameSurvival.cpp: water, food, warmth, the bag, the relic journal and the torch
	bool MenuOpen() const;
	void UpdateSurvival(float dt);
	void EatFood();
	void DrinkWater();
	void ToggleTorch();
	int  BagCount() const;
	bool BagHasRoom() const { return BagCount() < kBagCapacity; }
	void FindRelic(int id);
	int  UnfoundRelic();
	void AddInsight(int amount, const char* text);
	bool HandleMenuKey(unsigned char key);
	void DrawSurvivalHud();
	void DrawBagPanel();
	void DrawJournalPanel();

	void LoadOptions();
	void SaveOptions() const;
	void ApplyOptions();
	void SetPreset(int preset);
	void HandleOptionsKey(unsigned char key);
	void DrawOptionsPanel();
	void DrawFpsOverlay();

	// GameWeather.cpp: seasons turn with the days, and each season rolls its own weather
	void UpdateWeather(float dt);
	void AdvanceDay();
	void RollWeather();
	float WindAgainst(const Vec3& moveDir) const;   // [-1,1], positive when walking into the wind
	void DrawWeatherHud();

	// GameFx.cpp: screen moods that follow the story and the body, and the marks left on the ground
	void UpdateScreenFx(float dt);
	Vec3 ShakeOffset() const;
	bool PlayerHidden() const;   // a tree, roof or car stands between the eye and the player
	void UpdateFootprints();
	void DrawFootprints();
	void UpdateWorldFx(float dt);   // glints, embers, trails and dust thrown off by what is happening
	void UpdateWisps(float dt);
	void DrawLevelUpRings();

	// GameAnim.cpp: rigged bodies follow what their owners are doing
	void UpdateAnimations(float dt);
	void AnimatePlayer(float dt);
	void AnimateEnemy(EnemyActor& e, float dt);
	void AnimateGrandmother(float dt);
	void AnimateSleepers(float dt);
	void AnimateDeer(float dt);
	void AnimateNeighbors(float dt);
	void PlayPlayerAction(const char* clip);   // eat, pickup: a one-shot over whatever the body was doing
	void ShareSleeperPose(ChunkActor& chunk);  // the road's sleepers breathe with one shared pose

	// GameStory.cpp: the story the road tells as you grow, find and travel
	void UpdateStory(float dt);
	void Narrate(const char* speaker, const char* const* lines, int count);
	void UpdateNarration(float dt);
	void DrawNarration();
	const char* NarrationSpeaker() const;   // who says the line on screen now; empty when none
	bool StoryCalm() const;   // nothing hostile close or closing in
	bool MapOwned() const;    // found beside grandmother with her letter
	void DrawMapPanel();      // M: her map spread out, notes beside the places they speak of
	void DrawGrandmaMap(float x, float y, float w, float h, bool notes, int focus = -1);
	int  DrawWrapped(int x, int y, const char* text, int maxWidth, const Vec3& color);   // lines used
	void OpenBeat(const StoryBeat& beat);
	void MarkMemo(int memo);
	bool MemoMapRect(float& x, float& y, float& w, float& h) const;   // her map beside a note, at rest
	void DrawMemoMap();
	bool ReadingMemo() const;    // one of her map notes is open
	bool MemoPlace(int memo, Vec3& at, float& height) const;   // the thing a note names, where it stands and how tall
	Vec3 MemoViewPoint(int memo) const;   // where the view looks to centre that thing on screen
	bool PlaceInSight(const Vec3& at, float height) const;   // well inside the screen, not at its edge
	void OutlineMemoPlace(int memo, float strength);          // rings the place a note speaks of
	void NoteFirstFind(int itemType);
	void TellRelicOwner(int relic);
	Vec3 ShowcasePosition() const;   // where a find is held up: in front of everything, mid-screen
	void DrawRelicShowcase();
	bool FindRouteSleeper();  // a sleeper close by, unvisited and unguarded; sets the rest target
	void RestBySleeper();
	void DrawMemoPage(int x, int y, int maxWidth);

	Renderer* renderer = nullptr;
	int   level = LEVEL_VILLAGE;
	float levelTimer = 0.0f;      // seconds since the current level began

	SceneGraph   scene;
	Actor*       worldNode = nullptr;   // holds the current level, ahead of the player in draw order
	Actor*       levelNode = nullptr;
	PlayerActor* player = nullptr;
	CameraActor* camera = nullptr;

	WaterActor*   water = nullptr;
	PropActor*    letter = nullptr;
	ExitActor*    villageExit = nullptr;
	NpcActor*     grandmaNpc = nullptr;      // awake, the first day
	SleeperActor* grandmaSleeper = nullptr;  // asleep, the next morning
	DeerActor*    deer = nullptr;
	Vec3  homeDoor;
	Vec3  wellPos;
	Vec3  shedDoor;
	int   tutorial = TUT_WAKE;
	int   day = 1;
	int   radishes = 0;
	int   redBerries = 0;
	int   mealsBaseline = 0;
	bool  deerWatched = false;
	bool  riceEaten = false;
	bool  sleepAfterDialog = false;
	int   fragments = 0;
	int   fragmentGoal = 4;
	bool  letterOpen = false;
	float sleepTimer = -1.0f;         // >= 0 while the night passes
	float transitionTimer = -1.0f;    // >= 0 while leaving the village
	float dayCardTimer = -1.0f;       // >= 0 while "the next morning" shows
	float routeIntroHaze = 0.0f;      // the fog that lifts as Route 32 opens
	float sporeVisual = 0.55f;        // density of the drawn spore field
	int   targetSpot = SPOT_NONE;
	SleeperActor* targetSleeper = nullptr;
	ForageActor*  targetForage = nullptr;

	std::vector<std::string> dialogLines;
	std::string dialogSpeaker;
	size_t dialogIndex = 0;
	int    dialogNext = -1;           // tutorial step to enter once the dialog closes

	int   packHave[ITEM_TYPE_COUNT] = {};
	int   packTake[ITEM_TYPE_COUNT] = {};
	bool  packTorch = true;
	int   packCursor = 0;

	ChunkMap routeMap;
	int   playerChunkX = 0;
	int   playerChunkZ = 0;
	Actor*        chunkGroup = nullptr;
	Actor*        itemGroup = nullptr;
	Actor*        enemyGroup = nullptr;
	Actor*        forageGroup = nullptr;
	LanternActor* lantern = nullptr;
	Actor*        safePoint = nullptr;   // where the long sleep ends
	WaterActor*   reservoir = nullptr;   // hidden east of the road; the lantern deer knows the way
	bool  sheltered = false;     // the player stands in a lantern's light, where creatures do not follow
	bool  restLanternSeen = false;
	bool  busQueueSeen = false;  // the husks lined up at the bus stop, still waiting
	bool  reservoirFound = false;
	bool  townReached = false;
	float townCardTimer = -1.0f;
	float deerLeaveTimer = -1.0f;
	std::unordered_map<uint64_t, ChunkActor*> chunkActors;
	std::unordered_map<uint64_t, ChunkState> chunkStates;
	int   guide = GUIDE_TAKE_PIPE;
	int   guideBaseline = 0;      // counter value when the current guide step began

	// Past Ginkgo Town (GameHome.cpp). The jars hold a life that is enough at this much.
	static const int kStockFood = 15;
	static const int kStockWater = 10;
	static const int kStockHerb = 5;
	bool  townWallRead = false;
	bool  ridgeClimbed = false;
	bool  cityEntered = false;
	bool  parentsFound = false;
	bool  rootReached = false;
	bool  homeBuilt[kHomeProjects] = {};
	int   store[ITEM_TYPE_COUNT] = {};    // what is kept in the jars at home
	int   gardenReady = 0;                // radishes up and waiting to be pulled
	float gardenTimer = 0.0f;
	float wellTimer = 0.0f;
	bool  settled = false;                // the ending has been seen
	float settleCardTimer = -1.0f;        // >= 0 while the closing title shows; -2 while it waits for the words
	int   lateSpot = -1;                  // what the interaction key would use past the town
	bool  deerLate = false;               // the deer walking ahead is bound for the tree, not the water
	float treeMoteTimer = 0.0f;
	PropActor*    homeProps[HOME_SPOT_COUNT] = {};
	PropActor*    gardenCrops[6] = {};
	LanternActor* homeLantern = nullptr;
	static const int kNeighborCount = 2;           // Dohyeon, Yeoul
	NpcActor* neighborNpc[kNeighborCount] = {};
	bool  neighborMet[kNeighborCount] = {};
	int   neighborTalks[kNeighborCount] = {};      // talks since the first, to turn through what they say
	bool  neighborsSeen = false;

	CharacterStats stats;
	float health = 76.0f;
	int   inventory[ITEM_TYPE_COUNT] = {};

	static const int kBagCapacity = 8;
	float waterMeter = 60.0f;     // [0,100]
	float foodMeter = 35.0f;      // [0,100]
	float warmthMeter = 100.0f;   // [0,100], only once warmthActive
	bool  survivalShown = false;  // the tutorial brings the meters in at the well
	bool  warmthActive = false;
	float routeTime = 0.0f;       // seconds spent on Route 32
	int   natureInsight = 0;
	bool  relicFound[kRelicCount] = {};
	bool  torchOwned = false;
	bool  torchOn = false;
	float torchBattery = 100.0f;
	bool  bagOpen = false;
	bool  journalOpen = false;
	int   journalPage = 0;        // 0 the relics, 1 grandmother's map notes
	bool  mapOpen = false;        // grandmother's map spread out (M)
	bool  mapUnread = false;      // a note has come up on it since it was last opened
	bool  packOpen = false;       // the departure: choosing what goes in the bag
	int   mealsEaten = 0;
	bool  enemySeen[ENEMY_TYPE_COUNT] = {};
	float deathTimer = -1.0f;     // >= 0 while sinking into the long sleep
	float levelUpTimer = 0.0f;
	float hitStop = 0.0f;         // > 0 while a landed blow freezes the world
	Rng   combatRng{ 0x5EED5EEDULL };
	Rng   fxRng{ 0xF0CA11ULL };      // looks only; never touches a gameplay roll

	std::vector<Popup> popups;
	std::vector<Wisp>  wisps;

	bool  statPanelOpen = false;
	int   statCursor = 0;
	int   pendingPoints[STAT_COUNT] = { 0, 0, 0, 0 };

	int   kills[ENEMY_TYPE_COUNT] = { 0, 0, 0 };
	int   pickups[ITEM_TYPE_COUNT] = {};
	int   herbsUsed = 0;
	int   statConfirmations = 0;

	float time = 0.0f;
	int   tick = 0;               // updates run while the world was not paused
	float timeOfDay = 0.27f;      // [0,1), 0 is midnight
	float dayLength = 240.0f;     // real seconds per in-game day
	float timeScale = 1.0f;
	float sporeExposure = 0.15f;  // [0,1]

	Rng   weatherRng{ 0 };        // reseeded from the clock, so every run starts in its own season
	int   season = SEASON_SPRING;
	int   seasonDay = 0;          // whole days spent in the current season
	int   weather = WEATHER_CLEAR;
	float weatherTimer = 60.0f;   // game seconds until the next roll
	float rainLevel = 0.0f;       // [0,1], each eases toward the current weather
	float snowLevel = 0.0f;
	float windLevel = 0.0f;
	float windAngle = 0.0f;       // direction the wind blows toward, on the XZ plane
	float windDriftX = 0.0f;      // how far the wind has carried the air, for the particles
	float windDriftZ = 0.0f;
	float snowCover = 0.0f;       // [0,1] snow lying on the ground
	float wetness = 0.0f;         // [0,1] rain soaked into the ground
	float seasonWeight[SEASON_COUNT] = { 1.0f, 0.0f, 0.0f, 0.0f };   // eases between seasons

	ScreenFx screenFx;
	float hurtFlash = 0.0f;       // [0,1] the red at the edges from the last blow, fading
	float shake = 0.0f;           // [0,1] camera shake from the last blow
	float cutaway = 0.0f;         // [0,1] how open the view through occluders is
	float viewHeight = 20.0f;     // world height of the screen the camera eases toward
	std::vector<Footprint> footprints;
	Vec3  lastPrintPos;
	bool  printLeft = false;
	Vec3  swingTip;               // where the weapon's end was last update, for the trail behind it
	bool  swingTipValid = false;

	Animator roadSleeperAnim;     // one breathing pose for every sleeper lying by the road

	std::vector<StoryBeat> storyQueue;
	float storyCalm = 0.0f;       // seconds the road has been quiet
	std::vector<NarrationLine> narration;   // said over play, front first
	float narrationTimer = 0.0f;  // seconds the front line has shown
	bool  memoSeen[MEMO_COUNT] = {};
	int   outlineMemo = -1;       // the note whose place is ringed on screen while it is read
	float outlineStrength = 0.0f;
	int   memoMap = -1;           // the note whose map is held up beside it, until it is put away
	bool  memoMapLeft = true;     // on the side of the screen away from the place
	int   panMemo = -1;           // the note whose place the view has turned to, until it is back on Hayul
	float placePan = 0.0f;        // [0,1] how far the view has gone from Hayul to that place
	int   showcaseRelic = -1;     // the find held up mid-screen while its owner is told
	float showcaseFade = 0.0f;    // [0,1] gathers out of light as the dialog opens, comes apart as it closes
	int   nextRecall = 4;         // the level whose memory comes back next
	bool  firstFound[ITEM_TYPE_COUNT] = {};
	int   relicsTold = 0;         // relic count milestones already spoken
	int   roadDreams = 0;         // dreams seen beside the road's sleepers
	int   restChunkX = 0;
	int   restChunkZ = 0;
	int   restIndex = -1;         // the sleeper the interaction key would rest beside

	PerfStats perf;
	bool  optionsOpen = false;
	int   optionsCursor = 0;
	int   optionChoice[OPT_COUNT] = { PRESET_HIGH, 4, 2, 2, 1, 3, 1, 0, 1 };   // SSAO on, vsync on, FPS hidden

	std::string message;
	float messageTimer = 0.0f;
	std::string prompt;
	bool  quit = false;

	// Where this frame's HUD columns end, so each piece goes under the last one instead of on top of it.
	float hudLeftBottom = 0.0f;
	float hudRightBottom = 0.0f;
	float hudObjectiveRight = 0.0f;
};
