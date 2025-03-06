#version 430 compatibility

//pos
uniform sampler2D tex0;
//norm
uniform sampler2D tex1;
//noise
uniform sampler2D tex2;

uniform vec3 samples[64];
uniform mat4 projection;

out float fragColor;

int kernelSize = 64;
float radius = 0.25;
float bias = 0.025;

// tile noise texture over screen based on screen dimensions divided by noise size
const vec2 noiseScale = vec2(1024.0 / 4.0, 1024.0 / 4.0);


void main()
{
	vec2 texCoords = gl_TexCoord[0].st;

	vec3 fragPos = texture(tex0, texCoords).xyz;
	vec3 normal = normalize(texture(tex1, texCoords).rgb);
	vec3 randomVec = normalize(texture(tex2, texCoords * noiseScale).xyz);
	vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 TBN = mat3(tangent, bitangent, normal);
	float occlusion = 0.0;
	for (int i = 0; i < kernelSize; ++i) {
		vec3 samplePos = TBN * samples[i]; // from tangent to view-space
		samplePos = fragPos + samplePos * radius;

		// project sample position (to sample texture) (to get position on screen/texture)
		vec4 offset = vec4(samplePos, 1.0);
		offset = projection * offset; // from view to clip-space
		offset.xyz /= offset.w; // perspective divide
		offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0

		// get sample depth
		float sampleDepth = texture(tex0, offset.xy).z; // get depth value of kernel sample

		// range check & accumulate
		float rangeCheck = smoothstep(0.05, 1.0, radius / abs(fragPos.z - sampleDepth));
		occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
	}
	occlusion = 1.0 - (occlusion / kernelSize);

	fragColor = occlusion;
}