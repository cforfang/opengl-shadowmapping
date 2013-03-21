#include <iostream>
#include <stdexcept>

#include "Common.hpp"
#include "OpenGL.hpp"
#include "ShaderProgram.hpp"

// Type of mapping:
// SM_MANUAL (calculation from shadow map)
// SM_HW_PCF (show how GL_LINEAR can give free PCF using build-in GLSL function (on sampler2DShadow))
// SM_PCF    (manual PCF)
// SM_PCF2   (manual PCF where one can choose #-samples -- see shader code below)
#define SM_PCF2

// Texture
#define TEX_LINEAR // TEX_LINEAR or TEX_NEAREST -- note that LINEAR is needed to get HW PCF

// Need define since used in shader (for PCF2)
#define SHADOWMAP_SIZE 512

// Window-size
static const int WIDTH  = 1280;
static const int HEIGHT = 720;

// Object positions (world coordinates)
static glm::vec3 lightPos(-2.0, 2.0, -2);
static glm::vec3 cubePos(0.0, 0.0, -5.0);
static glm::vec3 planePos(1,-1,-6);
static glm::vec3 planeScale(7,1,7); // It's a scaled cube

static ShaderProgram program, shadowProgram;
static GLuint cubeVao, shadowMapTex, shadowMapFBO;

#define QU(x) #x
#define QUH(x) QU(x)
#define SM_SIZE_STR QUH(SHADOWMAP_SIZE)

static const char* normalVertexSource =
	"#version 330\n"

	"layout(location = 0) in vec3 position;"
	"layout(location = 1) in vec3 normal;"
	"layout(location = 2) in vec2 texcoord;"

	"out vec4 vneye;"
	"out vec4 vpeye;"
	"out vec4 sc;"
	"out vec2 Texcoord;"

	"uniform mat4 cameraToShadowProjector;"
	"uniform mat4 model, view, proj;"

	"const mat4 bias = mat4(0.5, 0.0, 0.0, 0.0,"
						  " 0.0, 0.5, 0.0, 0.0,"
						   "0.0, 0.0, 0.5, 0.0,"
						  " 0.5, 0.5, 0.5, 1.0);"

	"void main() {"
	"	Texcoord = texcoord;"
	"	gl_Position = proj * view * model * vec4(position, 1.0f);"
	"   vneye = view * model * vec4(normal,   0.0f);"
	"   vpeye = view * model * vec4(position, 1.0);"
	"   sc = bias * cameraToShadowProjector * model * vec4(position, 1.0f);"
	"}";

static const char* normalFragmentSource =
	"#version 330\n"

	"in vec4 vpeye;"
	"in vec4 vneye;"
	"in vec2 Texcoord;"
	"in vec4 sc;"

	"out vec4 outColor;"

	 // Both samplers are to the same depth-texture
	"uniform sampler2D shadowMap;"
	"uniform sampler2DShadow shadowMapS;"

	"uniform mat4 view;"
	"uniform vec3 lightPos;"
	"uniform float doTexture;"

	"struct light"
	"{"
	"	vec3 position;" //world-space
	"	vec4 diffuse;"
	"	vec4 specular;"
	"	float constantAttenuation, linearAttenuation, quadraticAttenuation;"
	"};"

	"light light0 = light("
	"	lightPos,"		
	"	vec4(1,1,1,1)," // diffuse
	"	vec4(1,1,1,1)," // specular
	"	1.0, 0.0, 0.0"  // atteniation (const, linear, quad)
	");"

	"void main() {"

	"   vec3 fragment = vec3(vpeye);"
	"   vec3 normal   = vec3(normalize(vneye));"
	"   vec3 viewDir  = normalize(-fragment);"

	/* Shadows */
	"	float shadowFactor = 1.0; "
	"	vec4 scPostW = sc / sc.w;"

	"	if (sc.w <= 0.0f || (scPostW.x < 0 || scPostW.y < 0) || (scPostW.x >= 1 || scPostW.y >= 1)) {"
			// Behind or outside frustrum: no shadow
	"		shadowFactor = 1;"
	"	} else {"
#ifdef SM_MANUAL
			// Standard shadow mapping
	"		float shadow = texture2D(shadowMap, scPostW.xy).x;"
	"		float epsilon = 0.00001; "
	"		if (shadow + epsilon < scPostW.z) shadowFactor = 0.0;"
#endif
#ifdef SM_HW_PCF
			// Using a sampler2DShadow (instead of doing it manually like above) could
			// give us some free filtering (with GL_LINEAR-textures)
	"		shadowFactor = textureProj(shadowMapS, sc);"
#endif
#ifdef SM_PCF // PCF 4x
	"		float shadow = 0.0;"
	"		shadow += textureProjOffset (shadowMapS, sc, ivec2 (-1,  1));"
	"		shadow += textureProjOffset (shadowMapS, sc, ivec2 ( 1,  1));"
	"		shadow += textureProjOffset (shadowMapS, sc, ivec2 (-1, -1));"
	"		shadow += textureProjOffset (shadowMapS, sc, ivec2 ( 1, -1));"
	"		shadowFactor = shadow / 4.0;"

