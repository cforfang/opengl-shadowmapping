#version 330

in vec4 vpeye;
in vec4 vneye;
in vec2 Texcoord;
in vec4 sc;

out vec4 outColor;

uniform sampler2D shadowMap;

uniform mat4 view;
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

vec4 scPostW;

float chebyshevUpperBound(float distance)
{
	vec2 moments = texture2D(shadowMap,scPostW.xy).rg;
	
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
	vec3 viewDir  = normalize(-fragment);

	/* Shadows */
	scPostW = sc / sc.w; 
	scPostW = scPostW * 0.5 + 0.5;
	float shadowFactor = chebyshevUpperBound(scPostW.z);
	if(sc.w < 0) shadowFactor = 1;

	/* Diffuse lighting */
	// Convert to eye-space (TODO could precompute)
	vec3 light = vec3(view * vec4(light0.position, 1.0));

	vec4 diffColor = vec4(1,1,1,1);
	if(doTexture != 0)
		diffColor = texture2D(shadowMap, vec2(Texcoord.x, 1-Texcoord.y));

	// Vectors
	vec3 positionToLight = light - fragment;
	vec3 lightDir  = normalize(positionToLight);

	// Angle between fragment-normal and incoming light
	float cosAngIncidence = dot(lightDir, normal);
	cosAngIncidence = clamp(cosAngIncidence, 0, 1);

	float attenuation = 1.0f;
	attenuation = 1.0 / (light0.constantAttenuation + light0.linearAttenuation * length(positionToLight) + light0.quadraticAttenuation * pow(length(positionToLight),2));

	vec4 diffuse  = diffColor * light0.diffuse  * cosAngIncidence * attenuation;

	vec4 total_lighting;
	total_lighting += vec4(0.1, 0.1, 0.1, 1.0) * diffColor; // Ambient
	total_lighting += diffuse * shadowFactor; // Diffuse

   outColor = vec4(vec3(total_lighting), 1.0);
};