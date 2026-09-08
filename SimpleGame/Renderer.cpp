#include "stdafx.h"
#include "Renderer.h"
#include "Dependencies\stb_image.h"

Renderer::Renderer(int windowSizeX, int windowSizeY)
{
	Initialize(windowSizeX, windowSizeY);
}


Renderer::~Renderer()
{
}

void Renderer::Initialize(int windowSizeX, int windowSizeY)
{
	//Set window size
	m_WindowSizeX = windowSizeX;
	m_WindowSizeY = windowSizeY;

	//Load shaders
	m_SolidRectShader = CompileShaders("./Shaders/SolidRect.vs", "./Shaders/SolidRect.fs");
	m_SpriteShader = CompileShaders("./Shaders/Sprite.vs", "./Shaders/Sprite.fs");
	m_BlurShader = CompileShaders("./Shaders/PostProcess.vs", "./Shaders/Blur.fs");
	m_CompositeShader = CompileShaders("./Shaders/PostProcess.vs", "./Shaders/Composite.fs");

	//Create VBOs
	CreateVertexBufferObjects();
	CreateSpriteVertexBufferObjects();
	CreateFullscreenQuad();
	CreateFramebuffers();

	//Allow textures with transparent/semi-transparent pixels (sprites) to blend with what's already drawn
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (m_SolidRectShader > 0 && m_VBORect > 0 && m_SpriteShader > 0 && m_VBOSprite > 0
		&& m_BlurShader > 0 && m_CompositeShader > 0 && m_SceneFBO > 0)
	{
		m_Initialized = true;
	}
}

bool Renderer::IsInitialized()
{
	return m_Initialized;
}

void Renderer::CreateVertexBufferObjects()
{
	float rect[]
		=
	{
		-1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f, -1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f, 1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f, //Triangle1
		-1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f,  1.f / m_WindowSizeX, 1.f / m_WindowSizeY, 0.f, 1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f, //Triangle2
	};

	glGenBuffers(1, &m_VBORect);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glBufferData(GL_ARRAY_BUFFER, sizeof(rect), rect, GL_STATIC_DRAW);
}

void Renderer::CreateSpriteVertexBufferObjects()
{
	// Same unit quad as the solid rect, but each vertex also carries a UV coordinate.
	// Layout per vertex: x, y, z, u, v
	float quad[]
		=
	{
		-1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f, 0.f, 0.f,
		-1.f / m_WindowSizeX,  1.f / m_WindowSizeY, 0.f, 0.f, 1.f,
		 1.f / m_WindowSizeX,  1.f / m_WindowSizeY, 0.f, 1.f, 1.f, //Triangle1

		-1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f, 0.f, 0.f,
		 1.f / m_WindowSizeX,  1.f / m_WindowSizeY, 0.f, 1.f, 1.f,
		 1.f / m_WindowSizeX, -1.f / m_WindowSizeY, 0.f, 1.f, 0.f, //Triangle2
	};

	glGenBuffers(1, &m_VBOSprite);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOSprite);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
}

void Renderer::CreateFullscreenQuad()
{
	// A quad spanning the full NDC range (-1..1), used by post-process passes
	// that don't need per-draw position/scale - just the whole screen.
	float quad[]
		=
	{
		-1.f, -1.f, 0.f, 0.f, 0.f,
		-1.f,  1.f, 0.f, 0.f, 1.f,
		 1.f,  1.f, 0.f, 1.f, 1.f,

		-1.f, -1.f, 0.f, 0.f, 0.f,
		 1.f,  1.f, 0.f, 1.f, 1.f,
		 1.f, -1.f, 0.f, 1.f, 0.f,
	};

	glGenBuffers(1, &m_VBOFullscreenQuad);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOFullscreenQuad);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
}

