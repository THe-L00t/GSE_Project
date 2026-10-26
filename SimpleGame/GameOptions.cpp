#include "stdafx.h"
#include "Game.h"

#include <cstdio>
#include <cstring>

namespace
{
	const char* kOptionsFile = "Options.txt";

	const float kScales[] = { 0.5f, 0.67f, 0.75f, 0.85f, 1.0f };
	const int   kMsaa[] = { 0, 2, 4, 8 };
	const int   kShadows[] = { 0, 1024, 2048, 4096 };
	const float kParticles[] = { 0.25f, 0.5f, 0.75f, 1.0f };

	const int kChoiceCount[OPT_COUNT] = { PRESET_COUNT + 1, 5, 4, 4, 3, 4, 2, 2, 2 };

	const char* kRowNames[OPT_COUNT] =
	{
		"품질 프리셋", "렌더 해상도", "안티에일리어싱", "그림자", "빛줄기 (볼류메트릭)", "날씨·포자 입자", "주변광 차폐 (SSAO)", "수직 동기화", "FPS 표시",
	};

	const char* kPresetNames[PRESET_COUNT + 1] = { "낮음", "보통", "높음", "최고", "사용자" };

	// Choice indices for the scale, MSAA, shadow, volumetric, particle and SSAO rows of each preset.
	const int kPresetRows = 6;
	const int kPresets[PRESET_COUNT][kPresetRows] =
	{
		{ 1, 0, 1, 0, 1, 0 },
		{ 3, 1, 1, 1, 2, 0 },
		{ 4, 2, 2, 1, 3, 1 },
		{ 4, 3, 3, 2, 3, 1 },
	};

	const char* kRowKeys[OPT_COUNT] = { "preset", "scale", "msaa", "shadow", "volumetric", "particles", "ssao", "vsync", "fps" };
}

void Game::ApplyOptions()
{
	RenderSettings s;
	s.renderScale = kScales[optionChoice[OPT_SCALE]];
	s.msaa = kMsaa[optionChoice[OPT_MSAA]];
	s.shadowSize = kShadows[optionChoice[OPT_SHADOW]];
	s.volumetric = optionChoice[OPT_VOLUMETRIC];
	s.particles = kParticles[optionChoice[OPT_PARTICLES]];
	s.ssao = optionChoice[OPT_SSAO] == 1;
	s.vsync = optionChoice[OPT_VSYNC] == 0;
	renderer->ApplySettings(s);
}

void Game::SetPreset(int preset)
{
	optionChoice[OPT_PRESET] = preset;
	if (preset >= PRESET_COUNT) return;

	for (int i = 0; i < kPresetRows; ++i)
		optionChoice[OPT_SCALE + i] = kPresets[preset][i];
}

void Game::LoadOptions()
{
	SetPreset(PRESET_HIGH);

	FILE* file = nullptr;
	if (fopen_s(&file, kOptionsFile, "r") == 0 && file)
	{
		char key[32];
		int value = 0;
		while (fscanf_s(file, " %31[^=]=%d", key, (unsigned)sizeof(key), &value) == 2)
		{
			for (int i = 0; i < OPT_COUNT; ++i)
			{
				if (strcmp(key, kRowKeys[i]) == 0 && value >= 0 && value < kChoiceCount[i]) optionChoice[i] = value;
			}
		}
		fclose(file);
	}
	ApplyOptions();
}

void Game::SaveOptions() const
{
	FILE* file = nullptr;
	if (fopen_s(&file, kOptionsFile, "w") != 0 || !file) return;
	for (int i = 0; i < OPT_COUNT; ++i)
		fprintf(file, "%s=%d\n", kRowKeys[i], optionChoice[i]);
	fclose(file);
}

void Game::ToggleOptions()
{
	optionsOpen = !optionsOpen;
	if (!optionsOpen) SaveOptions();
}

