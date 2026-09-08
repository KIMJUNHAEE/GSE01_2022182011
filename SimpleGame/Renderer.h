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
	void DrawSprite(float x, float y, float z, float width, float height, GLuint textureId, float alpha = 1.0f);

	// DrawSolidRect/DrawSprite take world-space coordinates; the camera position is
	// subtracted before converting to screen space, so setting the camera to the
	// player's world position keeps the player centered as the world scrolls.
	void SetCameraPosition(float x, float y);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	bool ReadFile(char* filename, std::string *target);
	void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType);
	GLuint CompileShaders(char* filenameVS, char* filenameFS);
	void CreateVertexBufferObjects();
	void CreateSpriteVertexBufferObjects();
	void GetGLPosition(float x, float y, float *newX, float *newY);

	bool m_Initialized = false;

	unsigned int m_WindowSizeX = 0;
	unsigned int m_WindowSizeY = 0;

	GLuint m_VBORect = 0;
	GLuint m_SolidRectShader = 0;

	GLuint m_VBOSprite = 0;
	GLuint m_SpriteShader = 0;

	float m_CameraX = 0.f;
	float m_CameraY = 0.f;
};

