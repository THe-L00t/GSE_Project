#pragma once

#include <vector>
#include <string>

#include "Math3D.h"
#include "Renderer.h"

// A static piece of the village. Everything is built from boxes.
struct Prop
{
	Vec3  pos;      // base centre, sitting on the ground
	Vec3  size;     // width, height, depth
	float yaw;
	Vec3  color;
	float emissive;
	bool  solid;    // takes part in collision

	Prop() : yaw(0.0f), emissive(0.0f), solid(true) {}
};

// A villager who fell asleep when the spores arrived.
struct Sleeper
{
	Vec3  pos;
	float yaw;
	bool  isGrandma;
	bool  visited;
	float phase;    // breathing offset

	Sleeper() : yaw(0.0f), isGrandma(false), visited(false), phase(0.0f) {}
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
	explicit Game(Renderer* renderer);

	void Update(float dt, const bool* keys);
	void Render();

	// Edge-triggered input (fires once per physical press).
	void OnKeyDown(unsigned char key);

	bool WantsQuit() const { return m_Quit; }

private:
	void BuildWorld();
	void AddHouse(const Vec3& pos, float w, float h, float d, float yaw,
				  const Vec3& wall, const Vec3& roof);
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

	Renderer* m_Renderer;

	// ---- world ----
	std::vector<Prop>    m_Props;
	std::vector<Sleeper> m_Sleepers;
	Vec3  m_WaterCenter;
	float m_WaterSizeX;
	float m_WaterSizeZ;
	Vec3  m_LetterPos;

	// ---- player ----
	Vec3  m_PlayerPos;
	Vec3  m_PlayerVel;
	float m_PlayerYaw;
	float m_WalkPhase;
	float m_RollTimer;      // > 0 while rolling
	float m_RollCooldown;
	float m_RollAngle;
	Vec3  m_RollDir;

	// ---- camera ----
	Vec3  m_CamTarget;
	float m_CamYaw;
	float m_CamPitch;
	float m_CamDistance;
	float m_OrthoHeight;

	// ---- time and atmosphere ----
	float m_Time;           // seconds since start
	float m_TimeOfDay;      // [0,1), 0 is midnight
	float m_DayLength;      // real seconds per in-game day
	float m_TimeScale;
	float m_SporeExposure;  // [0,1]

	// ---- quest ----
	int   m_Stage;
	int   m_Fragments;
	int   m_FragmentGoal;
	bool  m_LetterOpen;
	bool  m_LetterFound;

	// ---- presentation ----
	std::string m_Message;
	float m_MessageTimer;
	float m_TitleTimer;
	float m_EndingTimer;
	bool  m_Quit;

	// ---- interaction ----
	int   m_TargetSleeper;   // index into m_Sleepers, -1 if none
	bool  m_TargetLetter;
	std::string m_Prompt;
};
