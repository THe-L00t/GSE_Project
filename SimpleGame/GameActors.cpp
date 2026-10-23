#include "stdafx.h"
#include "GameActors.h"

#include <cstdlib>

#include "Mesh.h"
#include "Models.h"

namespace
{
	const float kRollPivot = 0.62f;
}

CameraActor::CameraActor()
	: Actor(ACTOR_CAMERA)
{
	SetYaw(DegToRad(45.0f));
}

void CameraActor::Apply(Renderer* renderer) const
{
	const Vec3& target = Position();
	Vec3 dir(cosf(elevation) * sinf(Yaw()), sinf(elevation), cosf(elevation) * cosf(Yaw()));
	Vec3 eye = target + dir * distance;

	float aspect = (float)renderer->GetWidth() / (float)Maxf((float)renderer->GetHeight(), 1.0f);
	float hh = orthoHeight * 0.5f;
	float hw = hh * aspect;

	Mat4 view = MatLookAt(eye, target, Vec3(0.0f, 1.0f, 0.0f));
	Mat4 proj = MatOrtho(-hw, hw, -hh, hh, 0.1f, 400.0f);
	float pixelsPerUnit = (float)renderer->GetHeight() / orthoHeight;

	renderer->SetCamera(view, proj, eye, pixelsPerUnit);
}

WeaponActor::WeaponActor()
	: Actor(ACTOR_WEAPON)
{
	model = MODEL_PIPE;
	SetPosition(Vec3(0.36f, 0.62f, 0.10f));
	SetSwing(0.0f);
}

void WeaponActor::SetSwing(float swingTimer)
{
	// At rest the pipe hangs forward from the right hand; a swing sweeps it from over the shoulder.
	float angle = 2.7f;
	if (swingTimer > 0.0f)
	{
		float t = 1.0f - swingTimer / kSwingTime;
		t = 1.0f - (1.0f - t) * (1.0f - t);
		angle = Lerpf(-0.6f, 2.4f, t);
	}
	SetPitch(angle, Vec3());
}

void WeaponActor::Draw(const DrawContext& dc) const
{
	dc.renderer->DrawModel(model, WorldMatrix(), DrawParams());
}

PlayerActor::PlayerActor()
	: Actor(ACTOR_PLAYER)
{
	collider.shape = COLLIDER_CIRCLE;
	collider.radius = kPlayerRadius;
}

void PlayerActor::UpdatePose(float deathTimer)
{
	// A rigged body rolls and falls by itself, and the pipe rides in its hand.
	if (anim.Bound())
	{
		SetPitch(0.0f, Vec3());
		if (gripBone == -2) gripBone = anim.GetSkeleton()->FindBone("grip_R");
		if (weapon && gripBone >= 0)
			weapon->SetLocalMatrix(Mul(anim.BoneMatrix(gripBone), MatRotateX(-0.5f * kPi)));
		else if (weapon)
			weapon->SetSwing(swingTimer);
		return;
	}

	// Roll pivots around the waist so the tumble reads from a quarter view.
	if (rollTimer > 0.0f)
		SetPitch(rollAngle, Vec3(0.0f, kRollPivot, 0.0f));
	else if (deathTimer >= 0.0f)
		SetPitch(-1.45f * Saturatef(deathTimer * 1.5f), Vec3());
	else
		SetPitch(0.0f, Vec3());

	if (weapon) weapon->SetSwing(swingTimer);
}

void PlayerActor::Draw(const DrawContext& dc) const
{
	// Lit.vs turns the walk phase into the bob; a roll holds it still.
	DrawParams params;
	params.phase = (rollTimer > 0.0f || anim.Bound()) ? 0.0f : walkPhase;
	params.flash = flash;
	if (hurtTimer > 0.0f && fmodf(hurtTimer, 0.16f) < 0.08f)
		params.flash = Maxf(params.flash, 0.35f);
	if (anim.Bound()) params.skin = &anim.SkinMatrices();

	dc.renderer->DrawShadow(Position(), 0.45f);
	dc.renderer->DrawModel(MODEL_PLAYER, WorldMatrix(), params);
}

