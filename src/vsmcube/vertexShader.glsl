#version 330

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texcoord;

out vec4 vneye;
out vec4 vpeye;
out vec4 sc;
out vec2 Texcoord;

uniform mat4 model, view, proj;

void main() {
	Texcoord = texcoord;
	gl_Position = proj * view * model * vec4(position, 1.0f);
	vneye = view * model * vec4(normal,   0.0f);
	vpeye = view * model * vec4(position, 1.0);
}