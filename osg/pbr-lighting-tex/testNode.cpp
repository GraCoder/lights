#include "testNode.h"
#include <osgUtil/CullVisitor>
#include <osg/MatrixTransform>
#include <osg/Texture2D>
#include <osgDB/ReadFile>

#include <string>

static const char *vertSource = R"(

#version 330 compatibility

out vec3 fragPos;
out vec3 normal;

uniform mat4 model;

void main(void)
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

	fragPos = gl_Vertex.xyz;
	normal = gl_Normal.xyz;
	
	gl_TexCoord[0] = gl_MultiTexCoord0;
}

)";

static const char *fragSource = R"(

#version 330 compatibility 

in vec3 fragPos;
in vec3 normal;

uniform sampler2D   albedoMap;
uniform sampler2D	metallicMap;
uniform sampler2D	roughnessMap;
uniform sampler2D	normalMap;
uniform sampler2D	aoMap;

uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];
uniform vec3 camPos;

vec2 texCoord;

const float pi = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}  

float distributionGGX(vec3 n, vec3 h, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndotH = max(dot(n, h), 0.0);
    float ndotH2 = ndotH * ndotH;

    float denom = (ndotH2 * (a2 - 1.0) + 1.0);
    denom = pi * denom * denom;

    return a2 / max(denom, 0.0000001); 
}

float schlickGGX(float ndotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float denom = ndotV * (1.0 - k) + k;

    return ndotV / denom;
}

float smithGeometry(vec3 n, vec3 v, vec3 l, float roughness)
{
    float ndotV = max(dot(n, v), 0.0);
    float ndotL = max(dot(n, l), 0.0);
    float ggx2 = schlickGGX(ndotV, roughness);
    float ggx1 = schlickGGX(ndotL, roughness);

    return ggx1 * ggx2;
}

vec3 getNormalFromMap()
{
	vec3 tangent = texture(normalMap, texCoord).xyz * 2.0 - 1.0;
	vec3 q1 = dFdx(fragPos);
	vec3 q2 = dFdy(fragPos);
	vec2 st1 = dFdx(texCoord);
	vec2 st2 = dFdy(texCoord);

	vec3 n = normalize(normal);
	vec3 t = normalize(q1 * st2.t - q2 * st1.t);
	vec3 b = -normalize(cross(n, t));
	mat3 tbn = mat3(t, b, n);
	return normalize(tbn * tangent);
}

void main(void)
{
	texCoord = gl_TexCoord[0].st;
	vec3 albedo = pow(texture(albedoMap, texCoord).rgb, vec3(2.2));
	float metallic = texture(metallicMap, texCoord).r;
	float roughness = texture(roughnessMap, texCoord).r;
	float ao = texture(aoMap, texCoord).r;

	vec3 n = getNormalFromMap();
	vec3 v = normalize(camPos - fragPos);

	vec3 f0 = vec3(0.04);
	f0 = mix(f0, albedo, metallic);
	vec3 lo = vec3(0.0);
	for(int i = 0; i < 4; i++)
	{
		vec3 l = normalize(lightPositions[i] - fragPos);
		vec3 h = normalize(v + l);
		float distance = length(lightPositions[i] - fragPos);
		float attenuation = 1.0 / (distance * distance);
		vec3 radiance = lightColors[i] * attenuation;

		float nv = distributionGGX(n, h, roughness);
		float gv = smithGeometry(n, v, l, roughness);
		vec3 fv = fresnelSchlick(clamp(dot(h, v), 0.0, 1.0), f0);

		vec3 nominator = nv * gv * fv;
		float denominator = 4 * max(dot(n, v), 0) * max(dot(n, l), 0.0);
		vec3 specular = nominator / max(denominator, 0.001);

		vec3 ks = fv; vec3 kd = vec3(1.0) - ks;
		kd *= (1.0 - metallic);

		float ndotL = max(dot(n, l), 0);

		lo += (kd *albedo / pi + specular) * radiance * ndotL;
	}
	
	vec3 ambient = vec3(0.03) * albedo * ao;
	vec3 color = ambient + lo;

	color = color / (color + vec3(1.0));
	color = pow(color, vec3(1.0/2.2));

	gl_FragColor = vec4(color, 1.0);
}

)" ;

osg::Geometry* createSphere(Vec3 pos, float radius)
{
	auto geometry = new Geometry;
	auto positions = new Vec3Array;
	auto normals = new Vec3Array;
	auto uvs = new Vec2Array;
	auto indices = new osg::DrawElementsUShort(GL_TRIANGLE_STRIP);

	const unsigned int X_SEGMENTS = 64;
	const unsigned int Y_SEGMENTS = 64;
	const float PI = 3.14159265359;
	for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
	{
		for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
		{
			float xSegment = (float)x / (float)X_SEGMENTS;
			float ySegment = (float)y / (float)Y_SEGMENTS;
			float xPos = radius * std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
			float yPos = radius * std::cos(ySegment * PI);
			float zPos = radius * std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

			positions->push_back(Vec3(xPos, yPos, zPos) + pos);
			uvs->push_back(Vec2(xSegment, ySegment));
			normals->push_back(Vec3(xPos, yPos, zPos));
		}
	}

	bool oddRow = false;
	for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
	{
		if (!oddRow) // even rows: y == 0, y == 2; and so on
		{
			for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
			{
				indices->push_back(y       * (X_SEGMENTS + 1) + x);
				indices->push_back((y + 1) * (X_SEGMENTS + 1) + x);
			}
		}
		else
		{
			for (int x = X_SEGMENTS; x >= 0; --x)
			{
				indices->push_back((y + 1) * (X_SEGMENTS + 1) + x);
				indices->push_back(y       * (X_SEGMENTS + 1) + x);
			}
		}
		oddRow = !oddRow;
	}

	geometry->setUseVertexBufferObjects(true);
	geometry->setVertexArray(positions);
	geometry->setNormalArray(normals, Array::BIND_PER_VERTEX);
	geometry->setTexCoordArray(0, uvs);
	geometry->addPrimitiveSet(indices);

	return geometry;
}

