// =============================================================================
// gl33.h — OpenGL 3.3 Core function pointer declarations + GL33 loader
// =============================================================================
// All OpenGL 3.3 functions are loaded via SDL_GL_GetProcAddress at runtime.
// This header is self-contained — no GL/glew dependency required.
// =============================================================================
#pragma once

#include <SDL.h>

#ifdef __cplusplus
#include <cstdint>
typedef std::uint32_t GLenum;
typedef std::uint32_t GLbitfield;
typedef std::uint32_t GLuint;
typedef std::int32_t  GLint;
typedef std::int32_t  GLsizei;
typedef std::uint32_t GLsizeiptr;
typedef std::int32_t  GLintptr;
typedef std::int8_t   GLboolean;
typedef float         GLfloat;
typedef unsigned char GLubyte;
typedef char          GLchar;
#else
typedef unsigned int   GLenum;
typedef unsigned int   GLbitfield;
typedef unsigned int   GLuint;
typedef int            GLint;
typedef int            GLsizei;
typedef std::uint32_t  GLsizeiptr;
typedef std::int32_t   GLintptr;
typedef unsigned char  GLboolean;
typedef float          GLfloat;
typedef unsigned char  GLubyte;
typedef char           GLchar;
#endif

// ---------------------------------------------------------------------------
// OpenGL constants (3.3 Core subset)
// ---------------------------------------------------------------------------
#define GL_DEPTH_TEST      0x0B71
#define GL_LEQUAL          0x0203
#define GL_CULL_FACE       0x0B44
#define GL_BACK            0x0405
#define GL_BLEND           0x0BE2
#define GL_SRC_ALPHA       0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ARRAY_BUFFER    0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW     0x88E4
#define GL_FLOAT           0x1406
#define GL_TRIANGLES       0x0004
#define GL_FALSE          0
#define GL_TRUE           1
#define GL_LINES          0x0001
#define GL_TRIANGLES      0x0004
#define GL_UNSIGNED_INT   0x1405
#define GL_FLOAT          0x1406
#define GL_COMPILE_STATUS  0x8B81
#define GL_LINK_STATUS     0x8B82
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER   0x8B31
#define GL_NO_ERROR        0
#define GL_VENDOR          0x1F00
#define GL_RENDERER        0x1F01
#define GL_VERSION         0x1F02
#define GL_COLOR_BUFFER_BIT  0x00004000
#define GL_DEPTH_BUFFER_BIT  0x00000100
typedef void   (*PFN_glDepthFunc)(GLenum);