GLuint Renderer::CreateRenderTexture(int width, int height)
{
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

void Renderer::CreateFramebuffers()
{
	// Full-resolution target the normal scene gets drawn into.
	m_SceneColorTex = CreateRenderTexture(m_WindowSizeX, m_WindowSizeY);
	glGenFramebuffers(1, &m_SceneFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_SceneFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_SceneColorTex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "Scene framebuffer is incomplete.. \n";
	}

	// Small ping-pong targets for the separable blur (downsampled - the blur
	// doesn't need full resolution and this keeps it cheap).
	for (int i = 0; i < 2; i++)
	{
		m_BlurTex[i] = CreateRenderTexture(BLUR_TEX_SIZE, BLUR_TEX_SIZE);
		glGenFramebuffers(1, &m_BlurFBO[i]);
		glBindFramebuffer(GL_FRAMEBUFFER, m_BlurFBO[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_BlurTex[i], 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			std::cout << "Blur framebuffer " << i << " is incomplete.. \n";
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::DrawFullscreenQuad(GLuint shader)
{
	glBindBuffer(GL_ARRAY_BUFFER, m_VBOFullscreenQuad);

	int attribPosition = glGetAttribLocation(shader, "a_Position");
	int attribTexCoord = glGetAttribLocation(shader, "a_TexCoord");

	glEnableVertexAttribArray(attribPosition);
	glVertexAttribPointer(attribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, 0);

	glEnableVertexAttribArray(attribTexCoord);
	glVertexAttribPointer(attribTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 3));

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(attribPosition);
	glDisableVertexAttribArray(attribTexCoord);
}

void Renderer::BeginSceneCapture()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_SceneFBO);
	glViewport(0, 0, m_WindowSizeX, m_WindowSizeY);
}

void Renderer::EndSceneCaptureAndComposite(float bloomIntensity, float vignetteStrength, float vignetteBlur)
{
	// Separable Gaussian blur of the captured scene, ping-ponged a few times
	// between two small offscreen targets for a soft, wide result. The final
	// blurred image does double duty: it's used both as a cheap bloom/glow
	// source (added back in additively, weighted by its own brightness so it
	// concentrates around bright lights) and as the "blurry edges" vignette.
	glUseProgram(m_BlurShader);
	glUniform1i(glGetUniformLocation(m_BlurShader, "u_Texture"), 0);
	glActiveTexture(GL_TEXTURE0);

	glBindFramebuffer(GL_FRAMEBUFFER, m_BlurFBO[0]);
	glViewport(0, 0, BLUR_TEX_SIZE, BLUR_TEX_SIZE);
	glBindTexture(GL_TEXTURE_2D, m_SceneColorTex);
	glUniform2f(glGetUniformLocation(m_BlurShader, "u_TexelSize"), 1.f / m_WindowSizeX, 1.f / m_WindowSizeY);
	glUniform2f(glGetUniformLocation(m_BlurShader, "u_Direction"), 1.f, 0.f);
	DrawFullscreenQuad(m_BlurShader);

	GLuint texelUniform = glGetUniformLocation(m_BlurShader, "u_TexelSize");
	GLuint dirUniform = glGetUniformLocation(m_BlurShader, "u_Direction");
	glUniform2f(texelUniform, 1.f / BLUR_TEX_SIZE, 1.f / BLUR_TEX_SIZE);

	const int EXTRA_PASSES = 2;
	int srcIndex = 0;
	for (int i = 0; i < 1 + EXTRA_PASSES; i++)
	{
		int dstIndex = 1 - srcIndex;

		glBindFramebuffer(GL_FRAMEBUFFER, m_BlurFBO[dstIndex]);
		glBindTexture(GL_TEXTURE_2D, m_BlurTex[srcIndex]);
		glUniform2f(dirUniform, 0.f, 1.f);
		DrawFullscreenQuad(m_BlurShader);

		srcIndex = dstIndex;
		dstIndex = 1 - srcIndex;

		glBindFramebuffer(GL_FRAMEBUFFER, m_BlurFBO[dstIndex]);
		glBindTexture(GL_TEXTURE_2D, m_BlurTex[srcIndex]);
		glUniform2f(dirUniform, 1.f, 0.f);
		DrawFullscreenQuad(m_BlurShader);

		srcIndex = dstIndex;
	}

	// Composite scene + blurred pass onto the real backbuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_WindowSizeX, m_WindowSizeY);

	glUseProgram(m_CompositeShader);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_SceneColorTex);
	glUniform1i(glGetUniformLocation(m_CompositeShader, "u_Scene"), 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_BlurTex[srcIndex]);
	glUniform1i(glGetUniformLocation(m_CompositeShader, "u_Blurred"), 1);

	glUniform1f(glGetUniformLocation(m_CompositeShader, "u_BloomIntensity"), bloomIntensity);
	glUniform1f(glGetUniformLocation(m_CompositeShader, "u_VignetteStrength"), vignetteStrength);
	glUniform1f(glGetUniformLocation(m_CompositeShader, "u_VignetteBlur"), vignetteBlur);

	DrawFullscreenQuad(m_CompositeShader);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	//���̴� ������Ʈ ����
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		fprintf(stderr, "Error creating shader type %d\n", ShaderType);
	}

	const GLchar* p[1];
	p[0] = pShaderText;
	GLint Lengths[1];

	size_t slen = strlen(pShaderText);
	if (slen > INT_MAX) {
		// Handle error
	}
	GLint len = (GLint)slen;

	Lengths[0] = len;
	//���̴� �ڵ带 ���̴� ������Ʈ�� �Ҵ�
	glShaderSource(ShaderObj, 1, p, Lengths);

	//�Ҵ�� ���̴� �ڵ带 ������
	glCompileShader(ShaderObj);

	GLint success;
	// ShaderObj �� ���������� ������ �Ǿ����� Ȯ��
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024];

		//OpenGL �� shader log �����͸� ������
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
		printf("%s \n", pShaderText);
	}

	// ShaderProgram �� attach!!
	glAttachShader(ShaderProgram, ShaderObj);
}

