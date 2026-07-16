// =============================================================================
// gl33.h — OpenGL 3.3 Core function pointer declarations
//
// Usage:
//   In ONE .cpp file:   #include "GL.cpp"   (defines the function pointers)
//   In all other files: #include "gl33.h"    (declares extern references)
//
// Call GL33::load() AFTER creating an OpenGL 3.3 context to populate
// all function pointer variables. Use glXxx() inline functions below
// instead of calling GL33::glXxx directly for cleaner code.
// =============================================================================

#pragma once

// GL types from MinGW native headers
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

// =============================================================================
// Function pointer typedefs (OpenGL 3.3 Core subset)
// =============================================================================
typedef void    (APIENTRY* PFNGLGENVERTEXARRAYSPROC)        (GLsizei, GLuint*);
typedef void    (APIENTRY* PFNGLBINDVERTEXARRAYPROC)         (GLuint);
typedef void    (APIENTRY* PFNGLDELETEVERTEXARRAYSPROC)      (GLsizei, const GLuint*);
typedef void    (APIENTRY* PFNGLGENBUFFERSPROC)               (GLsizei, GLuint*);
typedef void    (APIENTRY* PFNGLBINDBUFFERPROC)               (GLenum, GLuint);
typedef void    (APIENTRY* PFNGLDELETEBUFFERSPROC)             (GLsizei, const GLuint*);
typedef void    (APIENTRY* PFNGLBUFFERDATAPROC)                (GLenum, GLsizeiptr, const void*, GLenum);
typedef void    (APIENTRY* PFNGLENABLEVERTEXATTRIBARRAYPROC)  (GLuint);
typedef void    (APIENTRY* PFNGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint);
typedef void    (APIENTRY* PFNGLVERTEXATTRIBPOINTERPROC)      (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef GLuint  (APIENTRY* PFNGLCREATESHADERPROC)              (GLenum);
typedef void    (APIENTRY* PFNGLDELETESHADERPROC)              (GLuint);
typedef void    (APIENTRY* PFNGLSHADERSOURCEPROC)              (GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void    (APIENTRY* PFNGLCOMPILESHADERPROC)             (GLuint);
typedef void    (APIENTRY* PFNGLATTACHSHADERPROC)             (GLuint, GLuint);
typedef void    (APIENTRY* PFNGLDETACHSHADERPROC)             (GLuint, GLuint);
typedef void    (APIENTRY* PFNGLDELETEPROGRAMPROC)             (GLuint);
typedef GLuint  (APIENTRY* PFNGLCREATEPROGRAMPROC)             (void);
typedef void    (APIENTRY* PFNGLLINKPROGRAMPROC)               (GLuint);
typedef void    (APIENTRY* PFNGLGETSHADERIVPROC)               (GLuint, GLenum, GLint*);
typedef void    (APIENTRY* PFNGLGETPROGRAMIVPROC)             (GLuint, GLenum, GLint*);
typedef void    (APIENTRY* PFNGLGETSHADERINFOLOGPROC)          (GLuint, GLsizei, GLsizei*, GLchar*);
typedef void    (APIENTRY* PFNGLGETPROGRAMINFOLOGPROC)         (GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLint   (APIENTRY* PFNGLGETUNIFORMLOCATIONPROC)       (GLuint, const GLchar*);
typedef void    (APIENTRY* PFNGLUNIFORM1FPROC)                (GLint, GLfloat);
typedef void    (APIENTRY* PFNGLUNIFORM3FPROC)                (GLint, GLfloat, GLfloat, GLfloat);
typedef void    (APIENTRY* PFNGLUNIFORM1IPROC)                (GLint, GLint);
typedef void    (APIENTRY* PFNGLUNIFORMMATRIX4FVPROC)          (GLint, GLsizei, GLboolean, const GLfloat*);
typedef void    (APIENTRY* PFNGLUSEPROGRAMPROC)               (GLuint);
typedef void    (APIENTRY* PFNGLGETERRORPROC)                 (void);
typedef const GLubyte* (APIENTRY* PFNGLGETSTRINGPROC)          (GLenum);

// =============================================================================
// GL33 — Function pointer struct
//
// In ONE .cpp:          #define GL33_IMPLEMENTATION
//                        #include "gl33.h"
//                        // Then call GL33::load() after OpenGL context is created
//
// In all other files:   #include "gl33.h"
//                        // Use the inline glXxx() functions below
// =============================================================================
struct GL33 {
    // Call once after context creation
    bool load();

    // Function pointers
    PFNGLGENVERTEXARRAYSPROC        glGenVertexArrays        = nullptr;
    PFNGLBINDVERTEXARRAYPROC         glBindVertexArray         = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays      = nullptr;
    PFNGLGENBUFFERSPROC              glGenBuffers              = nullptr;
    PFNGLBINDBUFFERPROC              glBindBuffer              = nullptr;
    PFNGLDELETEBUFFERSPROC           glDeleteBuffers           = nullptr;
    PFNGLBUFFERDATAPROC              glBufferData              = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
    PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer     = nullptr;
    PFNGLCREATESHADERPROC            glCreateShader            = nullptr;
    PFNGLDELETESHADERPROC            glDeleteShader            = nullptr;
    PFNGLSHADERSOURCEPROC            glShaderSource            = nullptr;
    PFNGLCOMPILESHADERPROC           glCompileShader           = nullptr;
    PFNGLATTACHSHADERPROC            glAttachShader            = nullptr;
    PFNGLDETACHSHADERPROC            glDetachShader            = nullptr;
    PFNGLCREATEPROGRAMPROC           glCreateProgram           = nullptr;
    PFNGLLINKPROGRAMPROC             glLinkProgram             = nullptr;
    PFNGLDELETEPROGRAMPROC           glDeleteProgram           = nullptr;
    PFNGLGETSHADERIVPROC             glGetShaderiv             = nullptr;
    PFNGLGETPROGRAMIVPROC            glGetProgramiv            = nullptr;
    PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog        = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog       = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation      = nullptr;
    PFNGLUNIFORM1FPROC               glUniform1f               = nullptr;
    PFNGLUNIFORM3FPROC               glUniform3f               = nullptr;
    PFNGLUNIFORM1IPROC               glUniform1i               = nullptr;
    PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv        = nullptr;
    PFNGLUSEPROGRAMPROC              glUseProgram              = nullptr;
    PFNGLGETERRORPROC                glGetError                = nullptr;
    PFNGLGETSTRINGPROC               glGetString                = nullptr;
};

// -----------------------------------------------------------------------------
// Single global instance — initialized by GL.cpp
// -----------------------------------------------------------------------------
extern GL33 gGL;

// =============================================================================
// Convenience inline wrappers — use these instead of GL33::glXxx directly
// =============================================================================
inline GLuint   glCreateShader(GLenum type)                        { return gGL.glCreateShader       ? gGL.glCreateShader(type)              : 0; }
inline void     glDeleteShader(GLuint s)                            { if (gGL.glDeleteShader)           gGL.glDeleteShader(s); }
inline void     glShaderSource(GLuint s, GLsizei c, const GLchar* const* src, const GLint* len)
                                                                          { if (gGL.glShaderSource)           gGL.glShaderSource(s,c,src,len); }
inline void     glCompileShader(GLuint s)                            { if (gGL.glCompileShader)          gGL.glCompileShader(s); }
inline void     glAttachShader(GLuint p, GLuint s)                  { if (gGL.glAttachShader)           gGL.glAttachShader(p,s); }
inline void     glDetachShader(GLuint p, GLuint s)                  { if (gGL.glDetachShader)           gGL.glDetachShader(p,s); }
inline GLuint   glCreateProgram()                                   { return gGL.glCreateProgram        ? gGL.glCreateProgram()               : 0; }
inline void     glLinkProgram(GLuint p)                             { if (gGL.glLinkProgram)            gGL.glLinkProgram(p); }
inline void     glDeleteProgram(GLuint p)                           { if (gGL.glDeleteProgram)          gGL.glDeleteProgram(p); }
inline void     glGetShaderiv(GLuint s, GLenum p, GLint* v)         { if (gGL.glGetShaderiv)            gGL.glGetShaderiv(s,p,v); }
inline void     glGetProgramiv(GLuint p, GLenum e, GLint* v)        { if (gGL.glGetProgramiv)           gGL.glGetProgramiv(p,e,v); }
inline void     glGetShaderInfoLog(GLuint s, GLsizei b, GLsizei* l, GLchar* il)
                                                                          { if (gGL.glGetShaderInfoLog)       gGL.glGetShaderInfoLog(s,b,l,il); }
inline void     glGetProgramInfoLog(GLuint p, GLsizei b, GLsizei* l, GLchar* il)
                                                                          { if (gGL.glGetProgramInfoLog)      gGL.glGetProgramInfoLog(p,b,l,il); }
inline GLint    glGetUniformLocation(GLuint p, const GLchar* n)     { return gGL.glGetUniformLocation   ? gGL.glGetUniformLocation(p,n)       : -1; }
inline void     glUniform1f(GLint l, GLfloat v)                     { if (gGL.glUniform1f)              gGL.glUniform1f(l,v); }
inline void     glUniform3f(GLint l, GLfloat x, GLfloat y, GLfloat z)
                                                                          { if (gGL.glUniform3f)              gGL.glUniform3f(l,x,y,z); }
inline void     glUniform1i(GLint l, GLint v)                       { if (gGL.glUniform1i)             gGL.glUniform1i(l,v); }
inline void     glUniformMatrix4fv(GLint l, GLsizei c, GLboolean t, const GLfloat* v)
                                                                          { if (gGL.glUniformMatrix4fv)       gGL.glUniformMatrix4fv(l,c,t,v); }
inline void     glUseProgram(GLuint p)                               { if (gGL.glUseProgram)             gGL.glUseProgram(p); }
inline GLenum   glGetError()                                         { return gGL.glGetError             ? gGL.glGetError()                    : GL_NO_ERROR; }
inline const GLubyte* glGetString(GLenum n)                         { return gGL.glGetString            ? gGL.glGetString(n)                   : nullptr; }
inline void     glGenVertexArrays(GLsizei n, GLuint* a)              { if (gGL.glGenVertexArrays)        gGL.glGenVertexArrays(n,a); }
inline void     glBindVertexArray(GLuint a)                          { if (gGL.glBindVertexArray)        gGL.glBindVertexArray(a); }
inline void     glDeleteVertexArrays(GLsizei n, const GLuint* a)    { if (gGL.glDeleteVertexArrays)     gGL.glDeleteVertexArrays(n,a); }
inline void     glGenBuffers(GLsizei n, GLuint* b)                   { if (gGL.glGenBuffers)             gGL.glGenBuffers(n,b); }
inline void     glBindBuffer(GLenum t, GLuint b)                     { if (gGL.glBindBuffer)             gGL.glBindBuffer(t,b); }
inline void     glDeleteBuffers(GLsizei n, const GLuint* b)          { if (gGL.glDeleteBuffers)          gGL.glDeleteBuffers(n,b); }
inline void     glBufferData(GLenum t, GLsizeiptr s, const void* d, GLenum u)
                                                                          { if (gGL.glBufferData)             gGL.glBufferData(t,s,d,u); }
inline void     glEnableVertexAttribArray(GLuint i)                  { if (gGL.glEnableVertexAttribArray) gGL.glEnableVertexAttribArray(i); }
inline void     glDisableVertexAttribArray(GLuint i)                 { if (gGL.glDisableVertexAttribArray) gGL.glDisableVertexAttribArray(i); }
inline void     glVertexAttribPointer(GLuint i, GLint s, GLenum t, GLboolean n, GLsizei str, const void* p)
                                                                          { if (gGL.glVertexAttribPointer)    gGL.glVertexAttribPointer(i,s,t,n,str,p); }
inline void     glDrawArrays(GLenum m, GLint f, GLsizei c)          { ::glDrawArrays(m,f,c); }
inline void     glDrawElements(GLenum m, GLsizei c, GLenum t, const void* i) { ::glDrawElements(m,c,t,i); }