GroundActor::GroundActor(float size, const GroundParams& groundParams)
	: Actor(ACTOR_GROUND), extent(size), params(groundParams)
{
	layer = LAYER_GROUND;
}

void GroundActor::Draw(const DrawContext& dc) const
{
	dc.renderer->DrawGround(dc.viewCenter, extent, params);
}

WaterActor::WaterActor(float width, float depth, float dryBand, float mistAmount)
	: Actor(ACTOR_WATER), sizeX(width), sizeZ(depth), dry(dryBand), mist(mistAmount)
{
	// The shore is not a collider shape; the levels keep walkers out of it with PushOut.
	layer = LAYER_WATER;
}

void WaterActor::Draw(const DrawContext& dc) const
{
	Vec3 center = WorldPosition();
	float turn = Yaw();

	// The ground's mud band and the water's own shading both read the shore from here.
	ShoreParams shore;
	shore.center = center;
	shore.halfX = sizeX * 0.5f;
	shore.halfZ = sizeZ * 0.5f;
	shore.yaw = turn;
	shore.dry = dry;
	shore.mist = mist;
	dc.renderer->SetShore(shore);

	dc.renderer->DrawWater(center, turn, sizeX, sizeZ);
	if (mist > 0.0f) dc.renderer->DrawWaterMist(center, turn, sizeX, sizeZ);
	if (outline > 0.0f)
	{
		Mat4 model = Mul(Mul(MatTranslate(center), MatRotateY(turn)), MatScale(Vec3(sizeX, 1.0f, sizeZ)));
		dc.renderer->OutlineMesh(MODEL_WATER, model, outlineColor, outline);
	}
}

float WaterActor::Shore(const Vec3& pos) const
{
	Vec3 center = WorldPosition();
	return PondShore(pos.x - center.x, pos.z - center.z, sizeX * 0.5f, sizeZ * 0.5f, Yaw());
}

float WaterActor::ShoreDistance(const Vec3& pos) const
{
	return Maxf(Shore(pos), 0.0f);
}

void WaterActor::PushOut(Vec3& pos, float radius) const
{
	float d = Shore(pos);
	if (d >= radius) return;

	// Straight out across the shore, its direction taken from the distance a step either side.
	const float step = 0.05f;
	float gx = Shore(pos + Vec3(step, 0.0f, 0.0f)) - Shore(pos - Vec3(step, 0.0f, 0.0f));
	float gz = Shore(pos + Vec3(0.0f, 0.0f, step)) - Shore(pos - Vec3(0.0f, 0.0f, step));
	float len = sqrtf(gx * gx + gz * gz);
	if (len < 1e-6f) return;
	pos.x += gx / len * (radius - d);
	pos.z += gz / len * (radius - d);
}

Vec3 WaterActor::EdgePoint(float angle, float offset) const
{
	Vec3 p = PondEdge(WorldPosition(), sizeX * 0.5f, sizeZ * 0.5f, Yaw(), angle, offset);
	p.y = 0.0f;
	return p;
}

PropActor::PropActor(int actorKind, int propModel)
	: Actor(actorKind), model(propModel)
{
}

bool PropActor::IsDrawn(const DrawContext& dc) const
{
	if (!visible) return false;

	// A chunk is drawn whole once any of it shows; its props each check the view, so the ones off screen,
	// or outside the sun's shadow map, cost no vertices. Poles and lamps are thin but tall, and a signal's
	// arm reaches 3.3 m to the side.
	OccluderSize o = ModelOccluder(model);
	const Vec3& s = Scale();
	float reach = Maxf(o.radius * Maxf(s.x, s.z), 3.5f);
	float top = Maxf(o.height * s.y, 8.5f);
	return dc.renderer->MayShow(WorldPosition(), reach, top);
}

void PropActor::Draw(const DrawContext& dc) const
{
	DrawParams params;
	params.emissive = emissive;
	params.phase = phase;
	params.moss = moss;
	params.skin = skin;
	params.outline = outline;
	params.outlineColor = outlineColor;

	if (shadowRadius > 0.0f) dc.renderer->DrawShadow(WorldPosition(), shadowRadius);
	dc.renderer->DrawModel(model, WorldMatrix(), params);
}