#endif
#ifdef SM_PCF2
		// PCF X-sample-version
	"	float startstop = 2;" // 1 = 9x, 2 = 25x, 3 = 49x, ...
	
	"	float sum = 0, count = 0;"
	"	float x, y;"
	"	for (y = -startstop; y <= startstop; y += 1.0)"
	"	for (x = -startstop; x <= startstop; x += 1.0) {"
			// Can't use texture(Proj)Offset directly since it expects the offset to be a constant value,
			// i.e. no loops, so instead we calculate the offset manually (given the texture size)
	"       vec2 texmapscale = vec2(1/" SM_SIZE_STR ".0, 1/" SM_SIZE_STR ".0);"
	"       vec2 offset = vec2(x, y);"
	"		sum += textureProj(shadowMapS, vec4(sc.xy + offset * texmapscale * sc.w, sc.z, sc.w));"
	"       count++;"
	"   }"

	"	shadowFactor = sum / count;"
#endif
	"	}"

	/* Per-fragment diffuse lighting */
		// Convert to eye-space
	"	vec3 light = vec3(view * vec4(light0.position, 1.0));"


	"	vec4 diffColor = vec4(1,1,1,1);"
	"   if(doTexture != 0)" // Textures the cube with the shadowMap
	"		diffColor = texture2D(shadowMap, vec2(Texcoord.x, 1-Texcoord.y));"

		// Vectors
	"	vec3 positionToLight = light - fragment;"
	"	vec3 lightDir  = normalize(positionToLight);"

		// Angle between fragment-normal and incoming light
	"	float cosAngIncidence = dot(lightDir, normal);"
	"	cosAngIncidence = clamp(cosAngIncidence, 0, 1);"

	"	float attenuation = 1.0f;"
	"	attenuation = 1.0 / (light0.constantAttenuation + light0.linearAttenuation * length(positionToLight) + light0.quadraticAttenuation * pow(length(positionToLight),2));"
	"	vec4 diffuse  = diffColor * light0.diffuse  * cosAngIncidence * attenuation;"

	"	vec4 total_lighting;"
	"	total_lighting += vec4(0.1, 0.1, 0.1, 1.0) * diffColor;" // Ambient
	"	total_lighting += diffuse * shadowFactor;" // Diffuse

	"   outColor = vec4(vec3(total_lighting), 1.0);"
	"}";

// Vertex-shader for generating the shadow-map
static const char* shadowVertexSource =
	"#version 330\n"

	"layout(location = 0) in vec3 position;"

	"uniform mat4 cameraToShadowProjector;"
	"uniform mat4 model;"

	"void main() {"
	"	gl_Position = cameraToShadowProjector * model * vec4(position, 1.0);"
	"}";

// Fragment-shader for generating the shadow-map
static const char* shadowFragmentSource =
	"#version 330\n"
	"void main() {}";

bool init_opengl()
{
	// Initialize GLFW
	if( !glfwInit() ) 
		return false;

	glfwOpenWindowHint(GLFW_FSAA_SAMPLES, 4);
	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 3);
	glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwOpenWindowHint(GLFW_WINDOW_NO_RESIZE, GL_TRUE);
	glfwOpenWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	
	if (!glfwOpenWindow(WIDTH, HEIGHT, 0, 0, 0, 0, 24, 0, GLFW_WINDOW)) {
		glfwTerminate();
		return false;
	}

	// Load OpenGL-functions
	if (gl3wInit()) {
		fprintf(stderr, "failed to initialize OpenGL\n");
		return false;
	}
	if (!gl3wIsSupported(3, 3)) {
		fprintf(stderr, "OpenGL 3.3 not supported\n");
		return false;
	}

	// Enable debug output (avoid glGetError() all over the place)
	if(GL_ARB_debug_output)
	{
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
		glDebugMessageCallbackARB(DebugFunc, (void*)15);
	}

	glfwSetWindowTitle("OpenGL");

	// Print info
	printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION),
		glGetString(GL_SHADING_LANGUAGE_VERSION));

	return true;
}

void set_shadow_matrix_uniform(ShaderProgram &prog)
{
	glm::mat4 mat;
	mat *= glm::perspective(60.0f, 1.0f, 1.0f, 10.0f);
	mat *= glm::lookAt(lightPos, cubePos, glm::vec3(0,1,0)); // Point toward object regardless of position
	prog.UpdateUniform("cameraToShadowProjector", mat);
}

void draw_cubes(ShaderProgram &program, bool shadowpass)
{
	glBindVertexArray(cubeVao);

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

void draw_normal_pass()
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

	set_shadow_matrix_uniform(program);
	draw_cubes(program, false /*not shadowpass*/);
}

void draw_shadow_pass()
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
	// Set up OpenGL-context
	if(!init_opengl()){
		std::cout << "Error setting up OpenGL-context.\n";
		return -1;	
	}

	// Create programs
	program.LoadWithSource(normalVertexSource, normalFragmentSource);
	shadowProgram.LoadWithSource(shadowVertexSource, shadowFragmentSource);

	// Create VAO
	cubeVao = create_cube_vao();

	// ShadowMap-texture
	glGenTextures(1, &shadowMapTex);
	glBindTexture(GL_TEXTURE_2D, shadowMapTex);
	glTexImage2D (GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOWMAP_SIZE, SHADOWMAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
#ifdef TEX_LINEAR
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#else
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#endif
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	GLfloat border[] = {1.0f, 1.0f, 1.0f, 1.0f};
	glTexParameterfv (GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
#ifdef SM_MANUAL
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
#else
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
#endif

	// ShadowMap-FBO
	glGenFramebuffers(1, &shadowMapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMapTex, 0);
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

	while (glfwGetWindowParam(GLFW_OPENED))
	{
		draw_shadow_pass();
		draw_normal_pass();

		glfwSwapBuffers();

		if (glfwGetKey( GLFW_KEY_ESC ) == GLFW_PRESS)
			break;
	}

	glfwTerminate();

	// OS handles cleanup

	return 0;
}