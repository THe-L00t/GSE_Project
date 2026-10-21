#include "stdafx.h"
#include "Game.h"

#include "Models.h"

namespace
{
	// Ground each walk or run clip covers per second at its authored speed, from the Blender scripts.
	// The clips are sped up or slowed down to match how fast the body really moves, so feet do not slide.
	float LocomotionSpeed(int model)
	{
		switch (model)
		{
		case MODEL_PLAYER: return 4.2f;
		case MODEL_MITE:   return 3.2f;
		case MODEL_HUSK:   return 2.2f;
		case MODEL_BOAR:   return 2.6f;
		case MODEL_DEER:   return 1.3f;
		default:           return 2.0f;
		}
	}

	bool HasClip(const Animator& a, const char* clip)
	{
		return a.Bound() && a.GetSkeleton()->FindClip(clip) >= 0;
	}

	float MovedSpeed(Vec3& last, const Vec3& now, float dt)
	{
		float speed = DistXZ(now, last) / Maxf(dt, 1e-4f);
		last = now;
		return speed;
	}

	void Loop(Animator& a, const char* clip, float fade, float speed, float dt)
	{
		a.Play(clip, fade);
		a.SetSpeed(speed);
		a.Update(dt);
	}
}

void Game::UpdateAnimations(float dt)
{
	AnimatePlayer(dt);

	std::vector<EnemyActor*> enemies = LiveEnemies();
	for (size_t i = 0; i < enemies.size(); ++i)
		AnimateEnemy(*enemies[i], dt);

	AnimateGrandmother(dt);
	AnimateSleepers(dt);
	AnimateDeer(dt);

	if (!roadSleeperAnim.Bound())
	{
		roadSleeperAnim.Bind(renderer->ModelSkeleton(MODEL_SLEEPER));
		roadSleeperAnim.Play("breathe", 0.0f);
	}
	roadSleeperAnim.Update(dt);
}

void Game::PlayPlayerAction(const char* clip)
{
	if (player->rollTimer > 0.0f || player->swingTimer > 0.0f || deathTimer >= 0.0f) return;
	player->action = clip;
	player->actionFresh = true;
}

void Game::AnimatePlayer(float dt)
{
	PlayerActor& p = *player;
	Animator& a = p.anim;
	if (!a.Bound())
	{
		a.Bind(renderer->ModelSkeleton(MODEL_PLAYER));
		if (!a.Bound()) return;
		a.Play("idle", 0.0f);
		p.lastPos = p.Position();
	}

	float moved = MovedSpeed(p.lastPos, p.Position(), dt);

	// A fresh blow interrupts whatever the hands were doing.
	if (p.hurtTimer > p.lastHurt + 0.01f && HasClip(a, "hit"))
	{
		p.action = "hit";
		p.actionFresh = true;
	}
	p.lastHurt = p.hurtTimer;

	if (deathTimer >= 0.0f)
	{
		Loop(a, "lie", 0.25f, 1.0f, dt);
		return;
	}

	// The roll and the swing run on the game's own timers, so the pose and the hit always agree.
	if (p.rollTimer > 0.0f)
	{
		p.action = nullptr;
		a.Play("roll", 0.04f);
		a.SetNormalizedTime(1.0f - p.rollTimer / kRollTime, dt);
		return;
	}
	if (p.swingTimer > 0.0f)
	{
		p.action = nullptr;
		a.Play("attack", 0.04f);
		a.SetNormalizedTime(1.0f - p.swingTimer / kSwingTime, dt);
		return;
	}

	if (p.action != nullptr)
	{
		a.Play(p.action, 0.1f, p.actionFresh);
		p.actionFresh = false;
		a.SetSpeed(1.0f);
		a.Update(dt);
		if (!a.Finished() && a.IsPlaying(p.action)) return;
		p.action = nullptr;
	}

	if (moved > 0.4f)
		Loop(a, "run", 0.12f, Clampf(moved / LocomotionSpeed(MODEL_PLAYER), 0.5f, 1.6f), dt);
	else
		Loop(a, "idle", 0.2f, 1.0f, dt);
}

