#include "stdafx.h"
#include "Game.h"

#include <cstdio>

namespace
{
	const float kWaterDrain = 100.0f / 360.0f;    // per second on the road
	const float kFoodDrain = 100.0f / 540.0f;
	const float kColdDrain = 100.0f / 200.0f;     // at night, away from the lantern
	const float kWarmRecover = 100.0f / 60.0f;
	const float kTorchDrain = 100.0f / 150.0f;
	const float kFlaskWater = 35.0f;
	const float kFlaskClear = 0.45f;
	const float kLowMark = 25.0f;

	const int kEatOrder[] = { ITEM_BERRIES, ITEM_VEGETABLE, ITEM_CANNED_FOOD };

	bool Crossed(float before, float after, float mark)
	{
		return before >= mark && after < mark;
	}
}

bool Game::MenuOpen() const
{
	return statPanelOpen || bagOpen || journalOpen || packOpen;
}

void Game::UpdateSurvival(float dt)
{
	if (torchOn)
	{
		torchBattery = Maxf(torchBattery - kTorchDrain * dt, 0.0f);
		if (torchBattery <= 0.0f)
		{
			torchOn = false;
			ShowMessage("The torch flickers out. The battery is spent.", 3.5f);
		}
	}

	// The village asks nothing of the body; the meters move only with what you do there.
	if (level != LEVEL_ROUTE || deathTimer >= 0.0f) return;

	routeTime += dt;

	float oldWater = waterMeter;
	float oldFood = foodMeter;
	waterMeter = Maxf(waterMeter - kWaterDrain * dt, 0.0f);
	foodMeter = Maxf(foodMeter - kFoodDrain * dt, 0.0f);

	if (Crossed(oldWater, waterMeter, kLowMark)) ShowMessage("Your mouth is dry. Drink (R), or find water.", 4.0f);
	if (Crossed(oldWater, waterMeter, 0.01f)) ShowMessage("You are parched. The spores reach you faster.", 4.0f);
	if (Crossed(oldFood, foodMeter, kLowMark)) ShowMessage("Your stomach growls. Eat something (F).", 4.0f);
	if (Crossed(oldFood, foodMeter, 0.01f)) ShowMessage("Hunger slows your steps and your swings.", 4.0f);

	// Warmth only starts to matter once a whole day has passed out on the road.
	if (!warmthActive && routeTime > dayLength)
	{
		warmthActive = true;
		warmthMeter = 100.0f;
		ShowMessage("The nights are getting colder. Stay near light after dark.", 6.0f);
	}
	if (!warmthActive) return;

	bool night = timeOfDay < 0.22f || timeOfDay > 0.78f;
	bool nearLantern = lantern && DistXZ(player->Position(), lantern->WorldPosition()) < lantern->zoneRadius;

	float oldWarmth = warmthMeter;
	if (night && !nearLantern)
		warmthMeter = Maxf(warmthMeter - kColdDrain * dt, 0.0f);
	else
		warmthMeter = Minf(warmthMeter + kWarmRecover * dt, 100.0f);

	if (Crossed(oldWarmth, warmthMeter, kLowMark)) ShowMessage("You are shivering. Find the lantern, or wait for the sun.", 4.0f);

	if (warmthMeter <= 0.0f)
	{
		health -= dt;
		if (health <= 0.0f) FallAsleep("The cold takes you. You sink into a long sleep...");
	}
}

void Game::EatFood()
{
	if (deathTimer >= 0.0f) return;

	int pick = -1;
	for (int i = 0; i < 3 && pick < 0; ++i)
	{
		if (inventory[kEatOrder[i]] > 0) pick = kEatOrder[i];
	}

	if (pick < 0)
	{
		ShowMessage("You have nothing to eat.", 2.0f);
		return;
	}
	if (foodMeter >= 98.0f)
	{
		ShowMessage("You are not hungry.", 1.5f);
		return;
	}

	--inventory[pick];
	foodMeter = Minf(foodMeter + FoodValue(pick), 100.0f);
	++mealsEaten;

	char buf[96];
	sprintf_s(buf, sizeof(buf), "You eat the %s.  Food +%d", GetItemInfo(pick).name, (int)FoodValue(pick));
	ShowMessage(buf, 2.5f);
}

