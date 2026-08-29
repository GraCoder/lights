#version 330 compatibility

in vec3 normal;
in vec3 pos;

uniform vec3 viewPos;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragPos;

void main(void)
{
	const vec3 objectColor = vec3(0.5, 0.6, 0.7);
	//const vec3 lightColor = vec3(1, 1, 1);
	//const vec3 lightPos = vec3(100, 0, 100);
	//float ambientStrentch = 0.1;
	//vec3 ambient = ambientStrentch * lightColor;

	//vec3 norm = normalize(normal);
	//vec3 lightDir = normalize(lightPos - fragPos);
	//float diff = max(dot(norm, lightDir), 0);
	//vec3 diffuse = diff * lightColor;

	//float specularStrength = 0.5;
	//vec3 viewDir = normalize(viewPos - fragPos);
	//vec3 hfDir = normalize(viewDir + lightDir);
	//float spec = pow(max(dot(norm, hfDir), 0), 32);
	//vec3 specular = specularStrength * spec * lightColor;

	//vec3 result = (ambient + diffuse + specular) * objectColor;
	//gl_FragColor = vec4(result, 1.0);
	fragColor = objectColor;
	fragNormal = normal;
	fragPos = pos;
}