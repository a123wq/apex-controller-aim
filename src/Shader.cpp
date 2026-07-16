#include "App.hpp"
#include "gl33.h"
#include <fstream>
#include <sstream>
#include <cstring>

Shader::~Shader() {
    if (m_id != 0) glDeleteProgram(m_id);
}

bool Shader::compile(const char* vertexSource, const char* fragmentSource) {
    if (m_id != 0) { glDeleteProgram(m_id); m_id = 0; }

    auto compileShader = [](GLenum type, const char* src) -> unsigned int {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint ok;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(shader, 512, nullptr, log);
            fprintf(stderr, "[Shader] Compilation error: %s\n", log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    };

    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vs == 0 || fs == 0) return false;

    m_id = glCreateProgram();
    glAttachShader(m_id, vs);
    glAttachShader(m_id, fs);
    glLinkProgram(m_id);

    GLint ok;
    glGetProgramiv(m_id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(m_id, 512, nullptr, log);
        fprintf(stderr, "[Shader] Link error: %s\n", log);
        glDeleteProgram(m_id); m_id = 0;
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return true;
}

void Shader::bind() const { glUseProgram(m_id); }
void Shader::unbind() const { glUseProgram(0); }

int Shader::getUniformLocation(const char* name) const {
    return glGetUniformLocation(m_id, name);
}

void Shader::setUniform1i(const char* name, int value) {
    glUniform1i(getUniformLocation(name), value);
}

void Shader::setUniform1f(const char* name, float value) {
    glUniform1f(getUniformLocation(name), value);
}

void Shader::setUniform3f(const char* name, float x, float y, float z) {
    glUniform3f(getUniformLocation(name), x, y, z);
}

void Shader::setUniformMatrix4fv(const char* name, const glm::mat4& mat) {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
}
