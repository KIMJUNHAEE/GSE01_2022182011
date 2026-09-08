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

Renderer *g_Renderer = NULL;
GLuint g_TestSpriteTexture = 0;

// Player position in world space. The camera is kept locked to this, so the
// player sprite always renders at the center of the screen while the world
// scrolls underneath it.
float g_PlayerX = 0.f;
float g_PlayerY = 0.f;
const float PLAYER_MOVE_SPEED = 15.f;

void MovePlayer(float dx, float dy)
{
	g_PlayerX += dx;
	g_PlayerY += dy;
	g_Renderer->SetCameraPosition(g_PlayerX, g_PlayerY);
}

void RenderScene(void)
{
	glClearColor(0.0f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Open-world test: a grid of world-space markers spread far beyond the
	// window so panning the camera is obvious as the player moves around.
	for (int gx = -5; gx <= 5; gx++)
	{
		for (int gy = -5; gy <= 5; gy++)
		{
			bool alt = ((gx + gy) % 2) == 0;
			g_Renderer->DrawSolidRect((float)gx * 100.f, (float)gy * 100.f, 0, 6,
				alt ? 1.f : 0.f, alt ? 0.f : 1.f, 1.f, 1.f);
		}
	}

	// A fixed world-space sprite, to check that it scrolls with the grid.
	g_Renderer->DrawSprite(150, 80, 0, 64, 64, g_TestSpriteTexture);

	// Player, always drawn at its own world position; since the camera is
	// locked to the player, this always lands at the center of the screen.
	g_Renderer->DrawSolidRect(g_PlayerX, g_PlayerY, 0, 16, 1, 1, 1, 1);

	glutSwapBuffers();
}

void Idle(void)
{
	RenderScene();
}

void MouseInput(int button, int state, int x, int y)
{
	RenderScene();
}

void KeyInput(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 'w': MovePlayer(0, PLAYER_MOVE_SPEED); break;
	case 's': MovePlayer(0, -PLAYER_MOVE_SPEED); break;
	case 'a': MovePlayer(-PLAYER_MOVE_SPEED, 0); break;
	case 'd': MovePlayer(PLAYER_MOVE_SPEED, 0); break;
	}

	RenderScene();
}

void SpecialKeyInput(int key, int x, int y)
{
	switch (key)
	{
	case GLUT_KEY_UP: MovePlayer(0, PLAYER_MOVE_SPEED); break;
	case GLUT_KEY_DOWN: MovePlayer(0, -PLAYER_MOVE_SPEED); break;
	case GLUT_KEY_LEFT: MovePlayer(-PLAYER_MOVE_SPEED, 0); break;
	case GLUT_KEY_RIGHT: MovePlayer(PLAYER_MOVE_SPEED, 0); break;
	}

	RenderScene();
}

int main(int argc, char **argv)
{
	// Initialize GL things
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(500, 500);
	glutCreateWindow("Game Software Engineering KPU");

	glewInit();
	if (glewIsSupported("GL_VERSION_3_0"))
	{
		std::cout << " GLEW Version is 3.0\n ";
	}
	else
	{
		std::cout << "GLEW 3.0 not supported\n ";
	}

	// Initialize Renderer
	g_Renderer = new Renderer(500, 500);
	if (!g_Renderer->IsInitialized())
	{
		std::cout << "Renderer could not be initialized.. \n";
	}

	// Load Textures
	g_TestSpriteTexture = g_Renderer->LoadTexture("./Resources/Textures/test_sprite.png");

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyInput);
	glutMouseFunc(MouseInput);
	glutSpecialFunc(SpecialKeyInput);

	glutMainLoop();

	delete g_Renderer;

    return 0;
}