void Game::DrinkWater()
{
	if (deathTimer >= 0.0f) return;

	if (inventory[ITEM_CLEAN_WATER] <= 0)
	{
		ShowMessage("No water flasks left.", 2.0f);
		return;
	}

	--inventory[ITEM_CLEAN_WATER];
	waterMeter = Minf(waterMeter + kFlaskWater, 100.0f);
	sporeExposure = Maxf(sporeExposure - kFlaskClear, 0.0f);
	ShowMessage("You drink. The water rinses the spores out.", 2.5f);
}

void Game::ToggleTorch()
{
	if (!torchOwned) return;

	if (torchBattery <= 0.0f)
	{
		ShowMessage("The torch is dead. The battery will not come back.", 2.5f);
		return;
	}

	torchOn = !torchOn;
}

int Game::BagCount() const
{
	int count = torchOwned ? 1 : 0;
	for (int i = 0; i < ITEM_TYPE_COUNT; ++i)
	{
		if (TakesBagSlot(i)) count += inventory[i];
	}
	return count;
}

int Game::UnfoundRelic()
{
	int open[kRelicCount];
	int count = 0;
	for (int i = RELIC_WATCH; i <= RELIC_PHONE; ++i)
	{
		if (!relicFound[i]) open[count++] = i;
	}
	return count > 0 ? open[combatRng.RangeInt(0, count - 1)] : -1;
}

void Game::FindRelic(int id)
{
	if (id < 0) id = UnfoundRelic();

	if (id >= 0 && !relicFound[id])
	{
		relicFound[id] = true;
		char buf[128];
		sprintf_s(buf, sizeof(buf), "New entry in your Relic Journal: %s.  (B to read)", RelicName(id));
		ShowMessage(buf, 4.5f);
	}
	else
	{
		ShowMessage("Another old-world trinket. Nothing new to write down.", 3.0f);
	}

	if (level == LEVEL_ROUTE) GrantXp(12);
}

void Game::AddInsight(int amount, const char* text)
{
	natureInsight += amount;

	char buf[160];
	sprintf_s(buf, sizeof(buf), "%s  Nature Insight %d", text, natureInsight);
	ShowMessage(buf, 5.0f);
}

bool Game::HandleMenuKey(unsigned char key)
{
	if (bagOpen)
	{
		if (key == 'i' || key == 27) bagOpen = false;
		return true;
	}
	if (journalOpen)
	{
		if (key == 'b' || key == 27) journalOpen = false;
		return true;
	}
	return false;
}

void Game::DrawSurvivalHud()
{
	if (!survivalShown) return;

	const int h = renderer->GetHeight();

	struct Row
	{
		const char* label;
		float value;
		Vec3  low;
		Vec3  high;
	};

	Row rows[3] =
	{
		{ "WATER", waterMeter, Vec3(0.55f, 0.35f, 0.25f), Vec3(0.40f, 0.70f, 0.85f) },
		{ "FOOD", foodMeter, Vec3(0.55f, 0.35f, 0.25f), Vec3(0.85f, 0.75f, 0.40f) },
		{ "WARMTH", warmthMeter, Vec3(0.35f, 0.45f, 0.75f), Vec3(0.95f, 0.65f, 0.40f) },
	};
	int rowCount = warmthActive ? 3 : 2;

	int y = h - 92 - 30 * (rowCount - 1);
	for (int i = 0; i < rowCount; ++i)
	{
		bool low = rows[i].value < kLowMark;
		float pulse = low ? 0.6f + 0.4f * sinf(time * 6.0f) : 1.0f;
		renderer->DrawTexts(24, y, rows[i].label, low ? kHudAccent * pulse : kHudDim, false);
		renderer->DrawBarPx(100.0f, (float)y - 9.0f, 144.0f, 9.0f, rows[i].value / 100.0f, rows[i].low, rows[i].high);
		y += 30;
	}
}