SleeperActor::SleeperActor(bool grandma, float breathPhase)
	: Actor(ACTOR_SLEEPER), isGrandma(grandma), phase(breathPhase)
{
	if (isGrandma) return;

	mote = AddChild(new PropActor(ACTOR_PROP, MODEL_DREAM_MOTE));
	mote->phase = breathPhase;
	mote->emissive = 0.6f;
}

void SleeperActor::Draw(const DrawContext& dc) const
{
	DrawParams params;
	params.phase = phase;
	if (anim.Bound()) params.skin = &anim.SkinMatrices();
	dc.renderer->DrawModel(isGrandma ? MODEL_SLEEPER_ELDER : MODEL_SLEEPER, WorldMatrix(), params);
}

NpcActor::NpcActor()
	: Actor(ACTOR_NPC)
{
	model = MODEL_GRANDMA;
	collider.shape = COLLIDER_CIRCLE;
	collider.radius = 0.4f;
}

void NpcActor::Draw(const DrawContext& dc) const
{
	DrawParams params;
	params.phase = phase;
	if (anim.Bound()) params.skin = &anim.SkinMatrices();

	dc.renderer->DrawShadow(WorldPosition(), 0.4f);
	dc.renderer->DrawModel(model, WorldMatrix(), params);
}

ForageActor::ForageActor(int forageKind)
	: PropActor(ACTOR_FORAGE, MODEL_VEGETABLE), forage(forageKind)
{
	if (forage == FORAGE_RED_BERRY) model = MODEL_BERRY_RED;
	else if (forage == FORAGE_PALE_BERRY) model = MODEL_BERRY_PALE;
}

DeerActor::DeerActor()
	: Actor(ACTOR_DEER)
{
}

void DeerActor::Draw(const DrawContext& dc) const
{
	DrawParams params;
	params.phase = stepPhase;
	params.emissive = 0.15f;
	if (anim.Bound()) params.skin = &anim.SkinMatrices();

	dc.renderer->DrawShadow(WorldPosition(), 0.7f);
	dc.renderer->DrawModel(MODEL_DEER, WorldMatrix(), params);

	if (carriesGrandma)
	{
		DrawParams glow;
		glow.phase = stepPhase * 0.3f;
		glow.tint = Vec3(1.0f, 0.72f, 0.40f);
		glow.emissive = 0.9f;
		// Her light hangs from an antler tip when the deer has one to hang it on.
		// The mote model sits about 1.05 up and 0.2 forward of its origin.
		Mat4 at = Mul(WorldMatrix(), MatTranslate(Vec3(0.15f, 1.35f, 0.75f)));
		if (antlerBone >= 0)
			at = Mul(Mul(WorldMatrix(), anim.BoneMatrix(antlerBone)), MatTranslate(Vec3(0.0f, -1.05f, -0.2f)));
		dc.renderer->DrawModel(MODEL_DREAM_MOTE, at, glow);
	}
}

ExitActor::ExitActor()
	: Actor(ACTOR_EXIT)
{
}

LanternActor::LanternActor()
	: PropActor(ACTOR_LANTERN, MODEL_LANTERN)
{
	SetYaw(0.3f);
	emissive = 0.4f;
	shadowRadius = 0.6f;
	collider.shape = COLLIDER_CIRCLE;
	collider.radius = 0.5f;
}

