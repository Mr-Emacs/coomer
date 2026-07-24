#ifndef BM_GL_H
#define BM_GL_H

#include <GL/gl.h>
#include <GL/glx.h>

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif

#define BM_GL_FUNCTIONS                                                                                          \
    BM_GL_FUNC(GLuint, glCreateShader, (GLenum type))                                                            \
    BM_GL_FUNC(void, glShaderSource, (GLuint shader, GLsizei count, const char *const *string, const GLint *length)) \
    BM_GL_FUNC(void, glCompileShader, (GLuint shader))                                                           \
    BM_GL_FUNC(void, glGetShaderiv, (GLuint shader, GLenum pname, GLint *params))                                \
    BM_GL_FUNC(void, glGetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei *length, char *infoLog))       \
    BM_GL_FUNC(void, glDeleteShader, (GLuint shader))                                                            \
    BM_GL_FUNC(GLuint, glCreateProgram, (void))                                                                  \
    BM_GL_FUNC(void, glAttachShader, (GLuint program, GLuint shader))                                            \
    BM_GL_FUNC(void, glBindAttribLocation, (GLuint program, GLuint index, const char *name))                     \
    BM_GL_FUNC(void, glLinkProgram, (GLuint program))                                                            \
    BM_GL_FUNC(void, glGetProgramiv, (GLuint program, GLenum pname, GLint *params))                              \
    BM_GL_FUNC(void, glGetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei *length, char *infoLog))     \
    BM_GL_FUNC(void, glDeleteProgram, (GLuint program))                                                          \
    BM_GL_FUNC(void, glUseProgram, (GLuint program))                                                             \
    BM_GL_FUNC(void, glGenVertexArrays, (GLsizei n, GLuint *arrays))                                             \
    BM_GL_FUNC(void, glBindVertexArray, (GLuint array))                                                          \
    BM_GL_FUNC(void, glGenBuffers, (GLsizei n, GLuint *buffers))                                                 \
    BM_GL_FUNC(void, glBindBuffer, (GLenum target, GLuint buffer))                                               \
    BM_GL_FUNC(void, glBufferData, (GLenum target, long size, const void *data, GLenum usage))                   \
    BM_GL_FUNC(void, glEnableVertexAttribArray, (GLuint index))                                                  \
    BM_GL_FUNC(void, glVertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)) \
    BM_GL_FUNC(GLint, glGetUniformLocation, (GLuint program, const char *name))                                  \
    BM_GL_FUNC(void, glUniform1f, (GLint location, GLfloat v0))                                                  \
    BM_GL_FUNC(void, glUniform1i, (GLint location, GLint v0))                                                    \
    BM_GL_FUNC(void, glUniform2f, (GLint location, GLfloat v0, GLfloat v1))

#define BM_GL_FUNC(ret, name, args) typedef ret (*BM_PFN_##name) args;
BM_GL_FUNCTIONS
#undef BM_GL_FUNC

#define BM_GL_FUNC(ret, name, args) static BM_PFN_##name name;
BM_GL_FUNCTIONS
#undef BM_GL_FUNC

static bool bm_gl_load(void)
{
#define BM_GL_FUNC(ret, name, args) name = (BM_PFN_##name)glXGetProcAddress((const GLubyte *)#name);
    BM_GL_FUNCTIONS
#undef BM_GL_FUNC

    bool ok = true;
#define BM_GL_FUNC(ret, name, args) ok = ok && (name != NULL);
    BM_GL_FUNCTIONS
#undef BM_GL_FUNC

    return ok;
}

#endif //BM_GL_H
