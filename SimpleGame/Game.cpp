#include "stdafx.h"
#include "Game.h"

#include <cstdio>

#include "Models.h"

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
	camTarget = playerPos;
	BuildVillage();
}

void Game::Update(float dt, const bool* keys)
{
	time += dt;
	levelTimer += dt;
	if (messageTimer > 0.0f) messageTimer -= dt;

	timeOfDay += (dt * timeScale) / dayLength;
	while (timeOfDay >= 1.0f) timeOfDay -= 1.0f;

	if (level == LEVEL_VILLAGE)
		UpdateVillage(dt, keys);
	else
		UpdateRoute(dt, keys);

	UpdateCamera(dt);
}

void Game::UpdatePlayer(float dt, const bool* keys)
{
	// Camera relative: W always moves up the screen.
	Vec3 forward(-sinf(camYaw), 0.0f, -cosf(camYaw));
	Vec3 right(cosf(camYaw), 0.0f, -sinf(camYaw));

	Vec3 wish;
	if (keys['w']) wish = wish + forward;
	if (keys['s']) wish = wish - forward;
	if (keys['d']) wish = wish + right;
	if (keys['a']) wish = wish - right;

	float wishLen = Length(wish);
	if (wishLen > 0.001f) wish = wish * (1.0f / wishLen);

	if (rollCooldown > 0.0f) rollCooldown -= dt;

	if (rollTimer > 0.0f)
	{
		// The roll commits to its direction; steering during it would rob the weight.
		rollTimer -= dt;
		float k = Saturatef(rollTimer / kRollTime);
		float speed = kRollSpeed * (0.35f + 0.65f * k);
		playerPos = playerPos + rollDir * (speed * dt);
		rollAngle += dt / kRollTime * 2.0f * kPi;
		if (rollTimer <= 0.0f)
		{
			rollTimer = 0.0f;
			rollAngle = 0.0f;
		}
	}
	else if (wishLen > 0.001f)
	{
		playerPos = playerPos + wish * (kWalkSpeed * dt);
		playerYaw = atan2f(wish.x, wish.z);
		walkPhase += dt * 9.0f;
	}
	else
	{
		walkPhase = Approach(walkPhase, 0.0f, 6.0f, dt);
	}

	playerPos.y = 0.0f;
}

void Game::UpdateCamera(float dt)
{
	camTarget = LerpV(camTarget, playerPos, 1.0f - expf(-6.0f * dt));
}

void Game::OnKeyDown(unsigned char key)
{
	if (key >= 'A' && key <= 'Z') key = key - 'A' + 'a';

	if (key == 27) // ESC
	{
		quit = true;
		return;
	}

	if (key == 'e')
	{
		if (level == LEVEL_VILLAGE) TryInteract();
		return;
	}

	if (key == ' ')
	{
		if (letterOpen) return;
		if (rollTimer > 0.0f || rollCooldown > 0.0f) return;

		rollTimer = kRollTime;
		rollCooldown = kRollCooldown;
		rollAngle = 0.0f;
		rollDir = Vec3(sinf(playerYaw), 0.0f, cosf(playerYaw));
		return;
	}

	if (key == 't')
	{
		timeScale = (timeScale > 1.5f) ? 1.0f : 12.0f;
		ShowMessage(timeScale > 1.5f ? "Time: fast" : "Time: normal", 2.0f);
		return;
	}
}

void Game::SkipToRoute()
{
	if (level != LEVEL_ROUTE) StartRoute();
}

void Game::ShowMessage(const char* text, float seconds)
{
	message = text;
	messageTimer = seconds;
}

void Game::SetupCamera()
{
	Vec3 dir(cosf(camPitch) * sinf(camYaw), sinf(camPitch), cosf(camPitch) * cosf(camYaw));
	Vec3 eye = camTarget + dir * camDistance;

	float aspect = (float)renderer->GetWidth() / (float)Maxf((float)renderer->GetHeight(), 1.0f);
	float hh = orthoHeight * 0.5f;
	float hw = hh * aspect;

	Mat4 view = MatLookAt(eye, camTarget, Vec3(0.0f, 1.0f, 0.0f));
	Mat4 proj = MatOrtho(-hw, hw, -hh, hh, 0.1f, 400.0f);
	float pixelsPerUnit = (float)renderer->GetHeight() / orthoHeight;

	renderer->SetCamera(view, proj, eye, pixelsPerUnit);
}

void Game::Render()
{
	renderer->BeginFrame();
	renderer->SetFrame(timeOfDay, sporeExposure, camTarget, time);
	SetupCamera();

	if (level == LEVEL_VILLAGE)
		DrawVillage();
	else
		DrawRoute();

	DrawPlayer();

	// Spores last: additive, and they should sit over everything.
	renderer->DrawSpores(Vec3(camTarget.x, 0.0f, camTarget.z), Vec3(70.0f, 16.0f, 70.0f),
						 Vec3(0.55f, 0.95f, 0.80f), 0.55f, 0.13f);

	renderer->BeginUI();

	float haze = 0.08f + sporeExposure * 0.34f;
	float vignette = 0.28f + sporeExposure * 0.42f;
	renderer->DrawAtmosphere(vignette, haze, Vec3(0.42f, 0.70f, 0.62f));

	if (level == LEVEL_VILLAGE)
		DrawVillageHud();
	else
		DrawRouteHud();

	renderer->EndUI();
}

void Game::DrawPlayer()
{
	Mat4 base = Mul(MatTranslate(playerPos), MatRotateY(playerYaw));

	// Roll pivots around the waist so the tumble reads from a quarter view.
	const float pivot = 0.62f;
	Mat4 root = base;
	if (rollTimer > 0.0f)
	{
		Mat4 spin = Mul(Mul(MatTranslate(Vec3(0.0f, pivot, 0.0f)), MatRotateX(rollAngle)),
						MatTranslate(Vec3(0.0f, -pivot, 0.0f)));
		root = Mul(base, spin);
	}

	// Lit.vs turns the walk phase into the bob; a roll holds it still.
	DrawParams params;
	params.phase = (rollTimer > 0.0f) ? 0.0f : walkPhase;

	renderer->DrawShadow(playerPos, 0.45f);
	renderer->DrawModel(MODEL_PLAYER, root, params);
}

void Game::DrawObjective(const char* text)
{
	float boxW = Maxf(430.0f, (float)renderer->TextWidth(text, false) + 40.0f);
	renderer->DrawRectPx(18.0f, 18.0f, boxW, 62.0f, kHudPanel, 0.38f);
	renderer->DrawTexts(32, 40, "OBJECTIVE", kHudDim, false);
	renderer->DrawTexts(32, 64, text, kHudInk, false);
}

void Game::DrawCommonHud(const char* help)
{
	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	renderer->DrawTexts(24, h - 62, "SPORE EXPOSURE", kHudDim, false);
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
	if (levelTimer >= kTitleDuration) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	float alpha = 1.0f;
	if (levelTimer < 1.0f) alpha = levelTimer;
	else if (levelTimer > kTitleDuration - 2.0f) alpha = Saturatef((kTitleDuration - levelTimer) * 0.5f);

	int w1 = renderer->TextWidth(title, true);
	int w2 = renderer->TextWidth(subtitle, false);

	renderer->DrawTexts(w / 2 - w1 / 2, h / 2 - 20, title, Vec3(0.92f, 0.95f, 0.92f) * alpha, true);
	renderer->DrawTexts(w / 2 - w2 / 2, h / 2 + 10, subtitle, Vec3(0.70f, 0.82f, 0.76f) * alpha, false);
}
