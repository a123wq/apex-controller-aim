// =============================================================================
// GL.cpp — OpenGL 3.3 function pointer loader via SDL_GL_GetProcAddress
// =============================================================================
#define SDL_MAIN_HANDLED
#include "gl33.h"
#include <cstdio>

// Global GL33 instance (declared extern in gl33.h)
GL33 gGL;

// ---------------------------------------------------------------------------
static void* getGLProc(const char* name) {
    void* proc = SDL_GL_GetProcAddress(name);
    if (!proc) {
        fprintf(stderr, "[GL33] Failed to load: %s\n", name);
    }
    return proc;
}

#define LOAD(FN) gGL.FN = (PFN_##FN)getGLProc(#FN)

// ---------------------------------------------------------------------------
bool GL33::load() {

    // VAO
    LOAD(glGenVertexArrays);
    LOAD(glBindVertexArray);
    LOAD(glDeleteVertexArrays);

    // Buffers
    LOAD(glGenBuffers);
    LOAD(glBindBuffer);
    LOAD(glDeleteBuffers);
    LOAD(glBufferData);
    LOAD(glBufferSubData);

    // Vertex attrib
    LOAD(glEnableVertexAttribArray);
    LOAD(glDisableVertexAttribArray);
    LOAD(glVertexAttribPointer);

    // Shaders
    LOAD(glCreateShader);
    LOAD(glDeleteShader);
    LOAD(glShaderSource);
    LOAD(glCompileShader);
    LOAD(glAttachShader);
    LOAD(glDetachShader);

    // Program
    LOAD(glCreateProgram);
    LOAD(glLinkProgram);
    LOAD(glDeleteProgram);

    // Shader info
    LOAD(glGetShaderiv);
    LOAD(glGetProgramiv);
    LOAD(glGetShaderInfoLog);
    LOAD(glGetProgramInfoLog);

    // Uniforms
    LOAD(glGetUniformLocation);
    LOAD(glUniform1f);
    LOAD(glUniform2f);
    LOAD(glUniform3f);
    LOAD(glUniform1i);
    LOAD(glUniformMatrix4fv);

    // Program use
    LOAD(glUseProgram);

    // Drawing
    LOAD(glDrawArrays);
    LOAD(glDrawElements);

    // GL state (pre-loaded, not strictly needed but checked)
    LOAD(glCullFace);
    LOAD(glDepthFunc);
    LOAD(glEnable);
    LOAD(glDisable);
    LOAD(glBlendFunc);
    LOAD(glViewport);
    LOAD(glClear);
    LOAD(glClearColor);
    LOAD(glGetError);
    LOAD(glGetString);

    // Count nulls
    int nulls = 0;
    #define CHECK(FN) if (!gGL.FN) nulls++
    CHECK(glGenVertexArrays);
    CHECK(glBindVertexArray);
    CHECK(glDeleteVertexArrays);
    CHECK(glGenBuffers);
    CHECK(glBindBuffer);
    CHECK(glDeleteBuffers);
    CHECK(glBufferData);
    CHECK(glBufferSubData);
    CHECK(glEnableVertexAttribArray);
    CHECK(glDisableVertexAttribArray);
    CHECK(glVertexAttribPointer);
    CHECK(glCreateShader);
    CHECK(glDeleteShader);
    CHECK(glShaderSource);
    CHECK(glCompileShader);
    CHECK(glAttachShader);
    CHECK(glDetachShader);
    CHECK(glCreateProgram);
    CHECK(glLinkProgram);
    CHECK(glDeleteProgram);
    CHECK(glGetShaderiv);
    CHECK(glGetProgramiv);
    CHECK(glGetShaderInfoLog);
    CHECK(glGetProgramInfoLog);
    CHECK(glGetUniformLocation);
    CHECK(glUniform1f);
    CHECK(glUniform2f);
    CHECK(glUniform3f);
    CHECK(glUniform1i);
    CHECK(glUniformMatrix4fv);
    CHECK(glUseProgram);
    CHECK(glDrawArrays);
    CHECK(glDrawElements);
    CHECK(glCullFace);
    CHECK(glDepthFunc);
    CHECK(glEnable);
    CHECK(glDisable);
    CHECK(glBlendFunc);
    CHECK(glViewport);
    CHECK(glClear);
    CHECK(glClearColor);
    CHECK(glGetError);
    CHECK(glGetString);
    #undef CHECK

    if (nulls == 0) {
        fprintf(stderr, "[GL33] All %d OpenGL 3.3 functions loaded\n", 42);
    } else {
        fprintf(stderr, "[GL33] WARNING: %d/%d functions FAILED to load\n", nulls, 42);
    }

    return nulls == 0;
}