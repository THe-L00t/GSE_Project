#pragma once

#include <cstdint>

#include "Actor.h"
#include "ChunkMap.h"
#include "Renderer.h"
#include "Rpg.h"

const float kPlayerRadius = 0.38f;
const float kSwingTime = 0.22f;
const float kRollTime = 0.28f;
const float kDyingTime = 0.9f;
const int   kChunkViewRange = 3;    // chunks around the camera that are tested against the view
const float kChunkPropReach = 7.0f; // furthest any chunk prop spreads past its origin
const float kChunkPropTop = 14.0f;  // tallest any chunk prop stands

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

	// The rigged body (GameAnim.cpp). A one-shot action such as eating plays over the walk until it ends.
	Animator    anim;
	Vec3        lastPos;
	float       animSpeed = 0.0f;   // ground speed, smoothed, that picks and paces the walk
	float       lastHurt = 0.0f;
	const char* action = nullptr;
	bool        actionFresh = false;
	int         gripBone = -2;    // -2 until looked up
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

// A pond: the oval with its wandering shore (PondShore in Mesh.h), turned by the actor's yaw. Around it the
// ground shows the band of cracked mud the drought left, and mist lies low on it.
class WaterActor : public Actor
{
public:
	WaterActor(float width, float depth, float dryBand, float mistAmount);

	void Draw(const DrawContext& dc) const override;

	// Signed distance on the XZ plane from the shore: negative over the water.
	float Shore(const Vec3& pos) const;
	// Distance on the XZ plane from the shore; zero over the water.
	float ShoreDistance(const Vec3& pos) const;
	// Keeps a circle of this radius on dry land.
	void  PushOut(Vec3& pos, float radius) const;
	// A point offset out from the shore in a world direction (radians from +x toward +z) from the middle.
	Vec3  EdgePoint(float angle, float offset) const;

	float sizeX = 0.0f;
	float sizeZ = 0.0f;
	float dry = 0.0f;             // width of the mud band where the water used to reach
	float mist = 0.0f;            // how thick the mist lying on it is
	float clearRange = 4.5f;      // spore exposure falls this close to the shore
	float outline = 0.0f;         // [0,1] ringed on screen in outlineColor while a note speaks of it
	Vec3  outlineColor;
};

class PropActor : public Actor
{
public:
	PropActor(int actorKind, int propModel);

	bool IsDrawn(const DrawContext& dc) const override;
	void Draw(const DrawContext& dc) const override;

	int   model = 0;
	float phase = 0.0f;           // offsets shader animation so neighbours do not move in lockstep
	float emissive = 0.0f;
	float shadowRadius = 0.0f;    // zero casts no shadow
	float moss = 0.0f;            // how far the road's moss has grown over it
	const std::vector<Mat4>* skin = nullptr;   // a shared pose, for rigged models placed as props
	float outline = 0.0f;         // [0,1] ringed on screen in outlineColor while a note speaks of it
	Vec3  outlineColor;
};

class SleeperActor : public Actor
{
public:
	SleeperActor(bool grandma, float breathPhase);

	void Draw(const DrawContext& dc) const override;

	bool       isGrandma = false;
	bool       visited = false;
	int        neighbor = -1;     // which village neighbour, for their name and dream
	float      phase = 0.0f;      // breathing offset
	PropActor* mote = nullptr;    // the dream fragment, until it is taken
	Animator   anim;
};

// Grandmother while she is still awake: she stands, breathes and talks.
class NpcActor : public Actor
{
public:
	NpcActor();

	void Draw(const DrawContext& dc) const override;

	int   model = 0;
	float phase = 0.0f;
	Animator anim;
	float yawnTimer = 6.0f;       // the spores make her yawn now and then
};

enum ForageKind
{
	FORAGE_RADISH,
	FORAGE_RED_BERRY,
	FORAGE_PALE_BERRY
};

// Something growing that can be picked. A picked berry bush stays as a plain bush.
class ForageActor : public PropActor
{
public:
	explicit ForageActor(int forageKind);

	int  forage = FORAGE_RADISH;
	bool picked = false;

	// Route 32 bushes only: which chunk bush this is, and its share of the season.
	int   chunkX = 0;
	int   chunkZ = 0;
	int   spawnIndex = -1;
	float bearing = 0.0f;
};

// Its antlers glow; it walks toward a goal and, when leading, waits for the player.
class DeerActor : public Actor
{
public:
	DeerActor();

	void Draw(const DrawContext& dc) const override;

	Vec3  goal;
	float speed = 1.3f;
	bool  leading = false;
	bool  arrived = false;
	bool  carriesGrandma = false;  // after she sleeps, one more warm light rides on its antlers
	float stepPhase = 0.0f;
	Vec3  trailFrom;               // where the antler light last dropped a mote behind it
	bool  trailStarted = false;
	Animator anim;
	Vec3  lastPos;
	float animSpeed = 0.0f;
	int   antlerBone = -2;         // -2 until looked up
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
	float    modernity = 1.0f;
	uint64_t hash = 0;
	int      lastTick = 0;           // last update that looked the chunk up
	bool     inView = false;         // set each frame by the game from the camera
	float    reach = 0.0f;           // how far its props spread past the chunk square
	float    top = 0.0f;             // how tall its tallest prop stands
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
	bool  provoked = false;   // struck or startled: gives chase much further
	bool  holdsPlace = false; // stands at home facing homeYaw instead of wandering
	float homeYaw = 0.0f;
	float flash = 0.0f;
	int   chunkX = 0;
	int   chunkZ = 0;
	int   spawnIndex = -1;  // index into the owning chunk's spawns, -1 when scripted
	int   swarm = -1;       // index into the owning chunk's swarms
	bool  guarding = false; // wakes when anyone comes near guardPos, not only near itself
	Vec3  guardPos;
	Animator anim;
	Vec3  lastPos;
	float animSpeed = 0.0f;
	bool  asleep = false;   // a guard lying by its sleeper; it plays its waking when that ends
	bool  waking = false;
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
	int   relicId = -1;     // the journal entry a relic fills; -1 picks one not yet found
	bool  landmark = false; // placed by the level; never forgotten when left behind
	float sparkle = 0.0f;   // a relic's glint this frame, set by the game
};

// The model a relic shows: its own once it knows which entry it fills.
int RelicModel(int relicId);
