/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <vector>
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Renderer.h"
#include "Game.h"
#include "DebugMonitor.h"

// Laptops with two GPUs otherwise hand an OpenGL program to the integrated one.
extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

Renderer* g_Renderer = NULL;
Game*     g_Game = NULL;
DebugMonitor g_Monitor;

const int    kMonitorPort = 8080;
const double kSpikeMs = 33.4;

bool   g_Keys[256] = { false };
double g_PrevTime = 0.0;

double g_StatsStart = 0.0;
double g_PrevFrame = 0.0;
double g_UpdateMs = 0.0;      // the last update, so a spike can say which half was slow
double g_UpdateSum = 0.0;
double g_RenderSum = 0.0;
int    g_StatsDrawCalls = 0;
int    g_Spikes = 0;
std::vector<float> g_FrameMs;

static double NowMs()
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static unsigned char NormalizeKey(unsigned char key)
{
	if (key >= 'A' && key <= 'Z') return key - 'A' + 'a';
	return key;
}

void RenderScene(void)
{
	double start = NowMs();
	if (g_Game) g_Game->Render();
	if (g_Renderer) g_Renderer->EndFrame();
	double renderMs = NowMs() - start;
	glutSwapBuffers();

	if (g_Renderer == NULL) return;

	double now = NowMs();
	double frameMs = now - g_PrevFrame;
	g_PrevFrame = now;
	g_FrameMs.push_back((float)frameMs);
	g_RenderSum += renderMs;
	g_StatsDrawCalls += g_Renderer->TakeDrawCalls();

	if (frameMs > kSpikeMs && g_FrameMs.size() > 1)
	{
		// Only the first few each second, so a bad patch does not flood the console.
		if (++g_Spikes <= 3)
			printf("Spike %.1f ms (update %.1f, draw %.1f)\n", frameMs, g_UpdateMs, renderMs);
	}

	// One line per second; drawing to the console every frame would cost frames itself.
	double elapsed = now - g_StatsStart;
	if (elapsed >= 1000.0)
	{
		int frames = (int)g_FrameMs.size();
		std::vector<float> sorted = g_FrameMs;
		std::sort(sorted.begin(), sorted.end());
		float worst = sorted.back();
		float p99 = sorted[(size_t)((float)(frames - 1) * 0.99f)];

		PerfStats stats;
		stats.fps = (float)(frames * 1000.0 / elapsed);
		stats.lowFps = p99 > 0.0f ? 1000.0f / p99 : 0.0f;
		stats.worstMs = worst;
		stats.updateMs = (float)(g_UpdateSum / frames);
		stats.renderMs = (float)(g_RenderSum / frames);
		stats.gpu = g_Renderer->LastGpuTimes();
		stats.drawCalls = (g_StatsDrawCalls + frames / 2) / frames;
		stats.spikes = g_Spikes;
		if (g_Game) g_Game->SetPerf(stats);

		printf("FPS %.0f | 1%% low %.0f | worst %.1f ms | CPU update %.2f draw %.2f | GPU shadow %.2f scene %.2f post %.2f ui %.2f | calls %d | spikes %d\n",
			   stats.fps, stats.lowFps, stats.worstMs, stats.updateMs, stats.renderMs,
			   stats.gpu.shadow, stats.gpu.scene, stats.gpu.post, stats.gpu.ui, stats.drawCalls, stats.spikes);

		g_StatsStart = now;
		g_FrameMs.clear();
		g_UpdateSum = 0.0;
		g_RenderSum = 0.0;
		g_StatsDrawCalls = 0;
		g_Spikes = 0;
	}
}

void Idle(void)
{
	double now = NowMs();
	float dt = (float)((now - g_PrevTime) / 1000.0);
	g_PrevTime = now;

	// A long stall (window drag, breakpoint) must not teleport the player.
	if (dt < 0.0f) dt = 0.0f;
	if (dt > 0.05f) dt = 0.05f;

	if (g_Game)
	{
		g_Game->Update(dt, g_Keys);
		g_UpdateMs = NowMs() - now;
		g_UpdateSum += g_UpdateMs;
		if (g_Game->WantsQuit())
		{
			glutLeaveMainLoop();
			return;
		}
		g_Monitor.Poll([]() { return g_Game->MonitorJson(); });
	}

	glutPostRedisplay();
}

void Reshape(int w, int h)
{
	if (h <= 0) h = 1;
	if (g_Renderer) g_Renderer->Resize(w, h);
}

bool g_WheelDrag = false;
int  g_DragY = 0;

