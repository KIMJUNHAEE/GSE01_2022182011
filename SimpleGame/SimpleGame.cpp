/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#include "Renderer.h"

Renderer *g_Renderer = NULL;

GLuint g_FloorTextureA = 0;
GLuint g_FloorTextureB = 0;
GLuint g_PlayerTexture = 0;
GLuint g_PlayerWalk1 = 0;
GLuint g_PlayerWalk2 = 0;
GLuint g_CompanionTexture = 0;
GLuint g_CompanionWalk1 = 0;
GLuint g_CompanionWalk2 = 0;
GLuint g_PillarTexture = 0;
GLuint g_CrateTexture = 0;
GLuint g_GlowTexture = 0;
GLuint g_WhiteTexture = 0;
GLuint g_FontTexture = 0;

// Isometric (quarter-view) tile footprint in screen pixels, 2:1 ratio.
const float TILE_W = 128.f;
const float TILE_H = 64.f;
const float WINDOW_SIZE = 500.f;

// Bitmap font atlas layout (see Resources/Textures/font_atlas.png):
// ASCII 32.."126" laid out left-to-right, top-to-bottom on a 16x6 grid.
const int FONT_COLS = 16;
const int FONT_ROWS = 6;
const float FONT_CELL_W = 10.f;
const float FONT_CELL_H = 16.f;
const float FONT_ATLAS_W = 160.f;
const float FONT_ATLAS_H = 96.f;
const int FONT_FIRST_CHAR = 32;

// Converts a grid-space position (gridX, gridY - free-floating, not just
// tile-snapped) into the screen-pixel position used by Renderer's camera and
// draw calls. This is the only place the isometric projection lives; the
// Renderer itself just draws flat pixel-space quads and knows nothing about
// isometric grids.
void IsoToScreen(float gridX, float gridY, float* screenX, float* screenY)
{
	*screenX = (gridX - gridY) * (TILE_W / 2.f);
	*screenY = (gridX + gridY) * (TILE_H / 2.f);
}

// Player position in grid space. The camera is kept locked to its projected
// screen position, so the player sprite always renders at the center of the
// screen while the world scrolls underneath it.
float g_PlayerGridX = 0.f;
float g_PlayerGridY = 0.f;
const float PLAYER_MOVE_SPEED = 0.22f; // grid units per key press

int g_PlayerAnimMovingUntilMs = 0;
bool g_PlayerAnimFrame = false;

// Companion (captain) trails a fixed grid-space offset behind the player and
// eases toward that spot every frame, so it visibly "follows" rather than
// teleporting with the player.
float g_CompanionGridX = -1.f;
float g_CompanionGridY = 0.6f;
const float COMPANION_FOLLOW_OFFSET_X = -1.f;
const float COMPANION_FOLLOW_OFFSET_Y = 0.6f;
const float COMPANION_FOLLOW_LERP = 0.08f;

bool g_CompanionAnimFrame = false;
int g_CompanionAnimNextToggleMs = 0;

// Static "controlled planet" facility landmarks. Fixed grid positions so the
// player has something other than a repeating floor to navigate against.
struct EnvObject
{
	float gridX, gridY;
	float w, h;
	GLuint texture;
	bool isLightSource; // pillars emit a pulsing glow
};
std::vector<EnvObject> g_EnvObjects;

void MovePlayer(float dGridX, float dGridY)
{
	g_PlayerGridX += dGridX;
	g_PlayerGridY += dGridY;

	float camX, camY;
	IsoToScreen(g_PlayerGridX, g_PlayerGridY, &camX, &camY);
	g_Renderer->SetCameraPosition(camX, camY);

	g_PlayerAnimMovingUntilMs = glutGet(GLUT_ELAPSED_TIME) + 220;
	g_PlayerAnimFrame = !g_PlayerAnimFrame;
}

// WASD is mapped to screen-relative directions (up/down/left/right as the
// player actually sees them) rather than raw grid axes. On an isometric
// grid, moving straight "up" on screen means both grid axes change together
// (see IsoToScreen) - mapping keys to grid axes directly made W/A/S/D feel
// like they moved diagonally, which read as unclear/unintuitive.
void MovePlayerScreenRelative(float screenDX, float screenDY)
{
	// screenDX/screenDY are -1/0/1 "intent" values; convert to grid deltas.
	float dGridX = (screenDX + screenDY) * 0.5f;
	float dGridY = (screenDY - screenDX) * 0.5f;
	MovePlayer(dGridX * PLAYER_MOVE_SPEED * 2.f, dGridY * PLAYER_MOVE_SPEED * 2.f);
}

