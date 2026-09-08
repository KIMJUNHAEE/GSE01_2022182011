#pragma once

#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Dependencies\glew.h"

class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized();
	void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a);

	// Loads an image file (png/jpg/bmp/tga...) as a GL texture. Returns 0 on failure.
	GLuint LoadTexture(const char* filename);
	void DrawSprite(float x, float y, float z, float width, float height, GLuint textureId,
		float alpha = 1.0f, float r = 1.0f, float g = 1.0f, float b = 1.0f);

	// Draws only a UV sub-rectangle of the texture (u0,v0)-(u1,v1) instead of the
	// whole thing. Used for sprite atlases/bitmap fonts; DrawSprite is just this
	// with the full-texture rect (0,0)-(1,1). r/g/b tint the texture (white = no tint).
	void DrawSpriteRegion(float x, float y, float z, float width, float height, GLuint textureId,
		float u0, float v0, float u1, float v1, float alpha = 1.0f,
		float r = 1.0f, float g = 1.0f, float b = 1.0f);

	// DrawSolidRect/DrawSprite take world-space coordinates; the camera position is
	// subtracted before converting to screen space, so setting the camera to the
	// player's world position keeps the player centered as the world scrolls.
	void SetCameraPosition(float x, float y);

	// Post-processing: redirects DrawSolidRect/DrawSprite calls into an offscreen
	// scene texture instead of the screen. Call this, do the frame's normal world
	// drawing, then call EndSceneCaptureAndComposite() to blur+composite that
	// texture (bloom/glow + blurred, darkened vignette) onto the real screen.
	// Anything drawn AFTER EndSceneCaptureAndComposite() (e.g. HUD text) goes
	// straight to the screen, unaffected by the blur.
	void BeginSceneCapture();
	void EndSceneCaptureAndComposite(float bloomIntensity = 0.45f, float vignetteStrength = 0.55f, float vignetteBlur = 0.55f);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	bool ReadFile(char* filename, std::string *target);
	void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType);
	GLuint CompileShaders(char* filenameVS, char* filenameFS);
	void CreateVertexBufferObjects();
	void CreateSpriteVertexBufferObjects();
	void GetGLPosition(float x, float y, float *newX, float *newY);

	void CreateFullscreenQuad();
	void CreateFramebuffers();
	GLuint CreateRenderTexture(int width, int height);
	void DrawFullscreenQuad(GLuint shader);

	bool m_Initialized = false;

	unsigned int m_WindowSizeX = 0;
	unsigned int m_WindowSizeY = 0;

	GLuint m_VBORect = 0;
	GLuint m_SolidRectShader = 0;

	GLuint m_VBOSprite = 0;
	GLuint m_SpriteShader = 0;

	float m_CameraX = 0.f;
	float m_CameraY = 0.f;

	// Post-processing (bloom + blurred vignette)
	GLuint m_VBOFullscreenQuad = 0;
	GLuint m_BlurShader = 0;
	GLuint m_CompositeShader = 0;

	GLuint m_SceneFBO = 0;
	GLuint m_SceneColorTex = 0;

	static const int BLUR_TEX_SIZE = 128;
	GLuint m_BlurFBO[2] = { 0, 0 };
	GLuint m_BlurTex[2] = { 0, 0 };
};