void Game::HandleOptionsKey(unsigned char key)
{
	switch (key)
	{
	case 'w':
		optionsCursor = (optionsCursor + OPT_COUNT - 1) % OPT_COUNT;
		break;
	case 's':
		optionsCursor = (optionsCursor + 1) % OPT_COUNT;
		break;
	case 'a':
	case 'd':
	{
		int count = optionsCursor == OPT_PRESET ? PRESET_COUNT : kChoiceCount[optionsCursor];
		int step = key == 'd' ? 1 : count - 1;
		int choice = optionChoice[optionsCursor] >= count ? 0 : optionChoice[optionsCursor];
		choice = (choice + step) % count;

		if (optionsCursor == OPT_PRESET)
			SetPreset(choice);
		else
		{
			optionChoice[optionsCursor] = choice;
			if (optionsCursor <= OPT_SSAO) optionChoice[OPT_PRESET] = PRESET_COUNT;
		}
		ApplyOptions();
		break;
	}
	case 27:
	case 'e':
		ToggleOptions();
		break;
	default:
		break;
	}
}

void Game::DrawOptionsPanel()
{
	if (!optionsOpen) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	renderer->DrawFade(Vec3(0.02f, 0.03f, 0.04f), 0.55f);

	float pw = 600.0f, ph = 410.0f;
	float px = (float)w * 0.5f - pw * 0.5f;
	float py = (float)h * 0.5f - ph * 0.5f;
	renderer->DrawRectPx(px, py, pw, ph, Vec3(0.10f, 0.11f, 0.10f), 0.94f);
	renderer->DrawRectPx(px, py, pw, 3.0f, Vec3(0.45f, 0.60f, 0.50f), 0.8f);

	int x = (int)px + 36;
	int y = (int)py + 44;
	renderer->DrawTexts(x, y, "그래픽 옵션", kHudInk, true);
	y += 42;

	char value[64];
	for (int r = 0; r < OPT_COUNT; ++r)
	{
		int c = optionChoice[r];
		switch (r)
		{
		case OPT_PRESET:     sprintf_s(value, sizeof(value), "%s", kPresetNames[c]); break;
		case OPT_SCALE:      sprintf_s(value, sizeof(value), "%d%%", (int)(kScales[c] * 100.0f + 0.5f)); break;
		case OPT_MSAA:       sprintf_s(value, sizeof(value), c == 0 ? "끔" : "MSAA x%d", kMsaa[c]); break;
		case OPT_SHADOW:     sprintf_s(value, sizeof(value), c == 0 ? "끔" : "%d", kShadows[c]); break;
		case OPT_VOLUMETRIC: sprintf_s(value, sizeof(value), "%s", c == 0 ? "끔" : (c == 1 ? "낮음" : "높음")); break;
		case OPT_PARTICLES:  sprintf_s(value, sizeof(value), "%d%%", (int)(kParticles[c] * 100.0f + 0.5f)); break;
		case OPT_SSAO:       sprintf_s(value, sizeof(value), "%s", c == 1 ? "켬" : "끔"); break;
		default:             sprintf_s(value, sizeof(value), "%s", c == 0 ? "켬" : "끔"); break;
		}

		bool selected = r == optionsCursor;
		if (selected)
			renderer->DrawRectPx((float)x - 14.0f, (float)y - 18.0f, pw - 44.0f, 26.0f, Vec3(0.25f, 0.40f, 0.34f), 0.45f);

		Vec3 color = selected ? kHudInk : kHudDim;
		renderer->DrawTexts(x + 16, y, kRowNames[r], color, false);
		renderer->DrawTexts(x + 300, y, value, selected ? kHudAccent : color, false);
		y += 30;
	}

	renderer->DrawTexts(x, (int)(py + ph) - 22, "W/S 선택    A/D 바꾸기    F4·E 닫기 (저장)", kHudAccent, false);
}

void Game::DrawFpsOverlay()
{
	if (optionChoice[OPT_FPS] != 0) return;

	char buf[128];
	float gpu = perf.gpu.shadow + perf.gpu.scene + perf.gpu.post + perf.gpu.ui;
	sprintf_s(buf, sizeof(buf), "FPS %.0f  (1%% %.0f)   GPU %.1f ms   CPU %.1f ms", perf.fps, perf.lowFps, gpu, perf.updateMs + perf.renderMs);
	// Top middle, moved right of a long objective rather than over it.
	int tw = renderer->TextWidth(buf, false);
	int x = renderer->GetWidth() / 2 - tw / 2;
	if (x < (int)hudObjectiveRight + 16) x = (int)hudObjectiveRight + 16;
	renderer->DrawTexts(x, 22, buf, kHudDim, false);
}
