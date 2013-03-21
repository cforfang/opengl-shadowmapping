#include <GL3/gl3w.h>
#include <GL/glfw.h>

#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Common.hpp"
#include "ShaderProgram.hpp"

const int WIDTH  = 1280;
const int HEIGHT = 720;

// Comment out for no blurring of shadows
#define BLUR

// Size of shadowmap-textures(s)
//GLuint SHADOWMAP_SIZE = 128;
//GLuint SHADOWMAP_SIZE = 256;
GLuint SHADOWMAP_SIZE = 512;
//GLuint SHADOWMAP_SIZE = 1024;
//GLuint SHADOWMAP_SIZE = 2048;

// Texture-types
#define TYPE GL_RG32F
#define TYPE2 GL_RG

glm::vec3 lightPos(-0.0, 2, -7); // In world coordinates
glm::vec3 cubePos(-0.0, 0, -5.0);
glm::vec3 cubePos2(+3.0, -0.5, -5.0);
glm::vec3 cubePos3(0.0, 0, -8.0);
glm::vec3 planePos(1, -1, -6);
glm::vec3 cameraPos(0,4,0);
glm::vec3 planeScale(17,1,17); // It's a scaled cube

ShaderProgram normalProgram, shadowProgram, blurProgram;
GLuint cubeVao, quadVao;
GLuint blurFBO, blurredTex;
GLuint cubeTex, cubeDepthTex, cubeFBOs[6];
GLuint currentSideTex, currentSideDepthTex;
GLuint toCurrentSideFBO;

void set_shadow_matrix_uniform(ShaderProgram &program, int dir)
{
	glm::mat4 mat, view, tmp;
	mat *= glm::perspective(90.0f, 1.0f, 0.5f, 100.0f);
	switch(dir) {
	case 0:
		// +X
		tmp = glm::lookAt(lightPos, lightPos+glm::vec3(+1,+0,0), glm::vec3(0,-1,0));
		view = tmp;
		mat *= tmp;
		break;
	case 1:
		// -X
		tmp = glm::lookAt(lightPos, lightPos+glm::vec3(-1,+0,0), glm::vec3(0,-1,0));
		view = tmp;
		mat *= tmp;
		break;
	case 2:
		// +Y
		tmp = glm::lookAt(lightPos, lightPos+glm::vec3(0,+1,0), glm::vec3(0,0,-1));
		view = tmp;
		mat *= tmp;
		break;
	case 3:
		// -Y
		tmp = glm::lookAt(lightPos, lightPos+glm::vec3(0,-1,0), glm::vec3(0,0,-1));
		view = tmp;
		mat *= tmp;
		break;
	case 4:
		// +Z
		tmp = glm::lookAt(lightPos, lightPos+glm::vec3(0,0,+1), glm::vec3(0,-1,0));
		view = tmp;
		mat *= tmp;
		break;
	case 5:
		// -Z
		// Works
		tmp = glm::lookAt(lightPos, lightPos+glm::vec3(0,0,-1), glm::vec3(0,-1,0));
		view = tmp;
		mat *= tmp;
		break;
	default:
		// Not needed
		return;
		break;
	}
	program.UpdateUniform("cameraToShadowView", view);
	program.UpdateUniform("cameraToShadowProjector", mat);
}

void draw_cubes(ShaderProgram &normalProgram, bool shadowpass)
{
	glBindVertexArray(cubeVao);

	// Draw cube2
	normalProgram.UpdateUniform("model", glm::translate(glm::mat4(), cubePos));
	glDrawArrays(GL_TRIANGLES, 0, 36);
	normalProgram.UpdateUniform("model", glm::translate(glm::mat4(), cubePos2));
	glDrawArrays(GL_TRIANGLES, 0, 36);
	normalProgram.UpdateUniform("model", glm::translate(glm::mat4(), cubePos3));
	glDrawArrays(GL_TRIANGLES, 0, 36);

	// Draw plane
	glm::mat4 model = glm::translate(glm::mat4(), planePos);
	model           = glm::scale(model, planeScale);
	normalProgram.UpdateUniform("model", model);
	glDrawArrays(GL_TRIANGLES, 0, 36);

	// Light-box
	if(!shadowpass) { // Don't want it covering the light (casting shadows everywhere)
		model = glm::translate(glm::mat4(), lightPos);
		model = glm::scale(model, glm::vec3(0.1, 0.1, 0.1));
		normalProgram.UpdateUniform("model", model);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	glBindVertexArray(0);
}

void draw_normal_pass()
{
	normalProgram.UseProgram();

	glViewport(0, 0, WIDTH,HEIGHT);
	glCullFace(GL_BACK);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Upload uniforms
	glm::mat4 proj = glm::perspective((float) 45, (float) WIDTH / (float) HEIGHT, 0.1f, 100.0f);
	glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0, 0, -5), glm::vec3(0,1,0));
	normalProgram.UpdateUniform("view", view);
	normalProgram.UpdateUniform("proj", proj);
	normalProgram.UpdateUniform("lightPos", lightPos);

	set_shadow_matrix_uniform(normalProgram, -1);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);
	draw_cubes(normalProgram, false /*not shadowpass*/);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

}