void UpdateCompanion()
{
	float targetX = g_PlayerGridX + COMPANION_FOLLOW_OFFSET_X;
	float targetY = g_PlayerGridY + COMPANION_FOLLOW_OFFSET_Y;

	float dx = targetX - g_CompanionGridX;
	float dy = targetY - g_CompanionGridY;
	float dist = sqrtf(dx * dx + dy * dy);

	g_CompanionGridX += dx * COMPANION_FOLLOW_LERP;
	g_CompanionGridY += dy * COMPANION_FOLLOW_LERP;

	int nowMs = glutGet(GLUT_ELAPSED_TIME);
	if (dist > 0.05f && nowMs >= g_CompanionAnimNextToggleMs)
	{
		g_CompanionAnimFrame = !g_CompanionAnimFrame;
		g_CompanionAnimNextToggleMs = nowMs + 220;
	}
}

void DrawFloor()
{
	// "Controlled planet" facility floor: tile the ground around the camera
	// so it keeps covering the screen as the player roams the open world.
	// Two alternating tile variants (plain / grate) break up the repetition
	// so motion across the floor is easier to read.
	// Tiles are visited in back-to-front order (ascending grid depth, i.e.
	// gridX + gridY) so overlapping diamond sprites layer correctly.
	int centerGridX = (int)floorf(g_PlayerGridX);
	int centerGridY = (int)floorf(g_PlayerGridY);
	int range = (int)(WINDOW_SIZE / 2.f / (TILE_H / 2.f)) + 2;

	int minSum = -range * 2;
	int maxSum = range * 2;
	for (int sum = minSum; sum <= maxSum; sum++)
	{
		for (int gx = -range; gx <= range; gx++)
		{
			int gy = sum - gx;
			if (gy < -range || gy > range) continue;

			int tileX = centerGridX + gx;
			int tileY = centerGridY + gy;

			float screenX, screenY;
			IsoToScreen((float)tileX, (float)tileY, &screenX, &screenY);

			bool variantB = ((tileX * 31 + tileY * 17) % 7) == 0;
			g_Renderer->DrawSprite(screenX, screenY, 0, TILE_W, TILE_H,
				variantB ? g_FloorTextureB : g_FloorTextureA);
		}
	}
}

// Draws one line of text anchored so (x, y) is the vertical-center-left edge
// of the string, in WORLD/grid-camera space (scrolls with the world).
void DrawText(float x, float y, const char* text, float scale, float r, float g, float b, float a = 1.0f)
{
	float cursorX = x + (FONT_CELL_W * scale) / 2.f;
	for (const char* c = text; *c != '\0'; c++)
	{
		int index = (unsigned char)*c - FONT_FIRST_CHAR;
		if (index >= 0 && index < FONT_COLS * FONT_ROWS)
		{
			int col = index % FONT_COLS;
			int row = index / FONT_COLS;
			// LoadTexture flips images vertically on load (stb_image), so texture
			// row 0 is the atlas image's LAST row, not its first - flip here to
			// compensate and land on the glyph cell we actually meant.
			int flippedRow = (FONT_ROWS - 1) - row;
			float u0 = col * FONT_CELL_W / FONT_ATLAS_W;
			float v0 = flippedRow * FONT_CELL_H / FONT_ATLAS_H;
			float u1 = u0 + FONT_CELL_W / FONT_ATLAS_W;
			float v1 = v0 + FONT_CELL_H / FONT_ATLAS_H;

			g_Renderer->DrawSpriteRegion(cursorX, y, 0, FONT_CELL_W * scale, FONT_CELL_H * scale,
				g_FontTexture, u0, v0, u1, v1, a, r, g, b);
		}
		cursorX += FONT_CELL_W * scale;
	}
}

// HUD helpers: (offsetX, offsetY) are fixed SCREEN pixels regardless of where
// the camera/player currently is, by re-adding the camera's current world
// position before handing off to the normal (camera-relative) draw calls.
void GetCameraWorldPos(float* camX, float* camY)
{
	IsoToScreen(g_PlayerGridX, g_PlayerGridY, camX, camY);
}

void DrawTextScreen(float offsetX, float offsetY, const char* text, float scale, float r, float g, float b, float a = 1.0f)
{
	float camX, camY;
	GetCameraWorldPos(&camX, &camY);
	DrawText(camX + offsetX, camY + offsetY, text, scale, r, g, b, a);
}

