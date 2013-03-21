#version 330

in vec4 vpeye;
in vec4 vneye;
in vec2 Texcoord;

out vec4 outColor;

uniform samplerCube shadowCube;

uniform mat4 view, model;
uniform vec3 lightPos;
uniform float doTexture;

struct light
{
	vec3 position; //world-space
	vec4 diffuse;
	vec4 specular;
	float constantAttenuation, linearAttenuation, quadraticAttenuation;
};

light light0 = light(
	lightPos,		
	vec4(1,1,1,1), // diffuse
	vec4(1,1,1,1), // specular
	1.0, 0.0, 0.0  // atteniation (const, linear, quad)
);

float chebyshevUpperBound(float distance, vec3 dir)
{
	distance = distance/20 ;
	vec2 moments = texture(shadowCube, dir).rg;

	// Surface is fully lit. as the current fragment is before the light occluder
	if (distance <= moments.x)
		return 1.0;

	// The fragment is either in shadow or penumbra. We now use chebyshev's upperBound to check
	// How likely this pixel is to be lit (p_max)
	float variance = moments.y - (moments.x*moments.x);
	//variance = max(variance, 0.000002);
	variance = max(variance, 0.00002);

	float d = distance - moments.x;
	float p_max = variance / (variance + d*d);

	return p_max;
}

void main() 
{
	vec3 fragment = vec3(vpeye);
	vec3 normal   = vec3(normalize(vneye));
	vec3 fragmentToCamera_eye  = normalize(-fragment);

	/* Diffuse lighting */
	// Convert to eye-space (TODO could precompute)
	vec3 light = vec3(view * vec4(light0.position, 1.0));

	// Vectors
	vec3 fragmentToLight     = light - fragment;
	vec3 fragmentToLightDir  = normalize(fragmentToLight);

	/* Shadows */
	vec4 fragmentToLight_world = inverse(view) * vec4(fragmentToLightDir, 0.0);
	float shadowFactor = chebyshevUpperBound(length(fragmentToLight), -fragmentToLight_world);

	vec4 diffColor = vec4(1,1,1,1);
   //if(doTexture != 0)
		//diffColor = texture(shadowCube, -fragmentToLight_world.xyz);
		//diffColor = texture(shadowCube, lightSpacePos);
		//diffColor = texture(shadowCube, vec3(Texcoord.x, 1-Texcoord.y, -1));
		//diffColor = texture(shadowCube, vec3(Texcoord.x*2-1, +1, 2-Texcoord.y*2-1.0)); //+y
		//diffColor = texture(shadowCube, vec3(Texcoord.x*2-1, -1, 2-Texcoord.y*2-1.0)); //-y
		//diffColor = texture(shadowCube, vec3(Texcoord.x*2-1, 2-Texcoord.y*2-1.0, -1)); //-z
		//diffColor = texture(shadowCube, vec3(Texcoord.x*2-1, 2-Texcoord.y*2-1.0, +1)); //+z
		//diffColor = texture(shadowCube, vec3(+1, Texcoord.x*2-1, 2-Texcoord.y*2-1.0)); //+x
		//diffColor = texture(shadowCube, vec3(-1, Texcoord.x*2-1, 2-Texcoord.y*2-1.0)); //-x

	// Angle between fragment-normal and incoming light
	float cosAngIncidence = dot(fragmentToLightDir, normal);
	cosAngIncidence = clamp(cosAngIncidence, 0, 1);

	float attenuation = 1.0f;
	attenuation = 1.0 / (light0.constantAttenuation + light0.linearAttenuation * length(fragmentToLight) + light0.quadraticAttenuation * pow(length(fragmentToLight),2));
		
	vec4 diffuse  = diffColor * light0.diffuse  * cosAngIncidence * attenuation;

	vec4 total_lighting;
	total_lighting += vec4(0.1, 0.1, 0.1, 1.0) * diffColor; // Ambient
	total_lighting += diffuse * shadowFactor; // Diffuse

   outColor = vec4(vec3(total_lighting), 1.0);
};