#version 450 compatibility 

uniform vec4 boxPos;
uniform vec4 boxLength;

uniform vec4 screenSize;

uniform vec4 uCamPos;
uniform mat4 invModelViewProjectMatrix;

uniform sampler2D depTex;
uniform sampler3D noiseTex;
uniform sampler2D disTex;

vec4 uLightDir;
vec4 uLightClr;

in vec3 localVertex;

//layout(early_fragment_tests) in;

vec3 minBox, maxBox;

float remap(float original_value, float original_min, float original_max, float new_min, float new_max)
{
	return new_min + (((original_value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

float GetHeightFractionForPoint(vec3 inPosition, vec2 inCloudMinMax)
{
	float height_fraction = (inPosition.z - inCloudMinMax.x) / (inCloudMinMax.y - inCloudMinMax.x);
	return clamp(height_fraction, 0, 1);
}

float sampleDensity(vec3 pos)
{
	vec3 lv = maxBox - minBox;
	float ln = min(lv.x, min(lv.y, lv.z));
	vec3 texCoord = (pos - minBox) / ln / 2.0;
	vec4 low_noise = texture(noiseTex, texCoord);
	float low_fbm = low_noise.g * 0.625 + low_noise.b * 0.25 + low_noise.a * 0.125;
	float density = remap(low_noise.r, low_fbm - 1, 1, 0, 1);

	//fake distruction
	density = remap(density, .55, 1., 0., 1.);

	return density;

	float vv = texture(disTex, texCoord.st).r;
	float heightPercent = (pos.z - minBox.z) / (maxBox.z - minBox.z);
	float heightGradient = clamp(remap(heightPercent, 0.0, vv, 1, 0), 0, 1);
	float baseDensity = density * heightGradient;

	if (density > 0)
	{
		float densityTmp = 1 - baseDensity;
		float detail = densityTmp * densityTmp * densityTmp;
		density = baseDensity - detail;
		return density;
	}
	return 0;
}

bool outbox(vec3 position)
{
	return position.x < minBox.x || position.x > maxBox.x ||
		position.y < minBox.y || position.y > maxBox.y ||
		position.z < minBox.z || position.z > maxBox.z;
}

vec3 lightMarch(vec3 position, float dstTravelled)
{
	vec3 lightDir = -vec3(-1, -1, -1);
	vec3 lightClr = vec3(1, 1, 1);
	float stepSize = 1;
	float totalDensity = 0;

	for (int step = 0; step < 256; step++) {
		position += lightDir * stepSize;

		if (outbox(position))
			break;

		totalDensity += max(0, sampleDensity(position));
	}
	//float lightAbsorption = 1;
	//float transmittance = exp(-totalDensity * lightAbsorption);
	//vec3 cloudColor = lerp(_colA, _LightColor0, saturate(transmittance * _colorOffset1));
	//cloudColor = lerp(_colB, cloudColor, saturate(pow(transmittance * _colorOffset2, 3)));
	//return _darknessThreshold + transmittance * (1 - _darknessThreshold) * cloudColor;
	return vec3(1, 1, 1);
}

void main()
{
	//////////////////////////////////////////////

	vec3 camPos = uCamPos.xyz;
	vec2 uv = gl_FragCoord.xy / screenSize.xy;
	float dep = texture(depTex, uv).r;
	vec4 depPos = vec4(uv * 2 - 1.0, 0, 1);
	depPos.z = dep * 2 - 1.0;
	depPos = invModelViewProjectMatrix * depPos;
	vec3 depPoz = depPos.xyz / depPos.w;

	vec3 dir = normalize(localVertex - camPos);
	minBox = boxPos.xyz - boxLength.xyz;
	maxBox = boxPos.xyz + boxLength.xyz;

	vec3 startPos = localVertex;
	if (gl_FrontFacing) {
		startPos = startPos += dir * 0.001;
	} else {
		depPos = vec4(uv * 2 - 1.0, -1, 1);
		depPos = invModelViewProjectMatrix * depPos;
		startPos = depPos.xyz / depPos.w;
	}

	float mx = min(boxLength.x, min(boxLength.y, boxLength.z));
	const float step =  mx / 10 * 0.2;

	float sum = 0;
	for (int i = 0; ; i++) {
		float stepSize = i * step;
		vec3 tmpPos = startPos + dir * stepSize;

		if (outbox(tmpPos)) {
			break;
		}

		if (dot(tmpPos - depPoz, dir) > 0) {
			//gl_FragColor = vec4(1, 0, 0, 1);
			//return;
			break;
		}

		//sum *= exp(-density * stepSize);
		//if (sum < 0.01)
		//	break;
		float density = sampleDensity(tmpPos);

		sum += density * 0.015;

		if (sum > 1)
			break;


		//sum *= exp(-density * stepSize);

		//if (sum < 0.01)
		//	break;
	}
	gl_FragColor = vec4(sum);
	//gl_FragColor = vec4(1, 0, 0, 1);
}