void DrawRectScreen(float offsetX, float offsetY, float w, float h, float r, float g, float b, float a)
{
	float camX, camY;
	GetCameraWorldPos(&camX, &camY);
	g_Renderer->DrawSprite(camX + offsetX, camY + offsetY, 0, w, h, g_WhiteTexture, a, r, g, b);
}

void DrawHUD()
{
	// Half window in screen pixels, used to anchor HUD elements to corners.
	// (Vignette darkening/blur and the light-bloom glow are now real
	// post-process effects - see Renderer::EndSceneCaptureAndComposite -
	// rather than a flat overlay sprite drawn here.)
	const float HALF = WINDOW_SIZE / 2.f;

	// Title block, top-left.
	DrawTextScreen(-HALF + 14, HALF - 20, "STAR DRIFTER", 1.4f, 0.85f, 0.95f, 1.0f);
	DrawTextScreen(-HALF + 14, HALF - 36, "PROTOTYPE BUILD", 0.9f, 0.55f, 0.6f, 0.65f);

	// Controls hint, bottom-left.
	DrawTextScreen(-HALF + 14, -HALF + 16, "WASD / ARROWS - MOVE", 0.9f, 0.6f, 0.65f, 0.7f);

	// "SIGNAL" meter, top-right - a visual nod to the protagonist's glowing
	// mark/mystery. Purely cosmetic for now (no gameplay meaning yet).
	float barW = 90.f, barH = 10.f;
	float barOffsetX = HALF - 14 - barW / 2.f;
	float barOffsetY = HALF - 26;
	DrawTextScreen(HALF - 14 - barW, HALF - 14, "SIGNAL", 0.85f, 0.5f, 0.85f, 0.9f);
	DrawRectScreen(barOffsetX, barOffsetY, barW, barH, 0.08f, 0.1f, 0.13f, 0.85f);

	float t = glutGet(GLUT_ELAPSED_TIME) / 1000.f;
	float fill = 0.45f + 0.25f * (0.5f + 0.5f * sinf(t * 1.3f));
	float fillW = barW * fill;
	DrawRectScreen(barOffsetX - (barW - fillW) / 2.f, barOffsetY, fillW, barH - 4, 0.35f, 0.9f, 1.0f, 0.9f);
}

void RenderScene(void)
{
	// Draw the whole world into an offscreen texture first, so it can be
	// blurred/composited (bloom + blurred, darkened vignette) as a post-process
	// step below, before the HUD is drawn on top (sharp, unaffected by that).
	g_Renderer->BeginSceneCapture();

	glClearColor(0.0f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	DrawFloor();

	// Everything that stands on the floor (props, their light glow, companion,
	// player) is depth-sorted together by grid depth (gridX + gridY) so the
	// player can correctly walk in front of / behind pillars and crates.
	struct Sortable
	{
		float depth;
		float gridX, gridY;
		float pixelOffsetX, pixelOffsetY;
		float w, h;
		GLuint texture;
		float alpha, r, g, b;
	};
	std::vector<Sortable> drawList;
	drawList.reserve(g_EnvObjects.size() * 2 + 2);

	float t = glutGet(GLUT_ELAPSED_TIME) / 1000.f;

	for (auto& obj : g_EnvObjects)
	{
		float depth = obj.gridX + obj.gridY;
		drawList.push_back({ depth, obj.gridX, obj.gridY, 0, 0, obj.w, obj.h, obj.texture, 1, 1, 1, 1 });

		if (obj.isLightSource)
		{
			// Pulsing glow, phase-offset per pillar so they don't all pulse in lockstep.
			float phase = obj.gridX * 13.f + obj.gridY * 7.f;
			float pulse = 0.5f + 0.5f * sinf(t * 2.0f + phase);
			float glowAlpha = 0.30f + 0.25f * pulse;
			float glowSize = 70.f + 14.f * pulse;
			// Anchored near the pillar's light head, above its center.
			drawList.push_back({ depth + 0.001f, obj.gridX, obj.gridY, 0, -31, glowSize, glowSize,
				g_GlowTexture, glowAlpha, 1, 1, 1 });
		}
	}

	int nowMs = glutGet(GLUT_ELAPSED_TIME);
	GLuint playerTex = (nowMs < g_PlayerAnimMovingUntilMs)
		? (g_PlayerAnimFrame ? g_PlayerWalk2 : g_PlayerWalk1)
		: g_PlayerTexture;
	GLuint companionTex = g_CompanionAnimFrame ? g_CompanionWalk2 : g_CompanionWalk1;

	drawList.push_back({ g_PlayerGridX + g_PlayerGridY, g_PlayerGridX, g_PlayerGridY, 0, 0, 40, 56, playerTex, 1, 1, 1, 1 });
	drawList.push_back({ g_CompanionGridX + g_CompanionGridY, g_CompanionGridX, g_CompanionGridY, 0, 0, 40, 56, companionTex, 1, 1, 1, 1 });

	std::sort(drawList.begin(), drawList.end(), [](const Sortable& a, const Sortable& b) {
		return a.depth < b.depth;
	});

	for (auto& item : drawList)
	{
		float screenX, screenY;
		IsoToScreen(item.gridX, item.gridY, &screenX, &screenY);
		g_Renderer->DrawSprite(screenX + item.pixelOffsetX, screenY + item.pixelOffsetY, 0,
			item.w, item.h, item.texture, item.alpha, item.r, item.g, item.b);
	}

	g_Renderer->EndSceneCaptureAndComposite();

	// HUD drawn directly to the screen after compositing, so text stays crisp.
	DrawHUD();

	glutSwapBuffers();
}

void Idle(void)
{
	UpdateCompanion();
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
	case 'w': MovePlayerScreenRelative(0, -1); break;
	case 's': MovePlayerScreenRelative(0, 1); break;
	case 'a': MovePlayerScreenRelative(-1, 0); break;
	case 'd': MovePlayerScreenRelative(1, 0); break;
	}

	RenderScene();
}

