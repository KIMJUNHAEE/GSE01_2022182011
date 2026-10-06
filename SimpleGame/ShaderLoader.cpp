#include "stdafx.h"
#include "ShaderLoader.h"
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    GLuint CompileShaderFile(GLenum type, const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            throw std::runtime_error(std::string("Cannot open shader: ") + path);
        }
        std::string source((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        if (file.bad())
        {
            throw std::runtime_error(std::string("Cannot read shader: ") + path);
        }
        if (source.compare(0, 3, "\xEF\xBB\xBF") == 0)
        {
            source.erase(0, 3);
        }
        if (source.empty() || source.size() > (size_t)(std::numeric_limits<GLint>::max)())
        {
            throw std::runtime_error(std::string("Invalid shader file size: ") + path);
        }
        GLuint shader = glCreateShader(type);
        if (!shader)
        {
            throw std::runtime_error(std::string("Cannot create shader: ") + path);
        }
        try
        {
            const char* text = source.c_str();
            GLint length = (GLint)source.size();
            glShaderSource(shader, 1, &text, &length);
            glCompileShader(shader);
            GLint compiled = GL_FALSE;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (!compiled)
            {
                GLint logLength = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
                std::vector<char> log(logLength > 0 ? logLength : 1, '\0');
                glGetShaderInfoLog(shader, (GLsizei)log.size(), nullptr, log.data());
                throw std::runtime_error(std::string("Shader compilation failed: ") + path + "\n" +
                                         log.data());
            }
        }
        catch (...)
        {
            glDeleteShader(shader);
            throw;
        }
        return shader;
    }
} // namespace

GLuint LoadShaderProgram(const char* vertexPath, const char* fragmentPath)
{
    GLuint vertex = 0, fragment = 0, program = 0;
    try
    {
        vertex = CompileShaderFile(GL_VERTEX_SHADER, vertexPath);
        fragment = CompileShaderFile(GL_FRAGMENT_SHADER, fragmentPath);
        program = glCreateProgram();
        if (!program)
        {
            throw std::runtime_error("Cannot create shader program");
        }
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            GLint logLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<char> log(logLength > 0 ? logLength : 1, '\0');
            glGetProgramInfoLog(program, (GLsizei)log.size(), nullptr, log.data());
            throw std::runtime_error(std::string("Shader link failed: ") + vertexPath + " + " +
                                     fragmentPath + "\n" + log.data());
        }
        glDetachShader(program, vertex);
        glDetachShader(program, fragment);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return program;
    }
    catch (...)
    {
        glDeleteProgram(program);
        if (vertex)
        {
            glDeleteShader(vertex);
        }
        if (fragment)
        {
            glDeleteShader(fragment);
        }
        throw;
    }
}
