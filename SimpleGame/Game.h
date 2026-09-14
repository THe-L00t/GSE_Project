#pragma once

#include <vector>
#include <string>

#include "Math3D.h"
#include "Renderer.h"

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

class Game
{
public:
	explicit Game(Renderer* r);

	void Update(float dt, const bool* keys);
	void Render();

	// Edge-triggered: fires once per physical press.
	void OnKeyDown(unsigned char key);

	bool WantsQuit() const { return quit; }

private:
	void BuildWorld();
	void AddProp(int model, const Vec3& pos, const Vec3& scale, float yaw, float halfX, float halfZ);
	void AddHouse(const Vec3& pos, float w, float h, float d, float yaw, int model);
	void AddTree(const Vec3& pos, float scale);
	void AddFence(const Vec3& from, const Vec3& to);

	void UpdatePlayer(float dt, const bool* keys);
	void ResolveCollisions();
	void UpdateCamera(float dt);
	void UpdateWorldState(float dt);
	void UpdateInteractionTarget();

	void TryInteract();

	SceneEnv MakeEnv() const;
	Vec3 SkyColorNow() const;
	float SporeDensityNow() const;

	void DrawWorld();
	void DrawPlayer();
	void DrawSleepers();
	void DrawHUD();
	void DrawLetterPanel();
	void DrawTitleCards();

	void ShowMessage(const char* text, float seconds);
	const char* ObjectiveText() const;

	Renderer* renderer = nullptr;

	std::vector<Prop>    props;
	std::vector<Sleeper> sleepers;
	Vec3  waterCenter{ -15.0f, 0.03f, -14.0f };
	float waterSizeX = 22.0f;
	float waterSizeZ = 18.0f;
	Vec3  letterPos;

	Vec3  playerPos{ 2.0f, 0.0f, 1.5f };
	Vec3  playerVel;
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

	int   stage = QUEST_FIND_GRANDMOTHER;
	int   fragments = 0;
	int   fragmentGoal = 3;
	bool  letterOpen = false;
	bool  letterFound = false;

	std::string message;
	float messageTimer = 0.0f;
	float titleTimer = 0.0f;
	float endingTimer = -1.0f;
	bool  quit = false;

	int   targetSleeper = -1;     // index into sleepers, -1 if none
	bool  targetLetter = false;
	std::string prompt;
};