void SpecialKeyInput(int key, int x, int y)
{
	switch (key)
	{
	case GLUT_KEY_UP: MovePlayerScreenRelative(0, -1); break;
	case GLUT_KEY_DOWN: MovePlayerScreenRelative(0, 1); break;
	case GLUT_KEY_LEFT: MovePlayerScreenRelative(-1, 0); break;
	case GLUT_KEY_RIGHT: MovePlayerScreenRelative(1, 0); break;
	}

	RenderScene();
}

void SetupEnvironment()
{
	// Landmarks scattered around the spawn area so the player has visual
	// reference points to judge movement/direction against.
	g_EnvObjects.push_back({ 3.f, 1.f, 48, 96, g_PillarTexture, true });
	g_EnvObjects.push_back({ -2.f, 3.f, 48, 96, g_PillarTexture, true });
	g_EnvObjects.push_back({ 5.f, -2.f, 48, 96, g_PillarTexture, true });
	g_EnvObjects.push_back({ -4.f, -3.f, 48, 96, g_PillarTexture, true });
	g_EnvObjects.push_back({ 1.5f, 4.f, 44, 40, g_CrateTexture, false });
	g_EnvObjects.push_back({ -3.f, 0.5f, 44, 40, g_CrateTexture, false });
	g_EnvObjects.push_back({ 2.5f, -3.5f, 44, 40, g_CrateTexture, false });
	g_EnvObjects.push_back({ -1.f, -4.f, 44, 40, g_CrateTexture, false });
	g_EnvObjects.push_back({ 6.f, 2.f, 44, 40, g_CrateTexture, false });
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
	g_FloorTextureA = g_Renderer->LoadTexture("./Resources/Textures/floor_facility.png");
	g_FloorTextureB = g_Renderer->LoadTexture("./Resources/Textures/floor_facility_b.png");
	g_PlayerTexture = g_Renderer->LoadTexture("./Resources/Textures/player.png");
	g_PlayerWalk1 = g_Renderer->LoadTexture("./Resources/Textures/player_walk1.png");
	g_PlayerWalk2 = g_Renderer->LoadTexture("./Resources/Textures/player_walk2.png");
	g_CompanionTexture = g_Renderer->LoadTexture("./Resources/Textures/companion.png");
	g_CompanionWalk1 = g_Renderer->LoadTexture("./Resources/Textures/companion_walk1.png");
	g_CompanionWalk2 = g_Renderer->LoadTexture("./Resources/Textures/companion_walk2.png");
	g_PillarTexture = g_Renderer->LoadTexture("./Resources/Textures/pillar.png");
	g_CrateTexture = g_Renderer->LoadTexture("./Resources/Textures/crate.png");
	g_GlowTexture = g_Renderer->LoadTexture("./Resources/Textures/glow.png");
	g_WhiteTexture = g_Renderer->LoadTexture("./Resources/Textures/white_pixel.png");
	g_FontTexture = g_Renderer->LoadTexture("./Resources/Textures/font_atlas.png");

	SetupEnvironment();

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyInput);
	glutMouseFunc(MouseInput);
	glutSpecialFunc(SpecialKeyInput);

	glutMainLoop();

	delete g_Renderer;

    return 0;
}
