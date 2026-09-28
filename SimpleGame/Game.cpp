#include "stdafx.h"
#include "Game.h"

#include <algorithm>
#include <cstdio>

// On-screen text stays ASCII: the bitmap fonts freeglut ships cannot draw Hangul.

namespace
{
	const float kWalkSpeed = 4.2f;
	const float kRollSpeed = 12.0f;
	const float kRollTime = 0.28f;
	const float kRollCooldown = 0.70f;
	const float kTitleDuration = 8.0f;
}

Game::Game(Renderer* r)
	: renderer(r)
{
	worldNode = scene.Root()->AddChild(new Actor(ACTOR_NODE));

	player = scene.Root()->AddChild(new PlayerActor());
	player->SetPosition(Vec3(2.0f, 0.0f, 1.5f));
	player->SetYaw(kPi);

	camera = scene.Root()->AddChild(new CameraActor());
	camera->SetPosition(player->Position());

	BuildVillage();
}

void Game::Update(float dt, const bool* keys)
{
	if (messageTimer > 0.0f) messageTimer -= dt;

	// Panels pause the world so choices can be made calmly.
	if (MenuOpen()) return;

	time += dt;
	levelTimer += dt;
	++tick;

	timeOfDay += (dt * timeScale) / dayLength;
	while (timeOfDay >= 1.0f) timeOfDay -= 1.0f;

	if (level == LEVEL_VILLAGE)
		UpdateVillage(dt, keys);
	else
		UpdateRoute(dt, keys);

	UpdateSurvival(dt);
	UpdateCamera(dt);
	scene.RemoveDestroyed();
}

void Game::UpdatePlayer(float dt, const bool* keys)
{
	PlayerActor& p = *player;

	// Camera relative: W always moves up the screen.
	float camYaw = camera->Yaw();
	Vec3 forward(-sinf(camYaw), 0.0f, -cosf(camYaw));
	Vec3 right(cosf(camYaw), 0.0f, -sinf(camYaw));

	Vec3 wish;
	if (keys['w']) wish = wish + forward;
	if (keys['s']) wish = wish - forward;
	if (keys['d']) wish = wish + right;
	if (keys['a']) wish = wish - right;

	float wishLen = Length(wish);
	if (wishLen > 0.001f) wish = wish * (1.0f / wishLen);

	if (p.rollCooldown > 0.0f) p.rollCooldown -= dt;

	Vec3 pos = p.Position();
	if (p.rollTimer > 0.0f)
	{
		// The roll commits to its direction; steering during it would rob the weight.
		p.rollTimer -= dt;
		float k = Saturatef(p.rollTimer / kRollTime);
		float speed = kRollSpeed * (0.35f + 0.65f * k);
		pos = pos + p.rollDir * (speed * dt);
		p.rollAngle += dt / kRollTime * 2.0f * kPi;
		if (p.rollTimer <= 0.0f)
		{
			p.rollTimer = 0.0f;
			p.rollAngle = 0.0f;
		}
	}
	else if (wishLen > 0.001f)
	{
		float speed = kWalkSpeed * MoveSpeedScale(stats) * (p.swingTimer > 0.0f ? 0.7f : 1.0f) * (foodMeter <= 0.0f ? 0.85f : 1.0f);
		pos = pos + wish * (speed * dt);
		p.SetYaw(atan2f(wish.x, wish.z));
		p.walkPhase += dt * 9.0f;
	}
	else
	{
		p.walkPhase = Approach(p.walkPhase, 0.0f, 6.0f, dt);
	}

	pos.y = 0.0f;
	p.SetPosition(pos);
}

void Game::UpdateCamera(float dt)
{
	camera->SetPosition(LerpV(camera->Position(), player->Position(), 1.0f - expf(-6.0f * dt)));
}