void Game::DrawBagPanel()
{
	if (!bagOpen) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	renderer->DrawFade(Vec3(0.02f, 0.03f, 0.04f), 0.55f);

	float pw = 520.0f, ph = 330.0f;
	float px = (float)w * 0.5f - pw * 0.5f;
	float py = (float)h * 0.5f - ph * 0.5f;
	renderer->DrawRectPx(px, py, pw, ph, Vec3(0.10f, 0.11f, 0.10f), 0.94f);
	renderer->DrawRectPx(px, py, pw, 3.0f, Vec3(0.45f, 0.60f, 0.50f), 0.8f);

	int x = (int)px + 36;
	int y = (int)py + 44;
	char buf[128];

	sprintf_s(buf, sizeof(buf), "BAG   %d / %d", BagCount(), kBagCapacity);
	renderer->DrawTexts(x, y, buf, kHudInk, true);
	y += 38;

	const int shown[] = { ITEM_CLEAN_WATER, ITEM_BERRIES, ITEM_VEGETABLE, ITEM_CANNED_FOOD, ITEM_HERB };
	const char* keys[] = { "[R] drink", "[F] eat", "[F] eat", "[F] eat", "[Q] heal" };
	for (int i = 0; i < 5; ++i)
	{
		int type = shown[i];
		sprintf_s(buf, sizeof(buf), "%-16s x%d", GetItemInfo(type).name, inventory[type]);
		renderer->DrawTexts(x, y, buf, inventory[type] > 0 ? kHudInk : kHudDim, false);
		renderer->DrawTexts(x + 260, y, keys[i], kHudDim, false);
		y += 26;
	}

	if (torchOwned)
	{
		sprintf_s(buf, sizeof(buf), "Hand torch       battery %d%%", (int)(torchBattery + 0.5f));
		renderer->DrawTexts(x, y, buf, kHudInk, false);
		renderer->DrawTexts(x + 260, y, torchOn ? "[L] on" : "[L] off", kHudDim, false);
		y += 26;
	}

	y += 12;
	int relics = 0;
	for (int i = 0; i < kRelicCount; ++i) relics += relicFound[i] ? 1 : 0;
	sprintf_s(buf, sizeof(buf), "Nature Insight %d     Relics %d / %d", natureInsight, relics, kRelicCount);
	renderer->DrawTexts(x, y, buf, kHudAccent, false);

	renderer->DrawTexts(x, (int)(py + ph) - 22, "[I] close", kHudAccent, false);
}

void Game::DrawJournalPanel()
{
	if (!journalOpen) return;

	const int w = renderer->GetWidth();
	const int h = renderer->GetHeight();

	renderer->DrawFade(Vec3(0.02f, 0.03f, 0.04f), 0.55f);

	float pw = 760.0f, ph = 470.0f;
	float px = (float)w * 0.5f - pw * 0.5f;
	float py = (float)h * 0.5f - ph * 0.5f;
	renderer->DrawRectPx(px, py, pw, ph, Vec3(0.11f, 0.10f, 0.09f), 0.95f);
	renderer->DrawRectPx(px, py, pw, 3.0f, Vec3(0.70f, 0.60f, 0.40f), 0.8f);

	int x = (int)px + 36;
	int y = (int)py + 44;
	renderer->DrawTexts(x, y, "RELIC JOURNAL", kHudInk, true);
	renderer->DrawTexts(x + 250, y, "things the old world left behind", kHudDim, false);
	y += 36;

	for (int i = 0; i < kRelicCount; ++i)
	{
		if (relicFound[i])
		{
			renderer->DrawTexts(x, y, RelicName(i), Vec3(0.92f, 0.84f, 0.62f), false);
			renderer->DrawTexts(x + 16, y + 20, RelicText(i), kHudInk, false);
		}
		else
		{
			renderer->DrawTexts(x, y, "? ? ?", kHudDim, false);
		}
		y += 44;
	}

	renderer->DrawTexts(x, (int)(py + ph) - 18, "[B] close", kHudAccent, false);
}
