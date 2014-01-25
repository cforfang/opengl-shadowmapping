#include <iostream>
#include <stdexcept>
#include <string>

#include "Common.hpp"
#include "ShaderProgram.hpp"

static const int WIDTH = 1280;
static const int HEIGHT = 720;

#define BLUR_VSM 1

// Size of shadowmap
//GLuint SHADOWMAP_SIZE = 128;
//GLuint SHADOWMAP_SIZE = 256;
GLuint SHADOWMAP_SIZE = 512;
//GLuint SHADOWMAP_SIZE = 1024;
//GLuint SHADOWMAP_SIZE = 2048;

// VSM texture-types
#define TYPE GL_RG32F
#define TYPE2 GL_RG

// Geomtry
static glm::vec3 lightPos(-0.0, 2, -7); // In world coordinates
static glm::vec3 cubePos(-0.0, 0, -5.0);
static glm::vec3 cubePos2(+3.0, -0.5, -5.0);
static glm::vec3 cubePos3(0.0, 0, -8.0);
static glm::vec3 groundPos(1, -1, -6);
static glm::vec3 cameraPos(0, 4, 0);
static glm::vec3 groundScale(17, 1, 17); // It's a scaled cube

// Resources
static ShaderProgram normalProgram, shadowProgram, blurProgram;
static Mesh cubeMesh, quadMesh;
static GLuint blurFBO, blurTex;
static GLuint cubeTex, cubeDepthTex, cubeFBOs[6];
static GLuint currentSideTex, currentSideDepthTex;
static GLuint toCurrentSideFBO;

static GLuint GenerateDepthCube(GLsizei size)
{
	GLuint cube;
	glGenTextures(1, &cube);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_DEPTH_COMPONENT, size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_DEPTH_COMPONENT, size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_DEPTH_COMPONENT, size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_DEPTH_COMPONENT, size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_DEPTH_COMPONENT, size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_DEPTH_COMPONENT, size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	return cube;
}

static GLuint GenerateCube(GLsizei size)
{
	GLuint cube;
	glGenTextures(1, &cube);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB32F, size, size, 0, GL_RGB, GL_FLOAT, 0);
	return cube;
}

