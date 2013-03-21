#ifndef COMMON_HPP
#define COMMON_HPP

#include "OpenGL.hpp"
#include <string>

namespace texture
{
	GLuint GenerateMipmapClampEdge(GLint internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type);
	GLuint GenerateDepthMipmanClampBorder1(GLsizei size);
	GLuint GenerateDepthCube(GLsizei size);
	GLuint GenerateCube(GLsizei size);
	void   FramebufferCube(GLuint *cubeFBOs, GLuint cubeTex, GLuint cubeDepthTex);
	GLuint Framebuffer(int colorTex, int depthTex);
};

// OpenGL-error callback function 
// Used when GL_ARB_debug_output is supported
void APIENTRY DebugFunc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
	const GLchar* message, GLvoid* userParam);

int create_cube_vao();
int create_quad_vao();

bool init_opengl( int width, int height );

#endif