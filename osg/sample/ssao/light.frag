#version 330  compatibility

uniform sampler2D light_pos;
uniform sampler2D light_norm;
uniform sampler2D light_albedo;
uniform sampler2D light_ssao;

uniform bool ssao_on;

struct Light {
	vec3 Position;
	vec3 Color;

	float Linear;
	float Quadratic;
};

void main()
{
	Light light;

	{
		light.Position = vec3(2, 2, 2);
		light.Color = vec3(0.5);
		light.Linear = 0.09;
		light.Quadratic = 0.032;
	}

	vec2 uv = gl_TexCoord[0].st;
	vec3 fragPos = texture(light_pos, uv).rgb;
	vec3 normal = texture(light_norm, uv).rgb;
	vec3 albedo = texture(light_albedo, uv).rgb;

	float ao = 1;
	if(ssao_on)
		ao = texture(light_ssao, uv).r;

	vec3 ambient = vec3(0.3 * albedo * ao);
	vec3 lighting = ambient;

	vec3 viewDir = normalize(-fragPos); 

	vec3 lightDir = normalize(light.Position - fragPos);
	vec3 diffuse = max(dot(normal, lightDir), 0.0) * albedo * light.Color;

	vec3 halfwayDir = normalize(lightDir + viewDir);
	float spec = pow(max(dot(normal, halfwayDir), 0.0), 8.0);
	vec3 specular = light.Color * spec;

	float distance = length(light.Position - fragPos);
	float attenuation = 1.0 / (1.0 + light.Linear * distance + light.Quadratic * distance * distance);
	diffuse *= attenuation;
	specular *= attenuation;
	lighting += diffuse + specular;

	gl_FragColor = vec4(lighting, 1.0);
}