bool Renderer::ReadFile(char* filename, std::string *target)
{
	std::ifstream file(filename);
	if (file.fail())
	{
		std::cout << filename << " file loading failed.. \n";
		file.close();
		return false;
	}
	std::string line;
	while (getline(file, line)) {
		target->append(line.c_str());
		target->append("\n");
	}
	return true;
}

GLuint Renderer::CompileShaders(char* filenameVS, char* filenameFS)
{
	GLuint ShaderProgram = glCreateProgram(); //�� ���̴� ���α׷� ����

	if (ShaderProgram == 0) { //���̴� ���α׷��� ����������� Ȯ��
		fprintf(stderr, "Error creating shader program\n");
	}

	std::string vs, fs;

	//shader.vs �� vs ������ �ε���
	if (!ReadFile(filenameVS, &vs)) {
		printf("Error compiling vertex shader\n");
		return -1;
	};

	//shader.fs �� fs ������ �ε���
	if (!ReadFile(filenameFS, &fs)) {
		printf("Error compiling fragment shader\n");
		return -1;
	};

	// ShaderProgram �� vs.c_str() ���ؽ� ���̴��� �������� ����� attach��
	AddShader(ShaderProgram, vs.c_str(), GL_VERTEX_SHADER);

	// ShaderProgram �� fs.c_str() �����׸�Ʈ ���̴��� �������� ����� attach��
	AddShader(ShaderProgram, fs.c_str(), GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { 0 };

	//Attach �Ϸ�� shaderProgram �� ��ŷ��
	glLinkProgram(ShaderProgram);

	//��ũ�� �����ߴ��� Ȯ��
	glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &Success);

	if (Success == 0) {
		// shader program �α׸� �޾ƿ�
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		std::cout << filenameVS << ", " << filenameFS << " Error linking shader program\n" << ErrorLog;
		return -1;
	}

	glValidateProgram(ShaderProgram);
	glGetProgramiv(ShaderProgram, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(ShaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		std::cout << filenameVS << ", " << filenameFS << " Error validating shader program\n" << ErrorLog;
		return -1;
	}

	glUseProgram(ShaderProgram);
	std::cout << filenameVS << ", " << filenameFS << " Shader compiling is done.";

	return ShaderProgram;
}

void Renderer::DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a)
{
	float newX, newY;

	GetGLPosition(x, y, &newX, &newY);

	//Program select
	glUseProgram(m_SolidRectShader);

	glUniform4f(glGetUniformLocation(m_SolidRectShader, "u_Trans"), newX, newY, 0, size);
	glUniform4f(glGetUniformLocation(m_SolidRectShader, "u_Color"), r, g, b, a);

	int attribPosition = glGetAttribLocation(m_SolidRectShader, "a_Position");
	glEnableVertexAttribArray(attribPosition);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBORect);
	glVertexAttribPointer(attribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(attribPosition);
}

