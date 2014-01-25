#include <iostream>
#include <stdexcept>
#include <string>

#include "OpenGL.hpp"
#include "ShaderProgram.hpp"
#include "Common.hpp"

// Window size
static const int WIDTH = 1280;
static const int HEIGHT = 720;

// Geometry (world coordinates)
static glm::vec3 lightPos(-2, 2.0, -2);
static glm::vec3 cubePos(0.0, 0, -5.0);
static glm::vec3 groundPos(1, -1, -6);
static glm::vec3 groundScale(7, 1, 7); // It's just a scaled cube
static glm::vec3 cameraPos(0, 4, 0);

// Resources
static ShaderProgram program, shadowProgram, blurProgram;
static Mesh cubeMesh, quadMesh;
static GLuint shadowMapFBO, shadowMapTex, shadowMapTexDepth;
static GLuint blurFBO, blurTex;

// Shadow-map resolution
//static GLuint SHADOWMAP_SIZE = 256;
static GLuint SHADOWMAP_SIZE = 512;
//static GLuint SHADOWMAP_SIZE = 1024;
//static GLuint SHADOWMAP_SIZE = 2048;

// Amount of blurring
static const float BLUR_SCALE = 2.0;

// If defined 1, draws the VSM-shadowmap-texture to screen
#define DISPLAY_VSM_TEXTURE 0

static void set_shadow_matrix_uniform(ShaderProgram &program)
{
	glm::mat4 mat;
	mat *= glm::perspective(45.0f, 1.0f, 2.0f, 100.0f);
	mat *= glm::lookAt(lightPos, cubePos, glm::vec3(0, 1, 0)); // Point toward object regardless of position
	program.UpdateUniform("cameraToShadowProjector", mat);
}

static void draw_cubes(ShaderProgram &program, bool shadowpass)
{
	glBindVertexArray(cubeMesh.vao);

	if (!shadowpass) {
		// Draw shadowmap-texture on cube
		program.UpdateUniform("doTexture", 1.0f);
	}

	// Draw cube
	program.UpdateUniform("model", glm::translate(glm::mat4(), cubePos));
	glDrawArrays(GL_TRIANGLES, 0, 36);

	if (!shadowpass) {
		// Don't texture anything else with the shadowmap
		program.UpdateUniform("doTexture", 0.0f);
	}

	// Draw ground
	glm::mat4 model = glm::translate(glm::mat4(), groundPos);
	model = glm::scale(model, groundScale);
	program.UpdateUniform("model", model);
	glDrawArrays(GL_TRIANGLES, 0, 36);

	// Light-box
	if (!shadowpass) { // Don't want it covering the light (casting shadows everywhere)
		model = glm::translate(glm::mat4(), lightPos);
		model = glm::scale(model, glm::vec3(0.1, 0.1, 0.1));
		program.UpdateUniform("model", model);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	glBindVertexArray(0);
}

static void normal_pass()
{
	program.UseProgram();

	glViewport(0, 0, WIDTH, HEIGHT);
	glCullFace(GL_BACK);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Upload uniforms
	glm::mat4 proj = glm::perspective((float)45, (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
	glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0, 0, -5), glm::vec3(0, 1, 0));
	program.UpdateUniform("view", view);
	program.UpdateUniform("proj", proj);
	program.UpdateUniform("lightPos", lightPos);

	set_shadow_matrix_uniform(program);
	glBindTexture(GL_TEXTURE_2D, shadowMapTex);
	draw_cubes(program, false /*not shadowpass*/);
	glBindTexture(GL_TEXTURE_2D, 0);
}

static void draw_fullscreen_quad()
{
	glBindVertexArray(quadMesh.vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

static void blur_shadowmap()
{
	glDisable(GL_DEPTH_TEST);
	blurProgram.UseProgram();

	// Blur shadowMapTex (horizontally) to blurTex
	glBindFramebuffer(GL_FRAMEBUFFER, blurFBO);
	glBindTexture(GL_TEXTURE_2D, shadowMapTex); //Input-texture
	blurProgram.UpdateUniform("ScaleU", glm::vec2(1.0 / SHADOWMAP_SIZE * BLUR_SCALE, 0));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	draw_fullscreen_quad();

	// Blur blurTex vertically and write to shadowMapTex
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glBindTexture(GL_TEXTURE_2D, blurTex);
	blurProgram.UpdateUniform("ScaleU", glm::vec2(0, 1.0 / SHADOWMAP_SIZE*BLUR_SCALE));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	draw_fullscreen_quad();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}

static void blur_map()
{
	blurProgram.UseProgram();
	glViewport(0, 0, SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	// Blur shadowMapTex
	blur_shadowmap();
}

static void shadow_pass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glViewport(0, 0, SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	shadowProgram.UseProgram();
	set_shadow_matrix_uniform(shadowProgram);
	draw_cubes(shadowProgram, true /*shadowpass*/);

	// Reset
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	blur_map();
}

int main()
{
	GLFWwindow* window = nullptr;

	// Set up OpenGL-context
	if (!init_opengl(WIDTH, HEIGHT, &window)){
		std::cout << "Error setting up OpenGL-context.\n";
		return -1;
	}

	// Create programs
	if (!program.Load(ShaderInfo::VSFS("vsm/vertexShader.glsl", "vsm/fragmentShader.glsl")))
		return false;
	if (!shadowProgram.Load(ShaderInfo::VSFS("vsm/shadowVertexShader.glsl", "vsm/shadowFragmentShader.glsl")))
		return false;
	if (!blurProgram.Load(ShaderInfo::VSFS("blurVertexShader.glsl", "blurFragmentShader.glsl")))
		return false;

	// Create geometry
	cubeMesh = create_cube();
	quadMesh = create_quad();

	// ShadowMap-textures and FBO
	shadowMapTexDepth = texture::Create2D(GL_DEPTH_COMPONENT, SHADOWMAP_SIZE, SHADOWMAP_SIZE, GL_DEPTH_COMPONENT, GL_FLOAT);
	shadowMapTex = texture::Create2D(GL_RG32F, SHADOWMAP_SIZE, SHADOWMAP_SIZE, GL_RG, GL_FLOAT);
	shadowMapFBO = texture::Framebuffer(shadowMapTex, shadowMapTexDepth);

	// Textures and FBO to perform blurring
	blurTex = texture::Create2D(GL_RG32F, SHADOWMAP_SIZE, SHADOWMAP_SIZE, GL_RG, GL_FLOAT);
	blurFBO = texture::Framebuffer(blurTex, -1);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);

	while (!glfwWindowShouldClose(window))
	{
		shadow_pass();
		normal_pass();

#if DISPLAY_VSM_TEXTURE
		// Blur and draw to screen
		blurProgram.UseProgram(); // Since it draws fullscreen quad ...
		blurProgram.UpdateUniform("ScaleU", glm::vec2(0, 0)); // ... but make sure we don't actually blur
		glBindTexture(GL_TEXTURE_2D, shadowMapTex);
		draw_fullscreen_quad();
		glBindTexture(GL_TEXTURE_2D, 0);
#endif

		glfwSwapBuffers(window);
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			break;
	}

	program.DeleteProgram();
	shadowProgram.DeleteProgram();
	blurProgram.DeleteProgram();

	glDeleteTextures(1, &blurTex);
	glDeleteFramebuffers(1, &blurFBO);

	glDeleteTextures(1, &shadowMapTex);
	glDeleteTextures(1, &shadowMapTexDepth);
	glDeleteFramebuffers(1, &shadowMapFBO);

	delete_mesh(quadMesh);
	delete_mesh(cubeMesh);

	glfwTerminate();

	return 0;
}