void Game::OnKeyDown(unsigned char key)
{
	if (key >= 'A' && key <= 'Z') key = key - 'A' + 'a';

	if (statPanelOpen)
	{
		HandleStatPanelKey(key);
		return;
	}

	if (packOpen)
	{
		HandlePackKey(key);
		return;
	}

	if (HandleMenuKey(key)) return;

	if (key == 27) // ESC
	{
		quit = true;
		return;
	}

	if (key == 'e')
	{
		if (level == LEVEL_VILLAGE) TryInteract();
		else TryRouteInteract();
		return;
	}

	// While someone is talking or a letter is open, only E moves things on.
	if (DialogOpen() || letterOpen) return;

	if (key == 'i')
	{
		bagOpen = true;
		return;
	}
	if (key == 'b')
	{
		journalOpen = true;
		return;
	}
	if (key == 'l')
	{
		ToggleTorch();
		return;
	}
	if (key == 'f')
	{
		EatFood();
		return;
	}
	if (key == 'r')
	{
		DrinkWater();
		return;
	}

	if (key == ' ')
	{
		if (deathTimer >= 0.0f || sleepTimer >= 0.0f || transitionTimer >= 0.0f) return;
		if (player->rollTimer > 0.0f || player->rollCooldown > 0.0f) return;

		player->rollTimer = kRollTime;
		player->rollCooldown = kRollCooldown;
		player->rollAngle = 0.0f;
		player->rollDir = Vec3(sinf(player->Yaw()), 0.0f, cosf(player->Yaw()));
		return;
	}

	if (key == 't')
	{
		timeScale = (timeScale > 1.5f) ? 1.0f : 12.0f;
		ShowMessage(timeScale > 1.5f ? "시간: 빠르게" : "시간: 보통", 2.0f);
		return;
	}

	if (level != LEVEL_ROUTE) return;

	if (key == 'j') Attack();
	else if (key == 'q') UseHerb();
	else if (key == 'c') OpenStatPanel();
}

void Game::OnMouseDown()
{
	if (!statPanelOpen && level == LEVEL_ROUTE) Attack();
}

void Game::SkipToRoute()
{
	if (level == LEVEL_ROUTE) return;

	// Skipping the tutorial still sends you off with what the village would have given.
	if (tutorial < TUT_LEAVE)
	{
		if (inventory[ITEM_CLEAN_WATER] < 2) inventory[ITEM_CLEAN_WATER] = 2;
		if (inventory[ITEM_BERRIES] < 2) inventory[ITEM_BERRIES] = 2;
		if (inventory[ITEM_HERB] < 1) inventory[ITEM_HERB] = 1;
		torchOwned = true;
		relicFound[RELIC_TORCH] = true;
	}
	StartRoute();
}

void Game::ShowMessage(const char* text, float seconds)
{
	message = text;
	messageTimer = seconds;
}

void Game::Render()
{
	Vec3 view = camera->Position();

	renderer->BeginFrame();
	renderer->SetFrame(timeOfDay, sporeExposure, view, time);
	camera->Apply(renderer);

	if (level == LEVEL_ROUTE) PrepareChunkView();
	GatherLights(view);

	player->UpdatePose(deathTimer);

	// The same scene twice: once as depth from the sun, then lit with those shadows.
	renderer->BeginShadowPass(view);
	scene.Draw(renderer, view);
	renderer->EndShadowPass();

	scene.Draw(renderer, view);

	// Spores last: additive, and they should sit over everything.
	renderer->DrawSpores(Vec3(view.x, 0.0f, view.z), Vec3(70.0f, 16.0f, 70.0f),
						 Vec3(0.55f, 0.95f, 0.80f), sporeVisual, 0.13f);
	renderer->DrawAmbientMotes(Vec3(view.x, 0.0f, view.z));

	renderer->EndScene();
	renderer->BeginUI();

	// Entering Route 32, the fog lifts off the road.
	float haze = 0.08f + sporeExposure * 0.34f + routeIntroHaze * 0.7f;
	float vignette = 0.28f + sporeExposure * 0.42f;
	renderer->DrawAtmosphere(vignette, haze, Vec3(0.42f, 0.70f, 0.62f));

	if (level == LEVEL_VILLAGE)
		DrawVillageHud();
	else
		DrawRouteHud();

	renderer->EndUI();
}