// ---------------------------------------------------------------------------
// Function pointer types (plain convention for MinGW)
// ---------------------------------------------------------------------------
typedef void   (*PFN_glCullFace)(GLenum);
typedef void   (*PFN_glEnable)(GLenum);
typedef void   (*PFN_glDisable)(GLenum);
typedef void   (*PFN_glBlendFunc)(GLenum, GLenum);
typedef void   (*PFN_glViewport)(GLint, GLint, GLsizei, GLsizei);
typedef void   (*PFN_glClear)(GLbitfield);
typedef void   (*PFN_glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef GLenum (*PFN_glGetError)(void);
typedef const GLubyte* (*PFN_glGetString)(GLenum);
typedef void   (*PFN_glGenVertexArrays)(GLsizei, GLuint*);
typedef void   (*PFN_glBindVertexArray)(GLuint);
typedef void   (*PFN_glDeleteVertexArrays)(GLsizei, const GLuint*);
typedef void   (*PFN_glGenBuffers)(GLsizei, GLuint*);
typedef void   (*PFN_glBindBuffer)(GLenum, GLuint);
typedef void   (*PFN_glDeleteBuffers)(GLsizei, const GLuint*);
typedef void   (*PFN_glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void   (*PFN_glBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*);
typedef void   (*PFN_glEnableVertexAttribArray)(GLuint);
typedef void   (*PFN_glDisableVertexAttribArray)(GLuint);
typedef void   (*PFN_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef GLuint (*PFN_glCreateShader)(GLenum);
typedef void   (*PFN_glDeleteShader)(GLuint);
typedef void   (*PFN_glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void   (*PFN_glCompileShader)(GLuint);
typedef void   (*PFN_glAttachShader)(GLuint, GLuint);
typedef void   (*PFN_glDetachShader)(GLuint, GLuint);
typedef GLuint (*PFN_glCreateProgram)(void);
typedef void   (*PFN_glLinkProgram)(GLuint);
typedef void   (*PFN_glDeleteProgram)(GLuint);
typedef void   (*PFN_glGetShaderiv)(GLuint, GLenum, GLint*);
typedef void   (*PFN_glGetProgramiv)(GLuint, GLenum, GLint*);
typedef void   (*PFN_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (*PFN_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLint  (*PFN_glGetUniformLocation)(GLuint, const GLchar*);
typedef void   (*PFN_glUniform1f)(GLint, GLfloat);
typedef void   (*PFN_glUniform2f)(GLint, GLfloat, GLfloat);
typedef void   (*PFN_glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void   (*PFN_glUniform1i)(GLint, GLint);
typedef void   (*PFN_glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void   (*PFN_glUseProgram)(GLuint);
typedef void   (*PFN_glDrawArrays)(GLenum, GLint, GLsizei);
typedef void   (*PFN_glDrawElements)(GLenum, GLsizei, GLenum, const void*);

// ---------------------------------------------------------------------------
// GL33 struct — all function pointers
// ---------------------------------------------------------------------------
struct GL33 {
    PFN_glCullFace             glCullFace             = nullptr;
    PFN_glDepthFunc            glDepthFunc            = nullptr;
    PFN_glEnable               glEnable               = nullptr;
    PFN_glDisable              glDisable              = nullptr;
    PFN_glBlendFunc            glBlendFunc            = nullptr;
    PFN_glViewport             glViewport             = nullptr;
    PFN_glClear                glClear                = nullptr;
    PFN_glClearColor           glClearColor           = nullptr;
    PFN_glGetError             glGetError             = nullptr;
    PFN_glGetString            glGetString            = nullptr;
    PFN_glGenVertexArrays      glGenVertexArrays      = nullptr;
    PFN_glBindVertexArray      glBindVertexArray      = nullptr;
    PFN_glDeleteVertexArrays   glDeleteVertexArrays   = nullptr;
    PFN_glGenBuffers           glGenBuffers           = nullptr;
    PFN_glBindBuffer           glBindBuffer           = nullptr;
    PFN_glDeleteBuffers        glDeleteBuffers        = nullptr;
    PFN_glBufferData           glBufferData           = nullptr;
    PFN_glBufferSubData        glBufferSubData        = nullptr;
    PFN_glEnableVertexAttribArray  glEnableVertexAttribArray  = nullptr;
    PFN_glDisableVertexAttribArray glDisableVertexAttribArray = nullptr;
    PFN_glVertexAttribPointer  glVertexAttribPointer  = nullptr;
    PFN_glCreateShader         glCreateShader         = nullptr;
    PFN_glDeleteShader         glDeleteShader         = nullptr;
    PFN_glShaderSource         glShaderSource         = nullptr;
    PFN_glCompileShader        glCompileShader        = nullptr;
    PFN_glAttachShader         glAttachShader         = nullptr;
    PFN_glDetachShader         glDetachShader         = nullptr;
    PFN_glCreateProgram        glCreateProgram        = nullptr;
    PFN_glLinkProgram          glLinkProgram          = nullptr;
    PFN_glDeleteProgram        glDeleteProgram        = nullptr;
    PFN_glGetShaderiv          glGetShaderiv          = nullptr;
    PFN_glGetProgramiv         glGetProgramiv         = nullptr;
    PFN_glGetShaderInfoLog     glGetShaderInfoLog     = nullptr;
    PFN_glGetProgramInfoLog    glGetProgramInfoLog    = nullptr;
    PFN_glGetUniformLocation   glGetUniformLocation   = nullptr;
    PFN_glUniform1f            glUniform1f            = nullptr;
    PFN_glUniform2f            glUniform2f            = nullptr;
    PFN_glUniform3f            glUniform3f            = nullptr;
    PFN_glUniform1i            glUniform1i            = nullptr;
    PFN_glUniformMatrix4fv     glUniformMatrix4fv     = nullptr;
    PFN_glUseProgram           glUseProgram           = nullptr;
    PFN_glDrawArrays           glDrawArrays           = nullptr;
    PFN_glDrawElements         glDrawElements         = nullptr;

    bool load();
};

extern GL33 gGL;

// ---------------------------------------------------------------------------
// Inline wrappers so plain glXxx() calls work
// ---------------------------------------------------------------------------
inline void glCullFace(GLenum mode)                        { gGL.glCullFace(mode); }
inline void glDepthFunc(GLenum func)                       { gGL.glDepthFunc(func); }
inline void glEnable(GLenum cap)                           { gGL.glEnable(cap); }
inline void glDisable(GLenum cap)                          { gGL.glDisable(cap); }
inline void glBlendFunc(GLenum sf, GLenum df)              { gGL.glBlendFunc(sf, df); }
inline void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { gGL.glViewport(x, y, w, h); }
inline void glClear(GLbitfield mask)                       { gGL.glClear(mask); }
inline void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { gGL.glClearColor(r, g, b, a); }
inline GLenum glGetError()                                 { return gGL.glGetError ? gGL.glGetError() : 0; }
inline const GLubyte* glGetString(GLenum n)                { return gGL.glGetString ? gGL.glGetString(n) : nullptr; }
inline void glGenVertexArrays(GLsizei n, GLuint* a)        { gGL.glGenVertexArrays(n, a); }
inline void glBindVertexArray(GLuint a)                    { gGL.glBindVertexArray(a); }
inline void glDeleteVertexArrays(GLsizei n, const GLuint* a) { gGL.glDeleteVertexArrays(n, a); }
inline void glGenBuffers(GLsizei n, GLuint* b)             { gGL.glGenBuffers(n, b); }
inline void glBindBuffer(GLenum t, GLuint b)               { gGL.glBindBuffer(t, b); }
inline void glDeleteBuffers(GLsizei n, const GLuint* b)    { gGL.glDeleteBuffers(n, b); }
inline void glBufferData(GLenum t, GLsizeiptr s, const void* d, GLenum u) { gGL.glBufferData(t, s, d, u); }
inline void glBufferSubData(GLenum t, GLintptr o, GLsizeiptr s, const void* d) { gGL.glBufferSubData(t, o, s, d); }
inline void glEnableVertexAttribArray(GLuint i)            { gGL.glEnableVertexAttribArray(i); }
inline void glDisableVertexAttribArray(GLuint i)           { gGL.glDisableVertexAttribArray(i); }
inline void glVertexAttribPointer(GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void* p) { gGL.glVertexAttribPointer(i, s, t, n, st, p); }
inline GLuint glCreateShader(GLenum t)                     { return gGL.glCreateShader ? gGL.glCreateShader(t) : 0; }
inline void glDeleteShader(GLuint s)                       { if (gGL.glDeleteShader) gGL.glDeleteShader(s); }
inline void glShaderSource(GLuint s, GLsizei c, const GLchar* const* src, const GLint* l) { gGL.glShaderSource(s, c, src, l); }
inline void glCompileShader(GLuint s)                      { gGL.glCompileShader(s); }
inline void glAttachShader(GLuint p, GLuint s)             { gGL.glAttachShader(p, s); }
inline void glDetachShader(GLuint p, GLuint s)             { gGL.glDetachShader(p, s); }
inline GLuint glCreateProgram()                            { return gGL.glCreateProgram ? gGL.glCreateProgram() : 0; }
inline void glLinkProgram(GLuint p)                        { gGL.glLinkProgram(p); }
inline void glDeleteProgram(GLuint p)                      { if (gGL.glDeleteProgram) gGL.glDeleteProgram(p); }
inline void glGetShaderiv(GLuint s, GLenum p, GLint* v)    { gGL.glGetShaderiv(s, p, v); }
inline void glGetProgramiv(GLuint p, GLenum pl, GLint* v)  { gGL.glGetProgramiv(p, pl, v); }
inline void glGetShaderInfoLog(GLuint s, GLsizei b, GLsizei* l, GLchar* il) { gGL.glGetShaderInfoLog(s, b, l, il); }
inline void glGetProgramInfoLog(GLuint p, GLsizei b, GLsizei* l, GLchar* il) { gGL.glGetProgramInfoLog(p, b, l, il); }
inline GLint glGetUniformLocation(GLuint p, const GLchar* n){ return gGL.glGetUniformLocation ? gGL.glGetUniformLocation(p, n) : -1; }
inline void glUniform1f(GLint l, GLfloat v)                { gGL.glUniform1f(l, v); }
inline void glUniform2f(GLint l, GLfloat x, GLfloat y)     { gGL.glUniform2f(l, x, y); }
inline void glUniform3f(GLint l, GLfloat x, GLfloat y, GLfloat z) { gGL.glUniform3f(l, x, y, z); }
inline void glUniform1i(GLint l, GLint v)                  { gGL.glUniform1i(l, v); }
inline void glUniformMatrix4fv(GLint l, GLsizei c, GLboolean t, const GLfloat* v) { gGL.glUniformMatrix4fv(l, c, t, v); }
inline void glUseProgram(GLuint p)                         { gGL.glUseProgram(p); }
inline void glDrawArrays(GLenum mode, GLint first, GLsizei count) { gGL.glDrawArrays(mode, first, count); }
inline void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) { gGL.glDrawElements(mode, count, type, indices); }

// Without this #define, Renderer.cpp's static_assert for glad.h fails
#define GLAD_GL_IMPLEMENTATION