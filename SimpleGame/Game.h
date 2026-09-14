#pragma once

#include <vector>
#include <string>

#include "Math3D.h"
#include "Renderer.h"
#include "ChunkMap.h"

const float kPlayerRadius = 0.38f;
const float kInteractRange = 2.3f;

const Vec3 kHudPanel(0.03f, 0.05f, 0.05f);
const Vec3 kHudInk(0.90f, 0.93f, 0.90f);
const Vec3 kHudDim(0.62f, 0.68f, 0.65f);
const Vec3 kHudAccent(0.60f, 0.92f, 0.80f);

struct Prop
{
	int   model = 0;
	Vec3  pos;              // base centre, sitting on the ground
	Vec3  scale{ 1.0f, 1.0f, 1.0f };
	float yaw = 0.0f;
	float emissive = 0.0f;
	float halfX = 0.0f;     // collision footprint; zero takes no part in collision
	float halfZ = 0.0f;
};

struct Sleeper
{
	Vec3  pos;
	float yaw = 0.0f;
	bool  isGrandma = false;
	bool  visited = false;
	float phase = 0.0f;     // breathing offset
};

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

class Game
{
public:
	explicit Game(Renderer* r);

	void Update(float dt, const bool* keys);
	void Render();

	// Edge-triggered: fires once per physical press.
	void OnKeyDown(unsigned char key);

	// Demo aid: jump straight to the level after the tutorial.
	void SkipToRoute();

	bool WantsQuit() const { return quit; }

private:
	// Game.cpp: shared by every level
	void UpdatePlayer(float dt, const bool* keys);
	void UpdateCamera(float dt);
	void SetupCamera();
	void DrawPlayer();
	void DrawObjective(const char* text);
	void DrawCommonHud(const char* help);
	void DrawTitleCard(const char* title, const char* subtitle);
	void ShowMessage(const char* text, float seconds);

	// GameVillage.cpp: Mulangae Village, the tutorial
	void BuildVillage();
	void AddProp(int model, const Vec3& pos, const Vec3& scale, float yaw, float halfX, float halfZ);
	void AddHouse(const Vec3& pos, float w, float h, float d, float yaw, int model);
	void AddTree(const Vec3& pos, float scale);
	void AddFence(const Vec3& from, const Vec3& to);
	void UpdateVillage(float dt, const bool* keys);
	void ResolveVillageCollisions();
	void UpdateInteractionTarget();
	void TryInteract();
	void DrawVillage();
	void DrawSleepers();
	void DrawVillageHud();
	void DrawLetterPanel();
	void DrawEndingCard();
	const char* ObjectiveText() const;

	// GameRoute.cpp: Route 32, grown chunk by chunk from the world seed
	void StartRoute();
	void UpdateRoute(float dt, const bool* keys);
	void StreamChunks();
	void ResolveRouteCollisions();
	void DrawRoute();
	void DrawRouteHud();

	Renderer* renderer = nullptr;
	int   level = LEVEL_VILLAGE;
	float levelTimer = 0.0f;      // seconds since the current level began

	std::vector<Prop>    props;
	std::vector<Sleeper> sleepers;
	Vec3  waterCenter{ -15.0f, 0.03f, -14.0f };
	float waterSizeX = 22.0f;
	float waterSizeZ = 18.0f;
	Vec3  letterPos;
	int   stage = QUEST_FIND_GRANDMOTHER;
	int   fragments = 0;
	int   fragmentGoal = 3;
	bool  letterOpen = false;
	bool  letterFound = false;
	float endingTimer = -1.0f;
	int   targetSleeper = -1;     // index into sleepers, -1 if none
	bool  targetLetter = false;

	ChunkMap routeMap;
	int   playerChunkX = 0;
	int   playerChunkZ = 0;

	Vec3  playerPos{ 2.0f, 0.0f, 1.5f };
	float playerYaw = kPi;
	float walkPhase = 0.0f;
	float rollTimer = 0.0f;       // > 0 while rolling
	float rollCooldown = 0.0f;
	float rollAngle = 0.0f;
	Vec3  rollDir;

	Vec3  camTarget;
	float camYaw = DegToRad(45.0f);
	float camPitch = DegToRad(30.0f);
	float camDistance = 55.0f;
	float orthoHeight = 20.0f;

	float time = 0.0f;
	float timeOfDay = 0.27f;      // [0,1), 0 is midnight
	float dayLength = 240.0f;     // real seconds per in-game day
	float timeScale = 1.0f;
	float sporeExposure = 0.15f;  // [0,1]

	std::string message;
	float messageTimer = 0.0f;
	std::string prompt;
	bool  quit = false;
};
