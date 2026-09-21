#include "stdafx.h"
#include "GameActors.h"

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
	params.phase = (rollTimer > 0.0f) ? 0.0f : walkPhase;
	params.flash = flash;
	if (hurtTimer > 0.0f && fmodf(hurtTimer, 0.16f) < 0.08f)
		params.flash = Maxf(params.flash, 0.35f);

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

WaterActor::WaterActor(float width, float depth)
	: Actor(ACTOR_WATER), sizeX(width), sizeZ(depth)
{
	layer = LAYER_WATER;
	collider.shape = COLLIDER_BOX;
	collider.halfX = width * 0.5f;
	collider.halfZ = depth * 0.5f;
}

void WaterActor::Draw(const DrawContext& dc) const
{
	dc.renderer->DrawWater(WorldPosition(), sizeX, sizeZ);
}

float WaterActor::ShoreDistance(const Vec3& pos) const
{
	Vec3 center = WorldPosition();
	float nearX = Maxf(fabsf(pos.x - center.x) - sizeX * 0.5f, 0.0f);
	float nearZ = Maxf(fabsf(pos.z - center.z) - sizeZ * 0.5f, 0.0f);
	return sqrtf(nearX * nearX + nearZ * nearZ);
}

PropActor::PropActor(int actorKind, int propModel)
	: Actor(actorKind), model(propModel)
{
}

void PropActor::Draw(const DrawContext& dc) const
{
	DrawParams params;
	params.emissive = emissive;
	params.phase = phase;

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
	dc.renderer->DrawModel(isGrandma ? MODEL_SLEEPER_ELDER : MODEL_SLEEPER, WorldMatrix(), params);
}

ExitActor::ExitActor()
	: Actor(ACTOR_EXIT)
{
}
