#version 330

layout(location = 0) in vec3 position;

uniform mat4 cameraToShadowProjector;
uniform mat4 model;

void main() {
	gl_Position = cameraToShadowProjector * model * vec4(position, 1.0);
}