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

enum QuestStage
{
	QUEST_FIND_GRANDMOTHER = 0,
	QUEST_READ_LETTER,
	QUEST_LEAVE_VILLAGE,
	QUEST_DONE
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
	void AddSleeper(const Vec3& pos, float yaw, bool grandma, float phase);
	void UpdateVillage(float dt, const bool* keys);
	void ResolveVillageCollisions();
	void UpdateInteractionTarget();
	void TryInteract();
	void DrawVillageHud();
	void DrawLetterPanel();
	void DrawEndingCard();
	const char* ObjectiveText() const;

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
	void DropItem(int type, const Vec3& pos, int chunkX, int chunkZ, int spawnIndex);
	void PickUp(ItemActor& item);
	void EquipWeapon();
	void UseHerb();
	void UseWater();
	void GrantXp(int amount);
	void AddPopup(const Vec3& pos, const char* text, const Vec3& color);
	void OpenStatPanel();
	void CloseStatPanel();
	void HandleStatPanelKey(unsigned char key);
	void ConfirmStats();
	void DrawCombatHud();
	void DrawPopups();
	void DrawStatPanel();

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
	int   stage = QUEST_FIND_GRANDMOTHER;
	int   fragments = 0;
	int   fragmentGoal = 3;
	bool  letterOpen = false;
	bool  letterFound = false;
	float endingTimer = -1.0f;
	SleeperActor* targetSleeper = nullptr;
	bool  targetLetter = false;

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
	int   inventory[ITEM_TYPE_COUNT] = { 0, 0, 0, 0 };
	float deathTimer = -1.0f;     // >= 0 while sinking into the long sleep
	float levelUpTimer = 0.0f;
	Rng   combatRng{ 0x5EED5EEDULL };

	std::vector<Popup> popups;

	bool  statPanelOpen = false;
	int   statCursor = 0;
	int   pendingPoints[STAT_COUNT] = { 0, 0, 0, 0 };

	int   kills[ENEMY_TYPE_COUNT] = { 0, 0, 0 };
	int   pickups[ITEM_TYPE_COUNT] = { 0, 0, 0, 0 };
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
