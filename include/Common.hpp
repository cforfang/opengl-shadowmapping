#ifndef COMMON_HPP
#define COMMON_HPP

#include "OpenGL.hpp"
#include <string>

namespace texture
{
	GLuint Create2D(GLint internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type);

	enum Filtering { NEAREST, LINEAR, MIPMAP };
	void SetFiltering2D(GLuint texture, Filtering filtering);

	enum WrapMode { ClampEdge, ClampBorder, Repeat };
	void SetWrapMode2D(GLuint texture, WrapMode mode, GLfloat border = 0.0f);

	GLuint Framebuffer(int colorTex, int depthTex);
};

// OpenGL-error callback function 
// Used when GL_ARB_debug_output is supported
void APIENTRY DebugFunc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	const GLchar* message, GLvoid* userParam);

struct Mesh
{
	GLuint vao;
	GLuint vbo;
};

Mesh create_quad();
Mesh create_cube();
void delete_mesh(Mesh m);

bool init_opengl( int width, int height, GLFWwindow** ppWindow );

#endif