void MouseInput(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && g_Game)
		g_Game->OnMouseDown();
	if (button == GLUT_MIDDLE_BUTTON)
	{
		g_WheelDrag = state == GLUT_DOWN;
		g_DragY = y;
	}
}

// Holding the wheel down and dragging up brings the view in close; dragging down sends it back out.
void MotionInput(int x, int y)
{
	if (!g_WheelDrag || !g_Game) return;
	g_Game->ZoomBy((float)(g_DragY - y) * 0.005f);
	g_DragY = y;
}

void WheelInput(int wheel, int direction, int x, int y)
{
	if (g_Game) g_Game->ZoomBy((float)direction * 0.12f);
}

void KeyInput(unsigned char key, int x, int y)
{
	key = NormalizeKey(key);
	g_Keys[key] = true;
	if (g_Game) g_Game->OnKeyDown(key);
}

void KeyUpInput(unsigned char key, int x, int y)
{
	key = NormalizeKey(key);
	g_Keys[key] = false;
}

// Arrow keys drive the same state as WASD.
static unsigned char SpecialToKey(int key)
{
	switch (key)
	{
	case GLUT_KEY_UP:    return 'w';
	case GLUT_KEY_DOWN:  return 's';
	case GLUT_KEY_LEFT:  return 'a';
	case GLUT_KEY_RIGHT: return 'd';
	default:             return 0;
	}
}

static void ToggleMonitor()
{
	if (g_Monitor.IsRunning())
	{
		g_Monitor.Stop();
		std::cout << "Monitor off\n";
	}
	else if (g_Monitor.Start(kMonitorPort))
	{
		std::cout << "Monitor on: http://localhost:" << kMonitorPort << "\n";
	}
	else
	{
		std::cout << "Monitor could not listen on port " << kMonitorPort << "\n";
	}
}

void SpecialKeyInput(int key, int x, int y)
{
	if (key == GLUT_KEY_F2)
	{
		if (g_Game) g_Game->SkipToRoute();
		return;
	}
	if (key == GLUT_KEY_F3)
	{
		ToggleMonitor();
		return;
	}
	if (key == GLUT_KEY_F4)
	{
		if (g_Game) g_Game->ToggleOptions();
		return;
	}

	unsigned char k = SpecialToKey(key);
	if (k) g_Keys[k] = true;
}

void SpecialKeyUpInput(int key, int x, int y)
{
	unsigned char k = SpecialToKey(key);
	if (k) g_Keys[k] = false;
}

int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(80, 40);
	glutInitWindowSize(1280, 720);
	glutCreateWindow("Mulangae Village - SimpleGame Prototype");

	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glewInit();
	if (glewIsSupported("GL_VERSION_3_0"))
	{
		std::cout << " GLEW Version is 3.0\n ";
	}
	else
	{
		std::cout << "GLEW 3.0 not supported\n ";
	}

	g_Renderer = new Renderer(1280, 720);
	if (!g_Renderer->IsInitialized())
	{
		std::cout << "Renderer could not be initialized.. \n";
		std::cout << "Check that the working directory contains the Shaders folder.\n";
	}

	g_Game = new Game(g_Renderer);

	std::cout << "\n=== Mulangae Village - prototype ===\n";
	std::cout << " WASD / arrows : move\n";
	std::cout << " SPACE         : roll (dash)\n";
	std::cout << " E             : interact\n";
	std::cout << " T             : toggle fast time of day\n";
	std::cout << " F2            : skip to Route 32\n";
	std::cout << " F3            : toggle state monitor (browser)\n";
	std::cout << " F4            : graphics options\n";
	std::cout << " wheel drag    : zoom toward the player\n";
	std::cout << " ESC           : quit\n\n";

	// One key press per physical press, so SPACE cannot auto-repeat into a chain of rolls.
	glutIgnoreKeyRepeat(1);

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutReshapeFunc(Reshape);
	glutKeyboardFunc(KeyInput);
	glutKeyboardUpFunc(KeyUpInput);
	glutSpecialFunc(SpecialKeyInput);
	glutSpecialUpFunc(SpecialKeyUpInput);
	glutMouseFunc(MouseInput);
	glutMotionFunc(MotionInput);
	glutMouseWheelFunc(WheelInput);

	g_PrevTime = NowMs();
	g_StatsStart = g_PrevTime;
	g_PrevFrame = g_PrevTime;

	glutMainLoop();

	g_Monitor.Stop();
	delete g_Game;
	delete g_Renderer;

	return 0;
}