Vec3 lightPositions[] = {
	Vec3(0.0f,  0.0f, 10.0f),
};
Vec3 lightColors[] = {
	Vec3(150.0f, 150.0f, 150.0f),
};

TestNode::TestNode()
{
	float space = 2.5, radius = 1;
	for (int i = 0; i < 7; i++)
	{
		for (int j = 0; j < 7; j++)
		{
			auto gm = createSphere(Vec3((j - 3) * space, (i - 3) * space, 0), radius);
			addChild(gm);

			auto ss = gm->getOrCreateStateSet();
			ss->getOrCreateUniform("metallic", Uniform::FLOAT)->set(float(i) / 7);
			ss->getOrCreateUniform("roughness", Uniform::FLOAT)->set(osg::maximum(osg::minimum(float(j) / 7, 1.0f), 0.05f));
		}
	}

	auto ss = getOrCreateStateSet();
	ss->setMode(GL_LIGHTING, StateAttribute::OFF);
	osg::Program* program = new osg::Program;
	program->addShader(new osg::Shader(osg::Shader::VERTEX, vertSource));
	program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragSource));
	ss->setAttributeAndModes(program, osg::StateAttribute::ON);

	for(int i = 0; i < 4; i++)
		ss->getOrCreateUniform("lightPositions", Uniform::FLOAT_VEC3, 4)->setElement(i, lightPositions[i]);
	for(int i = 0; i < 4; i++)
		ss->getOrCreateUniform("lightColors", Uniform::FLOAT_VEC3, 4)->setElement(i, lightColors[i]);


	std::string texPath = "D:\\dev\\lights\\resources\\textures\\pbr\\rusted_iron\\";
	//std::string texPath = "D:\\dev\\gl-lighting\\gl-lighting\\resources\\textures\\pbr\\rusted_iron\\";
	auto albedo = new osg::Texture2D(osgDB::readImageFile(texPath + "albedo.png"));
	albedo->setFilter(Texture::MAG_FILTER, Texture::LINEAR);
	albedo->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
	albedo->setResizeNonPowerOfTwoHint(false);
	auto metallic = new osg::Texture2D(osgDB::readImageFile(texPath + "metallic.png"));
	metallic->setFilter(Texture::MAG_FILTER, Texture::LINEAR);
	metallic->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
	metallic->setResizeNonPowerOfTwoHint(false);
	auto roughness = new osg::Texture2D(osgDB::readImageFile(texPath + "roughness.png"));
	roughness->setFilter(Texture::MAG_FILTER, Texture::LINEAR);
	roughness->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
	roughness->setResizeNonPowerOfTwoHint(false);
	auto normal = new osg::Texture2D(osgDB::readImageFile(texPath + "normal.png"));
	normal->setFilter(Texture::MAG_FILTER, Texture::LINEAR);
	normal->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
	normal->setResizeNonPowerOfTwoHint(false);
	auto ao = new osg::Texture2D(osgDB::readImageFile(texPath + "ao.png"));
	ao->setFilter(Texture::MAG_FILTER, Texture::LINEAR);
	ao->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
	ao->setResizeNonPowerOfTwoHint(false);

	ss->setTextureAttribute(0, albedo);	ss->getOrCreateUniform("albedoMap", osg::Uniform::SAMPLER_2D)->set(0);
	ss->setTextureAttribute(1, metallic); ss->getOrCreateUniform("metallicMap", osg::Uniform::SAMPLER_2D)->set(1);
	ss->setTextureAttribute(2, roughness);	ss->getOrCreateUniform("roughnessMap", osg::Uniform::SAMPLER_2D)->set(2);
	ss->setTextureAttribute(3, normal);	ss->getOrCreateUniform("normalMap", osg::Uniform::SAMPLER_2D)->set(3);
	ss->setTextureAttribute(4, ao);	ss->getOrCreateUniform("aoMap", osg::Uniform::SAMPLER_2D)->set(4);
}

void TestNode::traverse(NodeVisitor & nv)
{
	if (nv.getVisitorType() == NodeVisitor::CULL_VISITOR)
	{
		auto ss = getOrCreateStateSet();
		auto vp = nv.asCullVisitor()->getViewPoint();
		ss->getOrCreateUniform("camPos", osg::Uniform::FLOAT_VEC3)->set(vp);
	}
	Group::traverse(nv);
}
