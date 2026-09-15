#pragma once
#include "Dependencies/glew.h"

// Loads independent shader files. The caller owns the returned program.
// Throws with the source path and compiler/linker log; failures release partial GL objects.
GLuint LoadShaderProgram(const char* vertexPath, const char* fragmentPath);