void Game::AnimateEnemy(EnemyActor& e, float dt)
{
	const EnemyInfo& info = GetEnemyInfo(e.type);
	Animator& a = e.anim;
	if (!a.Bound())
	{
		a.Bind(renderer->ModelSkeleton(info.model));
		if (!a.Bound()) return;
		e.lastPos = e.Position();
		e.asleep = e.guarding && HasClip(a, "sleep");
		a.Play(e.asleep ? "sleep" : "idle", 0.0f);
		a.SetTime(e.home.x * 0.37f + e.home.z * 0.21f);
	}

	float moved = MovedSpeed(e.lastPos, e.Position(), dt);

	if (!e.alive)
	{
		a.Play("die", 0.1f);
		a.SetNormalizedTime(1.0f - Saturatef(e.stateTimer / kDyingTime), dt);
		return;
	}

	switch (e.state)
	{
	case ENEMY_WINDUP:
		e.asleep = e.waking = false;
		a.Play("windup", 0.08f);
		a.SetNormalizedTime(1.0f - Saturatef(e.stateTimer / Maxf(info.windup, 0.01f)), dt);
		return;
	case ENEMY_STRIKE:
	{
		float length = info.lunge > 0.0f ? 0.4f : 0.15f;
		a.Play("strike", 0.05f);
		a.SetNormalizedTime(1.0f - Saturatef(e.stateTimer / length), dt);
		return;
	}
	case ENEMY_RECOVER:
		a.Play("recover", 0.1f);
		a.SetNormalizedTime(1.0f - Saturatef(e.stateTimer / Maxf(info.recover, 0.01f)), dt);
		return;
	default:
		break;
	}

	// A guard sleeps beside its sleeper until something comes close, then heaves itself up; back home
	// and calm again, it lies down once more.
	if (!e.asleep && !e.waking && e.guarding && e.state == ENEMY_IDLE && !e.provoked && moved < 0.05f &&
		DistXZ(e.Position(), e.home) < 0.3f && HasClip(a, "sleep"))
		e.asleep = true;
	if (e.asleep)
	{
		if (e.state == ENEMY_IDLE && !e.provoked)
		{
			Loop(a, "sleep", 0.3f, 1.0f, dt);
			return;
		}
		e.asleep = false;
		e.waking = HasClip(a, "wake");
		if (e.waking) a.Play("wake", 0.1f, true);
	}
	if (e.waking)
	{
		a.SetSpeed(1.8f);
		a.Update(dt);
		if (!a.Finished()) return;
		e.waking = false;
	}

	if (moved > 0.15f)
	{
		const char* clip = HasClip(a, "run") ? "run" : "walk";
		Loop(a, clip, 0.15f, Clampf(moved / LocomotionSpeed(info.model), 0.4f, 2.5f), dt);
	}
	else
	{
		Loop(a, "idle", 0.25f, 1.0f, dt);
	}
}

void Game::AnimateGrandmother(float dt)
{
	if (grandmaNpc == nullptr || grandmaNpc->IsDestroyed() || !grandmaNpc->visible) return;

	NpcActor& g = *grandmaNpc;
	Animator& a = g.anim;
	if (!a.Bound())
	{
		a.Bind(renderer->ModelSkeleton(g.model));
		if (!a.Bound()) return;
		a.Play("idle", 0.0f);
	}

	bool talking = DialogOpen() && dialogSpeaker == "할머니";
	if (talking)
	{
		Loop(a, "talk", 0.25f, 1.0f, dt);
		return;
	}

	// As the evening comes she sits down on her heels more and more.
	if (tutorial >= TUT_DUSK && tutorial <= TUT_BEDTIME && HasClip(a, "squat"))
	{
		Loop(a, "squat", 0.5f, 1.0f, dt);
		return;
	}

	// The spores are already on her: now and then a long yawn she tries to hide.
	g.yawnTimer -= dt;
	if (g.yawnTimer <= 0.0f && HasClip(a, "yawn"))
	{
		g.yawnTimer = 9.0f + fxRng.Range(0.0f, 7.0f);
		a.Play("yawn", 0.25f, true);
	}
	if (a.IsPlaying("yawn") && !a.Finished())
	{
		a.SetSpeed(1.0f);
		a.Update(dt);
		return;
	}
	Loop(a, "idle", 0.35f, 1.0f, dt);
}

void Game::AnimateSleepers(float dt)
{
	std::vector<SleeperActor*> sleepers;
	SceneGraph::Collect(scene.Root(), ACTOR_SLEEPER, sleepers);
	for (size_t i = 0; i < sleepers.size(); ++i)
	{
		SleeperActor& s = *sleepers[i];
		if (!s.visible) continue;
		if (!s.anim.Bound())
		{
			s.anim.Bind(renderer->ModelSkeleton(s.isGrandma ? MODEL_SLEEPER_ELDER : MODEL_SLEEPER));
			if (!s.anim.Bound()) continue;
			s.anim.Play("breathe", 0.0f);
			s.anim.SetTime(s.phase);
		}
		s.anim.Update(dt);
	}
}

void Game::AnimateDeer(float dt)
{
	if (deer == nullptr || deer->IsDestroyed()) return;

	Animator& a = deer->anim;
	if (!a.Bound())
	{
		a.Bind(renderer->ModelSkeleton(MODEL_DEER));
		if (!a.Bound()) return;
		deer->lastPos = deer->Position();
		deer->antlerBone = a.GetSkeleton()->FindBone("antler_tip_L");
		a.Play("idle", 0.0f);
	}

	float moved = MovedSpeed(deer->lastPos, deer->Position(), dt);
	if (moved > 0.2f)
		Loop(a, "walk", 0.2f, Clampf(moved / LocomotionSpeed(MODEL_DEER), 0.5f, 2.2f), dt);
	else if (deer->arrived && HasClip(a, "drink"))
		Loop(a, "drink", 0.4f, 1.0f, dt);
	else if (deer->leading && HasClip(a, "wait"))
		Loop(a, "wait", 0.35f, 1.0f, dt);
	else
		Loop(a, "idle", 0.3f, 1.0f, dt);
}

void Game::ShareSleeperPose(ChunkActor& chunk)
{
	for (size_t i = 0; i < chunk.ChildCount(); ++i)
	{
		Actor* child = chunk.Child(i);
		if (child->Kind() != ACTOR_PROP) continue;
		PropActor* prop = static_cast<PropActor*>(child);
		if (prop->model == MODEL_SLEEPER) prop->skin = &roadSleeperAnim.SkinMatrices();
	}
}
