#include <iostream>
#include <stdexcept>
#include <string>

#include "OpenGL.hpp"
#include "ShaderProgram.hpp"
#include "Common.hpp"

const int WIDTH  = 1280;
const int HEIGHT = 720;

// If defined 1, draws the VSM-shadowmap-texture to screen
#define SHOW 0

glm::vec3 lightPos(-2, 2.0, -2); // In world coordinates
glm::vec3 cubePos(0.0, 0, -5.0);
glm::vec3 planePos(1, -1, -6);
glm::vec3 cameraPos(0,4,0);
glm::vec3 planeScale(7,1,7); // It's a scaled cube

ShaderProgram program, shadowProgram, blurProgram;
GLuint cubeVao, shadowMapTex, shadowMapFBO, shadowMapTexDepth, quadVao;
GLuint blurFBO, blurredTex;

//GLuint SHADOWMAP_SIZE = 256;
GLuint SHADOWMAP_SIZE = 512;
//GLuint SHADOWMAP_SIZE = 1024;
//GLuint SHADOWMAP_SIZE = 2048;

// Amount of blurring
const float BLUR_SCALE = 2.0;

// Type of mapping
#define TEX_LINEAR // TEX_LINEAR or TEX_NEAREST

void set_shadow_matrix_uniform(ShaderProgram &program)
{
	glm::mat4 mat;
	mat *= glm::perspective(45.0f, 1.0f, 2.0f, 100.0f);
	mat *= glm::lookAt(lightPos, cubePos, glm::vec3(0,1,0)); // Point toward object regardless of position
	program.UpdateUniform("cameraToShadowProjector", mat);
}

void draw_cubes(ShaderProgram &program, bool shadowpass)
{
	glBindVertexArray(cubeVao);

	if(!shadowpass) {
		// Draw shadowmap-texture on cube
		program.UpdateUniform("doTexture", 1.0f);
	}

	// Draw cube
	program.UpdateUniform("model", glm::translate(glm::mat4(), cubePos));
	glDrawArrays(GL_TRIANGLES, 0, 36);

	if(!shadowpass) {
		// Don't texture anything else with the shadowmap
		program.UpdateUniform("doTexture", 0.0f);
	}

	// Draw plane
	glm::mat4 model = glm::translate(glm::mat4(), planePos);
	model           = glm::scale(model, planeScale);
	program.UpdateUniform("model", model);
	glDrawArrays(GL_TRIANGLES, 0, 36);

	// Light-box
	if(!shadowpass) { // Don't want it covering the light (casting shadows everywhere)
		model = glm::translate(glm::mat4(), lightPos);
		model = glm::scale(model, glm::vec3(0.1, 0.1, 0.1));
		program.UpdateUniform("model", model);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	glBindVertexArray(0);
}

void draw_normal_pass()
{
	program.UseProgram();

	glViewport(0, 0, WIDTH,HEIGHT);
	glCullFace(GL_BACK);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Upload uniforms
	glm::mat4 proj = glm::perspective((float) 45, (float) WIDTH / (float) HEIGHT, 0.1f, 100.0f);
	glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0, 0, -5), glm::vec3(0,1,0));
	program.UpdateUniform("view", view);
	program.UpdateUniform("proj", proj);
	program.UpdateUniform("lightPos", lightPos);

	set_shadow_matrix_uniform(program);
	glBindTexture(GL_TEXTURE_2D, shadowMapTex);
	draw_cubes(program, false /*not shadowpass*/);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void draw_fullscreen_quad()
{
	glBindVertexArray(quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void blur_shadowmap()
{
	glDisable(GL_DEPTH_TEST);
	blurProgram.UseProgram();

	// Blur shadowMapTex (horizontally) to blurredTex
	glBindFramebuffer(GL_FRAMEBUFFER, blurFBO);
	glBindTexture(GL_TEXTURE_2D, shadowMapTex); //Input-texture
	blurProgram.UpdateUniform("ScaleU", glm::vec2(1.0/SHADOWMAP_SIZE*BLUR_SCALE, 0));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	draw_fullscreen_quad();

	// Mipmaps
	glBindTexture(GL_TEXTURE_2D, shadowMapTex);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Blur blurredTex vertically and write to shadowMapTex
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glBindTexture(GL_TEXTURE_2D, blurredTex);
	blurProgram.UpdateUniform("ScaleU", glm::vec2(0, 1.0/SHADOWMAP_SIZE*BLUR_SCALE));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	draw_fullscreen_quad();

	// Mipmaps
	glBindTexture(GL_TEXTURE_2D, blurredTex);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}

void blur_map()
{
	blurProgram.UseProgram();
	glViewport(0, 0, SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	// Blur shadowMapTex (write to itself)
	blur_shadowmap();
}

void draw_shadow_pass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glViewport(0, 0, SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	shadowProgram.UseProgram();
	set_shadow_matrix_uniform(shadowProgram);
	draw_cubes(shadowProgram, true /*shadowpass*/);

	glBindTexture(GL_TEXTURE_2D, shadowMapTex);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Reset
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

int main()
{
	// Set up OpenGL-context
	if(!init_opengl( WIDTH, HEIGHT )){
		std::cout << "Error setting up OpenGL-context.\n";
		return -1;  
	}

	// Create programs
	program.LoadFromFile("vsm/vertexShader.glsl", "vsm/fragmentShader.glsl");
	shadowProgram.LoadFromFile("vsm/shadowVertexShader.glsl", "vsm/shadowFragmentShader.glsl");
	blurProgram.LoadFromFile("blurVertexShader.glsl", "blurFragmentShader.glsl");
	
	// Create VAO
	cubeVao       = create_cube_vao();
	quadVao       = create_quad_vao();

	// ShadowMap-texture (depth)
	shadowMapTexDepth = texture::GenerateDepthMipmanClampBorder1(SHADOWMAP_SIZE);

#define TYPE GL_RG32F
#define TYPE2 GL_RG

	shadowMapTex = texture::GenerateMipmapClampEdge(TYPE, SHADOWMAP_SIZE, SHADOWMAP_SIZE, TYPE2, GL_FLOAT);
	shadowMapFBO = texture::Framebuffer(shadowMapTex, shadowMapTexDepth);

	GLuint tmp = texture::GenerateDepthMipmanClampBorder1(SHADOWMAP_SIZE);
	blurredTex = texture::GenerateMipmapClampEdge(TYPE, SHADOWMAP_SIZE, SHADOWMAP_SIZE, TYPE2, GL_FLOAT);
	blurFBO = texture::Framebuffer(blurredTex, tmp);

	// Set up GL-state
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);

	glBindTexture(GL_TEXTURE_2D, shadowMapTex);

	while (glfwGetWindowParam(GLFW_OPENED))
	{
		draw_shadow_pass();
		blur_map();
		draw_normal_pass();

#if SHOW
		// Blur and draw to screen
		blurProgram.UseProgram(); // Since it draws fullscreen quad ...
		blurProgram.UpdateUniform("ScaleU", glm::vec2(0,0)); // ... but make sure we don't blur
		glBindTexture(GL_TEXTURE_2D, shadowMapTex);
		draw_fullscreen_quad();
		glBindTexture(GL_TEXTURE_2D, 0);
#endif

		glfwSwapBuffers();

		if (glfwGetKey( GLFW_KEY_ESC ) == GLFW_PRESS)
			break;
	}

	glfwTerminate();

	// No cleanup :(

	return 0;
}