void draw_fullscreen_quad()
{
	glBindVertexArray(quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void draw_shadow_pass()
{
	glViewport(0, 0, SHADOWMAP_SIZE, SHADOWMAP_SIZE);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	shadowProgram.UseProgram();

	for(int i=0; i<6; ++i) {
		
#ifdef BLUR
		shadowProgram.UseProgram();
		glBindFramebuffer(GL_FRAMEBUFFER, toCurrentSideFBO);
#else
		glBindFramebuffer(GL_FRAMEBUFFER, cubeFBOs[i]);
#endif

		//glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		set_shadow_matrix_uniform(shadowProgram,i);
		draw_cubes(shadowProgram, true);

#ifdef BLUR
		// Blur face
		glDisable(GL_DEPTH_TEST);
		blurProgram.UseProgram();
		blurProgram.UpdateUniform("ScaleU", glm::vec2(1.0/SHADOWMAP_SIZE, 0));
		glBindFramebuffer(GL_FRAMEBUFFER, blurFBO);
		glBindTexture(GL_TEXTURE_2D, currentSideTex); //Input-texture
		glClear(GL_COLOR_BUFFER_BIT);
		draw_fullscreen_quad();
		glBindTexture(GL_TEXTURE_2D, blurredTex);
		glBindFramebuffer(GL_FRAMEBUFFER, cubeFBOs[i]);
		blurProgram.UpdateUniform("ScaleU", glm::vec2(0, 1.0/SHADOWMAP_SIZE));
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		draw_fullscreen_quad();
		glEnable(GL_DEPTH_TEST);
#endif
	}

	// No mipmaps in cube (yet)
	//glBindTexture(GL_TEXTURE_2D, shadowMapTex);
	//glGenerateMipmap(GL_TEXTURE_2D);
	//glBindTexture(GL_TEXTURE_2D, 0);

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
	normalProgram.LoadFromFile("vsmcube/vertexShader.glsl", "vsmcube/fragmentShader.glsl");
	shadowProgram.LoadFromFile("vsmcube/shadowVertexShader.glsl", "vsmcube/shadowFragmentShader.glsl");
	blurProgram.LoadFromFile("blurVertexShader.glsl", "blurFragmentShader.glsl");
	
	// Create VAO
	cubeVao       = create_cube_vao();
	quadVao		  = create_quad_vao();

	// Cubemap
	cubeTex = texture::GenerateCube(SHADOWMAP_SIZE);
	cubeDepthTex = texture::GenerateDepthCube(SHADOWMAP_SIZE);
	texture::FramebufferCube(cubeFBOs, cubeTex, cubeDepthTex);

	blurredTex = texture::GenerateMipmapClampEdge(TYPE, SHADOWMAP_SIZE, SHADOWMAP_SIZE, TYPE2, GL_FLOAT);
	blurFBO = texture::Framebuffer(blurredTex, -1);
	currentSideTex = texture::GenerateMipmapClampEdge(TYPE, SHADOWMAP_SIZE, SHADOWMAP_SIZE, TYPE2, GL_FLOAT);
	currentSideDepthTex = texture::GenerateDepthMipmanClampBorder1(SHADOWMAP_SIZE);
	
	toCurrentSideFBO = texture::Framebuffer(currentSideTex, currentSideDepthTex);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CW);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);

	while (glfwGetWindowParam(GLFW_OPENED))
	{
		lightPos = glm::vec3(2,0,0);
		glm::mat4 mat = glm::translate(glm::mat4(), glm::vec3(0, 2, -5));
		mat *= glm::rotate(glm::mat4(), (float)glfwGetTime()*50, glm::vec3(0,1,0));

		lightPos = glm::vec3(mat * glm::vec4(lightPos, 1.0));

		draw_shadow_pass();
		draw_normal_pass();

		glfwSwapBuffers();

		if (glfwGetKey( GLFW_KEY_ESC ) == GLFW_PRESS)
			break;
	}

	glfwTerminate();

	// No cleanup :(
	return 0;
}