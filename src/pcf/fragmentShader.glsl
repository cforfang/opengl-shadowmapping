#version 330

in vec4 vpeye;
in vec4 vneye;
in vec2 Texcoord;
in vec4 sc;

out vec4 outColor;

// Both samplers are to the same depth-texture (unit 0)
uniform sampler2D shadowMap;
uniform sampler2DShadow shadowMapS;

uniform mat4 view;
uniform vec3 lightPos;
uniform float doTexture;

// 0 = MANUAL
// 1 = SM_HW_PCF
// 2 = SM_PCF
// 3 = SM_PCF2
uniform int samplingType = 0;

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

void main() {

   vec3 fragment = vec3(vpeye);
   vec3 normal   = vec3(normalize(vneye));
   vec3 viewDir  = normalize(-fragment);

	/* Shadows */
	float shadowFactor = 1.0;
	vec4 scPostW = sc / sc.w;

	if (sc.w <= 0.0f || (scPostW.x < 0 || scPostW.y < 0) || (scPostW.x >= 1 || scPostW.y >= 1)) {
		// Behind or outside frustrum: no shadow
		shadowFactor = 1;
	} else {
		if(samplingType == 0) {
			// Standard shadow mapping, done manually
			float shadow = texture2D(shadowMap, scPostW.xy).x;
			float epsilon = 0.00001;
			if (shadow + epsilon < scPostW.z) shadowFactor = 0.0;
		}
		else if(samplingType == 1) {
			// Using a sampler2DShadow (instead of doing it manually like above) could
			// give us some free filtering (with GL_LINEAR
			shadowFactor = textureProj(shadowMapS, sc);
		}
		else if(samplingType == 2) {
			// Manual 4x PCF
			float shadow = 0.0;
			shadow += textureProjOffset(shadowMapS, sc, ivec2(-1,  1));
			shadow += textureProjOffset(shadowMapS, sc, ivec2( 1,  1));
			shadow += textureProjOffset(shadowMapS, sc, ivec2(-1, -1));
			shadow += textureProjOffset(shadowMapS, sc, ivec2( 1, -1));
			shadowFactor = shadow / 4.0;
		}
		else if(samplingType == 3) {
			// PCF X-sample-version
			ivec2 ts = textureSize(shadowMap, 0);
			float startstop = 2; // 1 = 9x, 2 = 25x, 3 = 49x, ...
	
			float sum = 0, count = 0;
			float x, y;
			for (y = -startstop; y <= startstop; y += 1.0)
			for (x = -startstop; x <= startstop; x += 1.0) {
				// Can't use texture(Proj)Offset directly since it expects the offset to be a constant value,
				// i.e. no loops, so instead we calculate the offset manually (given the texture size)
				vec2 texmapscale = vec2(1.0/ts.x, 1.0/ts.y);
				vec2 offset = vec2(x, y);
				sum += textureProj(shadowMapS, vec4(sc.xy + offset * texmapscale * sc.w, sc.z, sc.w));
				count++;
			}

			shadowFactor = sum / count;
		}
	}

	/* Per-fragment diffuse lighting */
	// Convert to eye-space
	vec3 light = vec3(view * vec4(light0.position, 1.0));

	vec4 diffColor = vec4(1,1,1,1);
	if(doTexture != 0) // Textures the cube with the shadowMap
		diffColor = texture2D(shadowMap, vec2(Texcoord.x, 1-Texcoord.y));

	vec3 positionToLight = light - fragment;
	vec3 lightDir  = normalize(positionToLight);

	// Angle between fragment-normal and incoming light
	float cosAngIncidence = dot(lightDir, normal);
	cosAngIncidence = clamp(cosAngIncidence, 0, 1);

	float attenuation = 1.0;
	attenuation = 1.0 / (light0.constantAttenuation + light0.linearAttenuation * length(positionToLight) + light0.quadraticAttenuation * pow(length(positionToLight),2));
	vec4 diffuse  = diffColor * light0.diffuse  * cosAngIncidence * attenuation;

	vec4 total_lighting;
	total_lighting += vec4(0.1, 0.1, 0.1, 1.0) * diffColor; // Ambient
	total_lighting += diffuse * shadowFactor; // Diffuse

   outColor = vec4(vec3(total_lighting), 1.0);
}