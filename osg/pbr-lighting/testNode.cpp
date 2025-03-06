#include "testNode.h"
#include <osgUtil/CullVisitor>
#include <osg/MatrixTransform>


static const char *vertSource = R"(

#version 330 compatibility

out vec2 texCoord;
out vec3 fragPos;
out vec3 normal;

uniform mat4 model;

void main(void)
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

	fragPos = gl_Vertex.xyz;
	normal = gl_Normal.xyz;
	texCoord = gl_TexCoord[0].xy;
}

)";

static const char *fragSource = R"(

#version 330 compatibility 

in vec2 texCoord;
in vec3 fragPos;
in vec3 normal;

uniform vec3 albedo;
uniform float metallic;
uniform float roughness;
uniform float ao;

uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];
uniform vec3 camPos;

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

void main(void)
{
	vec3 n = normalize(normal);
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

	geometry->setVertexArray(positions);
	geometry->setNormalArray(normals, Array::BIND_PER_VERTEX);
	geometry->setTexCoordArray(0, uvs, Array::BIND_PER_VERTEX);
	geometry->addPrimitiveSet(indices);

	return geometry;
}

Vec3 lightPositions[] = {
	Vec3(-10.0f,  10.0f, 10.0f),
	Vec3( 10.0f,  10.0f, 10.0f),
	Vec3(-10.0f, -10.0f, 10.0f),
	Vec3( 10.0f, -10.0f, 10.0f),
};
Vec3 lightColors[] = {
	Vec3(300.0f, 300.0f, 300.0f),
	Vec3(300.0f, 300.0f, 300.0f),
	Vec3(300.0f, 300.0f, 300.0f),
	Vec3(300.0f, 300.0f, 300.0f)
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
			//ss->getOrCreateUniform("roughness", Uniform::FLOAT)->set(0.05f);
		}
	}

	auto ss = getOrCreateStateSet();
	osg::Program* program = new osg::Program;
	program->addShader(new osg::Shader(osg::Shader::VERTEX, vertSource));
	program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragSource));
	ss->setAttributeAndModes(program, osg::StateAttribute::ON);


	for(int i = 0; i < 4; i++)
		ss->getOrCreateUniform("lightPositions", Uniform::FLOAT_VEC3, 4)->setElement(i, lightPositions[i]);
	for(int i = 0; i < 4; i++)
		ss->getOrCreateUniform("lightColors", Uniform::FLOAT_VEC3, 4)->setElement(i, lightColors[i]);

	ss->getOrCreateUniform("ao", Uniform::FLOAT)->set(1.f);
	ss->getOrCreateUniform("albedo", Uniform::FLOAT_VEC3)->set(Vec3(0.02, 0, 0));
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

void TestNode::drawImplementation(RenderInfo &renderInfo) const
{
	
}
