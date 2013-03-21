#version 330

in vec4 v_position;
out vec4 outColor;

void main() {
	float depth = length( vec3(v_position) ) / 20;

	float moment1 = depth;
	float moment2 = depth * depth;

	float dx = dFdx(depth);
	float dy = dFdy(depth);
	moment2 += 0.25*(dx*dx+dy*dy);

	outColor = vec4( moment1, moment2, 0.0, 0.0);
};