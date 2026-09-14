/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <iostream>
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Renderer.h"
#include "Game.h"

Renderer* g_Renderer = NULL;
Game*     g_Game = NULL;

bool g_Keys[256] = { false };
int  g_PrevTime = 0;

static unsigned char NormalizeKey(unsigned char key)
{
	if (key >= 'A' && key <= 'Z') return key - 'A' + 'a';
	return key;
}

void RenderScene(void)
{
	if (g_Game) g_Game->Render();
	glutSwapBuffers();
}

void Idle(void)
{
	int now = glutGet(GLUT_ELAPSED_TIME);
	float dt = (float)(now - g_PrevTime) / 1000.0f;
	g_PrevTime = now;

	// A long stall (window drag, breakpoint) must not teleport the player.
	if (dt < 0.0f) dt = 0.0f;
	if (dt > 0.05f) dt = 0.05f;

	if (g_Game)
	{
		g_Game->Update(dt, g_Keys);
		if (g_Game->WantsQuit())
		{
			glutLeaveMainLoop();
			return;
		}
	}

	glutPostRedisplay();
}

void Reshape(int w, int h)
{
	if (h <= 0) h = 1;
	if (g_Renderer) g_Renderer->Resize(w, h);
}

void MouseInput(int button, int state, int x, int y)
{
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

void SpecialKeyInput(int key, int x, int y)
{
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

	g_PrevTime = glutGet(GLUT_ELAPSED_TIME);

	glutMainLoop();

	delete g_Game;
	delete g_Renderer;

	return 0;
}
