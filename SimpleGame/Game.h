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

// The opening of Route 32 walks through fighting, levelling up and assigning stats.
enum RouteGuide
{
	GUIDE_TAKE_PIPE,
	GUIDE_FIGHT_MITES,
	GUIDE_ASSIGN_STATS,
	GUIDE_FIGHT_BOAR,
	GUIDE_ASSIGN_AGAIN,
	GUIDE_EXPLORE
};

struct Popup
{
	Vec3  pos;
	Vec3  color;
	float timer = 0.0f;
	char  text[16] = "";
};

// What has happened to a generated chunk's creatures and finds since it was first visited.
struct ChunkState
{
	bool  active = false;
	int   cx = 0;
	int   cz = 0;
	std::vector<float> respawnAt;   // game time when each spawn may appear again
	std::vector<bool>  itemTaken;
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

	// Demo aid: jump straight to the level after the tutorial.
	void SkipToRoute();

	bool WantsQuit() const { return quit; }

private:
	// Game.cpp: shared by every level
	void UpdatePlayer(float dt, const bool* keys);
	void UpdateCamera(float dt);
	void DrawObjective(const char* text);
	void DrawCommonHud(const char* help);
	void DrawTitleCard(const char* title, const char* subtitle);
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
	void Say(const char* const* lines, int count, int next);
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
	ChunkActor* EnsureChunk(int cx, int cz);
	void PrepareChunkView();
	void ResolveRouteCollisions(Vec3& pos, float radius);
	void DrawRouteHud();
	void DrawGuideCard();

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
	void UpdateItems();
	void UpdateCombatTimers(float dt);
	void SpawnEnemy(int type, int enemyLevel, const Vec3& pos, int chunkX, int chunkZ, int spawnIndex);
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
	int   fragmentGoal = 3;
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
	LanternActor* lantern = nullptr;
	Actor*        safePoint = nullptr;   // where the long sleep ends
	std::unordered_map<uint64_t, ChunkActor*> chunkActors;
	std::unordered_map<uint64_t, ChunkState> chunkStates;
	int   guide = GUIDE_TAKE_PIPE;
	int   guideBaseline = 0;      // counter value when the current guide step began

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
	bool  packOpen = false;       // the departure: choosing what goes in the bag
	int   mealsEaten = 0;
	bool  enemySeen[ENEMY_TYPE_COUNT] = {};
	float deathTimer = -1.0f;     // >= 0 while sinking into the long sleep
	float levelUpTimer = 0.0f;
	Rng   combatRng{ 0x5EED5EEDULL };

	std::vector<Popup> popups;

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

	std::string message;
	float messageTimer = 0.0f;
	std::string prompt;
	bool  quit = false;
};