ChunkActor::ChunkActor(const Chunk& chunk)
	: Actor(ACTOR_CHUNK), cx(chunk.cx), cz(chunk.cz), stage(chunk.stage), modernity(chunk.modernity), hash(chunk.hash)
{
	// Stays at the origin, so its props keep the map's world coordinates exactly.
	layer = LAYER_GROUND;

	// The deeper into the green, the more of what the old world left has gone under moss.
	float overgrown = Saturatef((float)(stage - 1) * 0.3f + 0.12f + (1.0f - modernity) * 0.25f);

	for (size_t i = 0; i < chunk.props.size(); ++i)
	{
		const ChunkProp& p = chunk.props[i];
		PropActor* prop = AddChild(new PropActor(ACTOR_PROP, p.model));
		prop->SetPosition(p.pos);
		prop->SetYaw(p.yaw);
		prop->SetScale(Vec3(p.scale, p.scale, p.scale));
		prop->phase = p.pos.x * 0.37f + p.pos.z * 0.21f;
		prop->moss = overgrown;

		if (p.halfX > 0.0f)
		{
			prop->collider.shape = COLLIDER_BOX;
			prop->collider.halfX = p.halfX;
			prop->collider.halfZ = p.halfZ;
		}
		else if (p.radius > 0.0f)
		{
			prop->collider.shape = COLLIDER_CIRCLE;
			prop->collider.radius = p.radius;
		}

		// Poles and lamps are thin but tall, so nothing is reckoned shorter than one.
		OccluderSize o = ModelOccluder(p.model);
		reach = Maxf(reach, Maxf(o.radius, 1.5f) * p.scale);
		top = Maxf(top, Maxf(o.height, 8.5f) * p.scale);
	}

	// A lantern still lit at a rest lights, warms and keeps creatures off like the one where the road begins.
	for (size_t i = 0; i < chunk.lanterns.size(); ++i)
	{
		LanternActor* lantern = AddChild(new LanternActor());
		lantern->SetPosition(chunk.lanterns[i]);
		lantern->SetYaw(chunk.lanterns[i].x * 0.7f + chunk.lanterns[i].z * 0.3f);
	}
}

bool ChunkActor::IsDrawn(const DrawContext& dc) const
{
	return visible && inView;
}

void ChunkActor::Draw(const DrawContext& dc) const
{
	GroundParams ground;
	ground.stage = (float)stage;
	for (int i = 0; i < 4; ++i)
		ground.neighborStage[i] = neighborStage[i];
	ground.chunkSize = kChunkSize;
	ground.road = modernity;
	ground.city = 1.0f;

	// A hair of overlap hides cracks between neighbouring ground quads.
	dc.renderer->DrawGround(ChunkMap::ChunkCenter(cx, cz), kChunkSize + 0.02f, ground);
}

EnemyActor::EnemyActor(int enemyType, int enemyLevel)
	: Actor(ACTOR_ENEMY), type(enemyType), level(enemyLevel)
{
	collider.shape = COLLIDER_CIRCLE;
	collider.radius = GetEnemyInfo(enemyType).radius;
}

void EnemyActor::Draw(const DrawContext& dc) const
{
	const EnemyInfo& info = GetEnemyInfo(type);

	float size = 1.0f + 0.06f * (float)(level - 1);

	// Released, not killed: the body comes apart into light instead of collapsing.
	DrawParams params;
	params.phase = (float)(spawnIndex + 1) * 1.7f + home.x;
	params.flash = flash;
	if (!alive) params.dissolve = 1.0f - Saturatef(stateTimer / kDyingTime);

	// A reddening windup is the tell to roll.
	if (state == ENEMY_WINDUP)
	{
		float w = 1.0f - Saturatef(stateTimer / info.windup);
		params.tint = Vec3(1.0f, 1.0f - 0.55f * w, 1.0f - 0.55f * w);
		params.emissive = 0.25f * w;
	}

	if (anim.Bound()) params.skin = &anim.SkinMatrices();

	Mat4 model = Mul(WorldMatrix(), MatScale(Vec3(size, size, size)));
	if (alive) dc.renderer->DrawShadow(WorldPosition(), info.radius * 1.1f * size);
	dc.renderer->DrawModel(info.model, model, params);
}

ItemActor::ItemActor(int itemType)
	: Actor(ACTOR_ITEM), type(itemType)
{
	SetScale(Vec3(1.2f, 1.2f, 1.2f));
}

void ItemActor::Draw(const DrawContext& dc) const
{
	DrawParams params;
	params.phase = phase;
	params.emissive = 0.35f + sparkle * 1.4f;

	int model = type == ITEM_RELIC ? RelicModel(relicId) : GetItemInfo(type).model;
	dc.renderer->DrawShadow(WorldPosition(), 0.3f);
	dc.renderer->DrawModel(model, WorldMatrix(), params);
}

int RelicModel(int relicId)
{
	// Once every entry is filled, what is left lying about is old-world odds and ends: an old radio will do.
	if (relicId < 0 || relicId >= kRelicCount) return MODEL_RELIC_RADIO;
	return MODEL_RELIC_TORCH + relicId;
}