static void FramebufferCube(GLuint *cubeFBOs, GLuint cubeTex, GLuint cubeDepthTex)
{
	glGenFramebuffers(6, cubeFBOs);
	for (int i = 0; i < 6; i++) {
		glBindFramebuffer(GL_FRAMEBUFFER, cubeFBOs[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubeTex, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubeDepthTex, 0);
		GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (GL_FRAMEBUFFER_COMPLETE != result) {
			printf("ERROR: Framebuffer is not complete.\n");
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void set_shadow_matrix_uniform(ShaderProgram &program, int dir)
{
	glm::mat4 mat, view, tmp;
	mat *= glm::perspective(90.0f, 1.0f, 0.5f, 100.0f);
	switch (dir) {
	case 0:
		// +X
		tmp = glm::lookAt(lightPos, lightPos + glm::vec3(+1, +0, 0), glm::vec3(0, -1, 0));
		view = tmp;
		mat *= tmp;
		break;
	case 1:
		// -X
		tmp = glm::lookAt(lightPos, lightPos + glm::vec3(-1, +0, 0), glm::vec3(0, -1, 0));
		view = tmp;
		mat *= tmp;
		break;
	case 2:
		// +Y
		tmp = glm::lookAt(lightPos, lightPos + glm::vec3(0, +1, 0), glm::vec3(0, 0, -1));
		view = tmp;
		mat *= tmp;
		break;
	case 3:
		// -Y
		tmp = glm::lookAt(lightPos, lightPos + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
		view = tmp;
		mat *= tmp;
		break;
	case 4:
		// +Z
		tmp = glm::lookAt(lightPos, lightPos + glm::vec3(0, 0, +1), glm::vec3(0, -1, 0));
		view = tmp;
		mat *= tmp;
		break;
	case 5:
		// -Z
		// Works
		tmp = glm::lookAt(lightPos, lightPos + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0));
		view = tmp;
		mat *= tmp;
		break;
	default:
		// Do nothing
		break;
	}
	program.UpdateUniform("cameraToShadowView", view);
	program.UpdateUniform("cameraToShadowProjector", mat);
}

static void draw_cubes(ShaderProgram &normalProgram, bool shadowpass)
{
	glBindVertexArray(cubeMesh.vao);

	// Draw cube2
	normalProgram.UpdateUniform("model", glm::translate(glm::mat4(), cubePos));
	glDrawArrays(GL_TRIANGLES, 0, 36);
	normalProgram.UpdateUniform("model", glm::translate(glm::mat4(), cubePos2));
	glDrawArrays(GL_TRIANGLES, 0, 36);
	normalProgram.UpdateUniform("model", glm::translate(glm::mat4(), cubePos3));
	glDrawArrays(GL_TRIANGLES, 0, 36);

	// Draw ground
	glm::mat4 model = glm::translate(glm::mat4(), groundPos);
	model = glm::scale(model, groundScale);
	normalProgram.UpdateUniform("model", model);
	glDrawArrays(GL_TRIANGLES, 0, 36);

	// Light-box
	if (!shadowpass) { // Don't want it covering the light (casting shadows everywhere)
		model = glm::translate(glm::mat4(), lightPos);
		model = glm::scale(model, glm::vec3(0.1, 0.1, 0.1));
		normalProgram.UpdateUniform("model", model);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	glBindVertexArray(0);
}

static void draw_normal_pass()
{
	normalProgram.UseProgram();

	glViewport(0, 0, WIDTH, HEIGHT);
	glCullFace(GL_BACK);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Upload uniforms
	glm::mat4 proj = glm::perspective(45.0f, (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
	glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0, 0, -5), glm::vec3(0, 1, 0));
	normalProgram.UpdateUniform("view", view);
	normalProgram.UpdateUniform("proj", proj);
	normalProgram.UpdateUniform("lightPos", lightPos);

	set_shadow_matrix_uniform(normalProgram, -1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);
	draw_cubes(normalProgram, false /*not shadowpass*/);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

static void draw_fullscreen_quad()
{
	glBindVertexArray(quadMesh.vao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

static void draw_shadow_pass()
{
	glViewport(0, 0, SHADOWMAP_SIZE, SHADOWMAP_SIZE);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	shadowProgram.UseProgram();

	// For each side of cubemap
	for (int i = 0; i < 6; ++i) {
#if BLUR_VSM
		// Draw to temp. storage
		shadowProgram.UseProgram();
		glBindFramebuffer(GL_FRAMEBUFFER, toCurrentSideFBO);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		set_shadow_matrix_uniform(shadowProgram, i);
		draw_cubes(shadowProgram, true /* is shadowpass */);

		// Blur horizontally to blurTex
		glDisable(GL_DEPTH_TEST);

		blurProgram.UseProgram();
		blurProgram.UpdateUniform("ScaleU", glm::vec2(1.0 / SHADOWMAP_SIZE, 0));

		glBindFramebuffer(GL_FRAMEBUFFER, blurFBO);
		glBindTexture(GL_TEXTURE_2D, currentSideTex);
		glClear(GL_COLOR_BUFFER_BIT);
		draw_fullscreen_quad();

		// Blur vertically to actual cubemap
		glBindFramebuffer(GL_FRAMEBUFFER, cubeFBOs[i]);

		glBindTexture(GL_TEXTURE_2D, blurTex);
		blurProgram.UpdateUniform("ScaleU", glm::vec2(0, 1.0 / SHADOWMAP_SIZE));

		draw_fullscreen_quad();

		glEnable(GL_DEPTH_TEST);
#else
		// Draw directly to cubemap
		glBindFramebuffer(GL_FRAMEBUFFER, cubeFBOs[i]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		set_shadow_matrix_uniform(shadowProgram, i);
		draw_cubes(shadowProgram, true /* is shadowpass */);
#endif
	}

	// Reset state
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
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
	if (!normalProgram.Load(ShaderInfo::VSFS("vsmcube/vertexShader.glsl", "vsmcube/fragmentShader.glsl")))
		return false;
	if (!shadowProgram.Load(ShaderInfo::VSFS("vsmcube/shadowVertexShader.glsl", "vsmcube/shadowFragmentShader.glsl")))
		return false;
	if (!blurProgram.Load(ShaderInfo::VSFS("blurVertexShader.glsl", "blurFragmentShader.glsl")))
		return false;

	// Create geometry
	cubeMesh = create_cube();
	quadMesh = create_quad();

	// Create cubemap
	cubeTex      = GenerateCube(SHADOWMAP_SIZE);
	cubeDepthTex = GenerateDepthCube(SHADOWMAP_SIZE);
	FramebufferCube(cubeFBOs, cubeTex, cubeDepthTex);

	// Textures and FBO to perform blurring
	blurTex = texture::Create2D(GL_RG32F, SHADOWMAP_SIZE, SHADOWMAP_SIZE, GL_RG, GL_FLOAT);
	texture::SetWrapMode2D(blurTex, texture::WrapMode::ClampEdge);
	blurFBO = texture::Framebuffer(blurTex, -1);

	// Temporary storage
	currentSideTex = texture::Create2D(TYPE, SHADOWMAP_SIZE, SHADOWMAP_SIZE, TYPE2, GL_FLOAT);
	texture::SetWrapMode2D(currentSideTex, texture::WrapMode::ClampEdge);
	currentSideDepthTex = texture::Create2D(GL_DEPTH_COMPONENT, SHADOWMAP_SIZE, SHADOWMAP_SIZE, GL_DEPTH_COMPONENT, GL_FLOAT);
	toCurrentSideFBO = texture::Framebuffer(currentSideTex, currentSideDepthTex);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);

	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	while (!glfwWindowShouldClose(window))
	{
		glm::mat4 mat = glm::translate(glm::mat4(1.0f), glm::vec3(0, 2, -5));
		mat          *= glm::rotate(glm::mat4(), (float) glfwGetTime() * 50.0f, glm::vec3(0, 1, 0));
		lightPos = glm::vec3(mat * glm::vec4(glm::vec3(2, 0, 0), 1.0));

		draw_shadow_pass();
		draw_normal_pass();

		glfwSwapBuffers(window);
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			break;
	}

	normalProgram.DeleteProgram();
	shadowProgram.DeleteProgram();
	blurProgram.DeleteProgram();

	delete_mesh(quadMesh);
	delete_mesh(cubeMesh);

	glDeleteTextures(1, &blurTex);
	glDeleteFramebuffers(1, &blurFBO);

	glDeleteTextures(1, &cubeDepthTex);
	glDeleteTextures(1, &cubeTex);
	glDeleteFramebuffers(6, cubeFBOs);

	glDeleteTextures(1, &currentSideTex);
	glDeleteTextures(1, &currentSideDepthTex);
	glDeleteFramebuffers(1, &toCurrentSideFBO);

	glfwTerminate();

	return 0;
}