void Renderer::GetGLPosition(float x, float y, float *newX, float *newY)
{
	// Offset by the camera first so (x, y) is treated as a world-space position,
	// then convert the camera-relative position into NDC screen space.
	*newX = (x - m_CameraX) * 2.f / m_WindowSizeX;
	*newY = (y - m_CameraY) * 2.f / m_WindowSizeY;
}

void Renderer::SetCameraPosition(float x, float y)
{
	m_CameraX = x;
	m_CameraY = y;
}

GLuint Renderer::LoadTexture(const char* filename)
{
	// Flip on load: image files store row 0 at the top, but OpenGL texture
	// coordinate (0,0) is the bottom-left corner.
	stbi_set_flip_vertically_on_load(true);

	int width, height, channels;
	unsigned char* data = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);
	if (data == NULL)
	{
		std::cout << filename << " texture loading failed.. \n";
		return 0;
	}

	GLuint textureId;
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// Nearest filtering keeps the pixel-art sprites and bitmap font crisp
	// instead of blurring them when scaled.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	stbi_image_free(data);
	glBindTexture(GL_TEXTURE_2D, 0);

	return textureId;
}

void Renderer::DrawSprite(float x, float y, float z, float width, float height, GLuint textureId,
	float alpha, float r, float g, float b)
{
	DrawSpriteRegion(x, y, z, width, height, textureId, 0.f, 0.f, 1.f, 1.f, alpha, r, g, b);
}

void Renderer::DrawSpriteRegion(float x, float y, float z, float width, float height, GLuint textureId,
	float u0, float v0, float u1, float v1, float alpha, float r, float g, float b)
{
	float newX, newY;

	GetGLPosition(x, y, &newX, &newY);

	glUseProgram(m_SpriteShader);

	glUniform4f(glGetUniformLocation(m_SpriteShader, "u_Trans"), newX, newY, width, height);
	glUniform4f(glGetUniformLocation(m_SpriteShader, "u_UVRect"), u0, v0, u1 - u0, v1 - v0);
	glUniform1f(glGetUniformLocation(m_SpriteShader, "u_Alpha"), alpha);
	glUniform3f(glGetUniformLocation(m_SpriteShader, "u_ColorTint"), r, g, b);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glUniform1i(glGetUniformLocation(m_SpriteShader, "u_Texture"), 0);

	int attribPosition = glGetAttribLocation(m_SpriteShader, "a_Position");
	int attribTexCoord = glGetAttribLocation(m_SpriteShader, "a_TexCoord");

	glBindBuffer(GL_ARRAY_BUFFER, m_VBOSprite);

	glEnableVertexAttribArray(attribPosition);
	glVertexAttribPointer(attribPosition, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, 0);

	glEnableVertexAttribArray(attribTexCoord);
	glVertexAttribPointer(attribTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 3));

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(attribPosition);
	glDisableVertexAttribArray(attribTexCoord);

	glBindTexture(GL_TEXTURE_2D, 0);
}