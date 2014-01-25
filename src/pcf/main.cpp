#include <iostream>
#include <stdexcept>

#include "Common.hpp"
#include "OpenGL.hpp"
#include "ShaderProgram.hpp"

#define SHADOWMAP_SIZE 512

// Window-size
static const int WIDTH  = 1280;
static const int HEIGHT = 720;

// Object positions (world coordinates)
static glm::vec3 lightPos(-2.0, 2.0, -2);
static glm::vec3 cubePos(0.0, 0.0, -5.0);
static glm::vec3 planePos(1,-1,-6);
static glm::vec3 planeScale(7,1,7); // It's a scaled cube

// Resources
static ShaderProgram program, shadowProgram;
static Mesh cubeMesh, quadMesh;
static GLuint shadowMapFBO, shadowMapTex, shadowMapTexDepth;

static char* samplingTypeText[] = {"Manual", "Free HW PCF", "Manual 4x PCF", "Manual ?x PCF (see shader)"};
static GLint samplingType = 0;

static void set_shadow_matrix_uniform(ShaderProgram &prog)
{
	glm::mat4 mat;
	mat *= glm::perspective(60.0f, 1.0f, 1.0f, 10.0f);
	mat *= glm::lookAt(lightPos, cubePos, glm::vec3(0,1,0)); // Point toward object regardless of position
	prog.UpdateUniform("cameraToShadowProjector", mat);
}

static void draw_cubes(ShaderProgram &program, bool shadowpass)
{
	glBindVertexArray(cubeMesh.vao);

	glm::mat4 model;

	if(!shadowpass) {
		// Draw shadowmap-texture on cube
		program.UpdateUniform("doTexture", 1.0f);
	}

	// Draw cube
	model = glm::translate(glm::mat4(), cubePos);
	program.UpdateUniform("model", model);
	glDrawArrays(GL_TRIANGLES, 0, 36);

	if(!shadowpass) {
		// Don't texture anything else with the shadowmap
		program.UpdateUniform("doTexture", 0.0f);
	}

	// Draw plane
	model = glm::translate(glm::mat4(), planePos);
	model = glm::scale(model, planeScale);
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

static void draw_normal_pass()
{
	glBindFramebuffer (GL_FRAMEBUFFER, 0);
	program.UseProgram();

	glViewport(0, 0, WIDTH,HEIGHT);
	glCullFace(GL_BACK);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 proj = glm::perspective((float) 45, (float) WIDTH / (float) HEIGHT, 0.1f, 100.0f);
	glm::mat4 view = glm::lookAt(glm::vec3(0,5,0), glm::vec3(0, 0, -5), glm::vec3(0,1,0));

	// Upload model and view
	program.UpdateUniform("view", view);
	program.UpdateUniform("proj", proj);
	program.UpdateUniform("lightPos", lightPos);
	program.UpdateUniformi("samplingType", samplingType);

	set_shadow_matrix_uniform(program);
	draw_cubes(program, false /*not shadowpass*/);
}

static void draw_shadow_pass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);

	glCullFace(GL_FRONT);
	glViewport(0, 0, SHADOWMAP_SIZE, SHADOWMAP_SIZE);
	glClear(GL_DEPTH_BUFFER_BIT);

	shadowProgram.UseProgram();
	set_shadow_matrix_uniform(shadowProgram);

	draw_cubes(shadowProgram, true /*shadowpass*/);
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
	if (!program.Load(ShaderInfo::VSFS("pcf/vertexShader.glsl", "pcf/fragmentShader.glsl")))
		return false;
	if (!shadowProgram.Load(ShaderInfo::VSFS("pcf/shadowVertexShader.glsl", "pcf/shadowFragmentShader.glsl")))
		return false;

	// Geometry
	cubeMesh = create_cube();

	// ShadowMap-texture
	shadowMapTex = texture::Create2D(GL_DEPTH_COMPONENT, SHADOWMAP_SIZE, SHADOWMAP_SIZE, GL_DEPTH_COMPONENT, GL_FLOAT);
	texture::SetFiltering2D(shadowMapTex, texture::Filtering::LINEAR);
	texture::SetWrapMode2D(shadowMapTex, texture::WrapMode::ClampBorder, 1.0f);

	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	// ShadowMap-FBO
	glGenFramebuffers(1, &shadowMapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMapTex, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	GLenum result = glCheckFramebufferStatus (GL_FRAMEBUFFER);
	if (GL_FRAMEBUFFER_COMPLETE != result) {
		 printf ("ERROR: Framebuffer not complete.\n");
		 return -1;	
	}
	glBindFramebuffer (GL_FRAMEBUFFER, 0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);

	printf("Press space to switch sampling-mode.\n");

	while (!glfwWindowShouldClose(window))
	{
		draw_shadow_pass();
		draw_normal_pass();

		glfwSwapBuffers(window);
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			break;

		// Switches between sampling modes
		static bool lastState = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
		bool thisState = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
		if (lastState != thisState && thisState) {
			samplingType = (samplingType + 1) % 4;
			printf("Using sampling type: %d (%s)\n", samplingType, samplingTypeText[samplingType]);
		}
		lastState = thisState;
	}

	program.DeleteProgram();
	shadowProgram.DeleteProgram();

	delete_mesh(cubeMesh);
	delete_mesh(quadMesh);

	glDeleteFramebuffers(1, &shadowMapFBO);
	glDeleteTextures(1, &shadowMapTex);
	glDeleteTextures(1, &shadowMapTexDepth);

	glfwTerminate();

	return 0;
}