void Game::GatherLights(const Vec3& view)
{
	struct Candidate
	{
		Vec3  pos;
		Vec3  color;
		float radius;
		float dist;
	};
	std::vector<Candidate> lights;

	// The torch always makes the cut: it is the light you are holding.
	if (torchOn)
	{
		Vec3 facing(sinf(player->Yaw()), 0.0f, cosf(player->Yaw()));
		Vec3 pos = player->Position() + facing * 1.6f + Vec3(0.0f, 1.3f, 0.0f);
		lights.push_back({ pos, Vec3(1.00f, 0.92f, 0.75f) * 2.0f, 8.0f, -1.0f });
	}

	std::vector<LanternActor*> lanterns;
	SceneGraph::Collect(scene.Root(), ACTOR_LANTERN, lanterns);
	for (size_t i = 0; i < lanterns.size(); ++i)
	{
		Vec3 pos = lanterns[i]->WorldPosition() + Vec3(0.0f, 1.45f, 0.0f);
		lights.push_back({ pos, Vec3(1.00f, 0.70f, 0.38f) * 2.2f, 8.0f, DistXZ(pos, view) });
	}

	std::vector<SleeperActor*> sleepers;
	SceneGraph::Collect(scene.Root(), ACTOR_SLEEPER, sleepers);
	for (size_t i = 0; i < sleepers.size(); ++i)
	{
		if (sleepers[i]->mote == nullptr) continue;
		Vec3 pos = sleepers[i]->mote->WorldPosition() + Vec3(0.0f, 1.05f, 0.2f);
		lights.push_back({ pos, Vec3(0.45f, 0.95f, 0.78f) * 1.2f, 3.5f, DistXZ(pos, view) });
	}

	std::vector<DeerActor*> deers;
	SceneGraph::Collect(scene.Root(), ACTOR_DEER, deers);
	for (size_t i = 0; i < deers.size(); ++i)
	{
		Vec3 facing(sinf(deers[i]->Yaw()), 0.0f, cosf(deers[i]->Yaw()));
		Vec3 pos = deers[i]->WorldPosition() + facing * 0.7f + Vec3(0.0f, 2.4f, 0.0f);
		lights.push_back({ pos, Vec3(0.95f, 0.85f, 0.50f) * 1.6f, 6.0f, DistXZ(pos, view) });
	}

	std::vector<ItemActor*> items = LiveItems();
	for (size_t i = 0; i < items.size(); ++i)
	{
		Vec3 pos = items[i]->WorldPosition() + Vec3(0.0f, 0.5f, 0.0f);
		lights.push_back({ pos, Vec3(0.70f, 0.95f, 0.75f) * 0.6f, 2.5f, DistXZ(pos, view) });
	}

	std::sort(lights.begin(), lights.end(), [](const Candidate& a, const Candidate& b) { return a.dist < b.dist; });

	renderer->ClearLights();
	for (size_t i = 0; i < lights.size(); ++i)
		renderer->AddLight(lights[i].pos, lights[i].color, lights[i].radius);
}

void Game::DrawObjective(const char* text)
{
	float boxW = Maxf(430.0f, (float)renderer->TextWidth(text, false) + 40.0f);
	renderer->DrawRectPx(18.0f, 18.0f, boxW, 62.0f, kHudPanel, 0.38f);
	renderer->DrawTexts(32, 40, "목표", kHudDim, false);
	renderer->DrawTexts(32, 64, text, kHudInk, false);
}

void Game::DrawCommonHud(const char* help)
{
	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	renderer->DrawTexts(24, h - 62, "포자 노출", kHudDim, false);
	renderer->DrawBarPx(24.0f, (float)h - 54.0f, 220.0f, 12.0f, sporeExposure,
						Vec3(0.35f, 0.70f, 0.60f), Vec3(0.75f, 0.95f, 0.55f));

	int helpWidth = renderer->TextWidth(help, false);
	renderer->DrawTexts(w - helpWidth - 24, h - 24, help, kHudDim, false);

	if (!prompt.empty())
	{
		int tw = renderer->TextWidth(prompt.c_str(), true);
		renderer->DrawRectPx((float)(w / 2 - tw / 2 - 18), (float)(h - 150), (float)(tw + 36), 40.0f, kHudPanel, 0.45f);
		renderer->DrawTexts(w / 2 - tw / 2, h - 123, prompt.c_str(), kHudAccent, true);
	}

	if (messageTimer > 0.0f && !message.empty())
	{
		float alpha = Saturatef(messageTimer);
		int tw = renderer->TextWidth(message.c_str(), false);
		renderer->DrawRectPx((float)(w / 2 - tw / 2 - 20), (float)(h - 100), (float)(tw + 40), 36.0f,
							 Vec3(0.02f, 0.04f, 0.04f), 0.55f * alpha);
		renderer->DrawTexts(w / 2 - tw / 2, h - 77, message.c_str(), kHudInk, false);
	}
}

void Game::DrawTitleCard(const char* title, const char* subtitle)
{
	DrawTitleCardAt(title, subtitle, levelTimer);
}

void Game::DrawTitleCardAt(const char* title, const char* subtitle, float t)
{
	if (t < 0.0f || t >= kTitleDuration) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	float alpha = 1.0f;
	if (t < 1.0f) alpha = t;
	else if (t > kTitleDuration - 2.0f) alpha = Saturatef((kTitleDuration - t) * 0.5f);

	int w1 = renderer->TextWidth(title, true);
	int w2 = renderer->TextWidth(subtitle, false);

	renderer->DrawTexts(w / 2 - w1 / 2, h / 2 - 20, title, Vec3(0.92f, 0.95f, 0.92f) * alpha, true);
	renderer->DrawTexts(w / 2 - w2 / 2, h / 2 + 10, subtitle, Vec3(0.70f, 0.82f, 0.76f) * alpha, false);
}
