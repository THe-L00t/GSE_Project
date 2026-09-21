#pragma once

#include <cstdint>

#include "Actor.h"
#include "ChunkMap.h"
#include "Renderer.h"
#include "Rpg.h"

const float kPlayerRadius = 0.38f;
const float kSwingTime = 0.22f;
const float kDyingTime = 0.6f;
const int   kChunkDrawRadius = 1;   // chunks drawn around the camera; covers the orthographic view

enum EnemyState
{
	ENEMY_IDLE,
	ENEMY_CHASE,
	ENEMY_WINDUP,
	ENEMY_STRIKE,
	ENEMY_RECOVER,
	ENEMY_DYING
};

class CameraActor : public Actor
{
public:
	CameraActor();

	// Orthographic quarter view of the actor's position.
	void Apply(Renderer* renderer) const;

	float elevation = DegToRad(30.0f);
	float distance = 55.0f;
	float orthoHeight = 20.0f;
};

class WeaponActor : public Actor
{
public:
	WeaponActor();

	void Draw(const DrawContext& dc) const override;
	void SetSwing(float swingTimer);

	int   model = 0;
	float power = 6.0f;     // added to attack power
	float reach = 2.1f;
};

class PlayerActor : public Actor
{
public:
	PlayerActor();

	void Draw(const DrawContext& dc) const override;
	void UpdatePose(float deathTimer);

	WeaponActor* weapon = nullptr;
	float walkPhase = 0.0f;
	float rollTimer = 0.0f;       // > 0 while rolling
	float rollCooldown = 0.0f;
	float rollAngle = 0.0f;
	Vec3  rollDir;
	float attackTimer = 0.0f;     // cooldown until the next swing
	float swingTimer = 0.0f;      // > 0 while the swing plays
	float hurtTimer = 0.0f;       // invulnerable while > 0
	float flash = 0.0f;
};

// Follows the view, so it never runs out.
class GroundActor : public Actor
{
public:
	GroundActor(float size, const GroundParams& groundParams);

	void Draw(const DrawContext& dc) const override;

	float        extent = 0.0f;
	GroundParams params;
};

class WaterActor : public Actor
{
public:
	WaterActor(float width, float depth);

	void Draw(const DrawContext& dc) const override;

	// Distance on the XZ plane from the shore; zero over the water.
	float ShoreDistance(const Vec3& pos) const;

	float sizeX = 0.0f;
	float sizeZ = 0.0f;
	float clearRange = 4.5f;      // spore exposure falls this close to the shore
};

class PropActor : public Actor
{
public:
	PropActor(int actorKind, int propModel);

	void Draw(const DrawContext& dc) const override;

	int   model = 0;
	float phase = 0.0f;           // offsets shader animation so neighbours do not move in lockstep
	float emissive = 0.0f;
	float shadowRadius = 0.0f;    // zero casts no shadow
};

class SleeperActor : public Actor
{
public:
	SleeperActor(bool grandma, float breathPhase);

	void Draw(const DrawContext& dc) const override;

	bool       isGrandma = false;
	bool       visited = false;
	float      phase = 0.0f;      // breathing offset
	PropActor* mote = nullptr;    // the dream fragment, until it is taken
};

// The player leaves the level once past this line, heading +Z.
class ExitActor : public Actor
{
public:
	ExitActor();

	bool Contains(const Vec3& pos) const { return pos.z > WorldPosition().z; }
};

class LanternActor : public PropActor
{
public:
	LanternActor();

	float zoneRadius = 4.5f;      // clears the spores and mends wounds within
};

// Ground and props of one generated chunk.
class ChunkActor : public Actor
{
public:
	explicit ChunkActor(const Chunk& chunk);

	bool IsDrawn(const DrawContext& dc) const override;
	void Draw(const DrawContext& dc) const override;

	int      cx = 0;
	int      cz = 0;
	int      stage = 1;
	uint64_t hash = 0;
	int      lastTick = 0;           // last update that looked the chunk up
	bool     hasNeighborStages = false;
	float    neighborStage[4] = { 0.0f, 0.0f, 0.0f, 0.0f };   // -x, +x, -z, +z
};

class EnemyActor : public Actor
{
public:
	EnemyActor(int enemyType, int enemyLevel);

	void Draw(const DrawContext& dc) const override;

	int   type = ENEMY_SPORE_MITE;
	int   level = 1;
	Vec3  home;
	Vec3  moveTarget;
	Vec3  strikeDir;
	Vec3  knockback;
	float health = 1.0f;
	float maxHealth = 1.0f;
	int   state = ENEMY_IDLE;
	float stateTimer = 0.0f;
	bool  strikeHit = false;
	bool  alive = true;
	float flash = 0.0f;
	int   chunkX = 0;
	int   chunkZ = 0;
	int   spawnIndex = -1;  // index into the owning chunk's spawns, -1 when scripted
};

class ItemActor : public Actor
{
public:
	explicit ItemActor(int itemType);

	void Draw(const DrawContext& dc) const override;

	int   type = ITEM_HERB;
	float phase = 0.0f;
	int   chunkX = 0;
	int   chunkZ = 0;
	int   spawnIndex = -1;  // index into the owning chunk's items, -1 for drops and scripted items
};
