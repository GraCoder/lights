#include "testNode.h"
#include <osgUtil/CullVisitor>
#include <osg/MatrixTransform>
#include <osg/Texture2D>
#include <osg/TextureCubeMap>
#include <osg/CullFace>
#include <osgDB/ReadFile>
#include <osgUtil/CullVisitor>

#define GL_RGB16F 0x881B

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
uniform samplerCube irrMap;

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

	vec3 ks = fresnelSchlick(max(dot(n, v), 0.0), f0);
	vec3 kd = 1.0 - ks;
	kd *= 1.0 - metallic;
	vec3 irr = texture(irrMap, n).rgb;
	vec3 diffuse = irr * albedo;
	vec3 ambient = (kd * diffuse) * ao;

	//vec3 ambient = vec3(0.03) * albedo * ao;
	vec3 color = ambient + lo;

	color = color / (color + vec3(1.0));
	color = pow(color, vec3(1.0/2.2));

	gl_FragColor = vec4(color, 1.0);
}

)" ;

static const char *hdrVertSource = R"(

#version 330 compatibility

out vec3 localPos;

void main(void)
{
	localPos = gl_Vertex.xyz;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

)";

static const char *hdrFragSource = R"(

#version 330 compatibility 

in vec3 localPos;

uniform samplerCube envMap;
uniform vec3 color;

void main(void)
{
	vec3 envColor = texture(envMap, localPos).rgb;
	envColor = envColor / (envColor + vec3(1.0));
	envColor = pow(envColor, vec3(1.0 / 2.2));
	gl_FragColor = vec4(envColor, 1.0);
}

)";

static const char *intVertSource = R"(

#version 330 compatibility

out vec3 localPos;

void main(void)
{
	localPos = gl_Vertex.xyz;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

)";

static const char *intFragSource = R"(

#version 330 compatibility 

in vec3 localPos;

uniform sampler2D equirecTangularMap;

vec2 sampleSphereUV(vec3 v)
{
	float len = length(v);
	vec2 uv = vec2(atan(v.z, v.x), asin(v.y / len));
	uv *= vec2(0.1591, 0.3183);
	uv += 0.5;
	return uv;
}

void main(void)
{
	vec2 uv = sampleSphereUV(localPos);
	vec3 color = texture(equirecTangularMap, uv).rgb;
	gl_FragColor = vec4(color, 1.0);
}

)";

static const char *irrFragSource = R"(

#version 330 compatibility 

in vec3 localPos;

uniform samplerCube environmentMap;

const float pi = 3.14159265359;

void main(void)
{
	vec3 n = normalize(localPos);
	vec3 irr = vec3(0);
	vec3 up = vec3(0, 0, 1);
	vec3 rt = normalize(cross(up, n));
	up = normalize(cross(n, rt));
	float sampleDelta = 0.025;
	float numSamples = 0.0;
	for(float phi = 0.0; phi < 2.0 * pi; phi += sampleDelta)
	{
		for(float theta = 0.0; theta < 0.5 * pi; theta += sampleDelta)
		{	
			vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			vec3 sampleVec = tangentSample.x * rt + tangentSample.y * up + tangentSample.z * n;
			irr += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
			numSamples++;	
		}
	}
	irr = pi * irr *(1.0 / float(numSamples));
	gl_FragColor = vec4(irr, 1.0);
}

)";

static const float metric = 200;

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
	ss->getOrCreateUniform("albedo", Uniform::FLOAT_VEC3)->set(Vec3(0.5, 0, 0));

	auto geometry = new osg::Geometry;
	{
		auto vertexs = new Vec3Array;
		{
			vertexs->push_back({-metric, -metric, -metric});
			vertexs->push_back({ metric, -metric, -metric});
			vertexs->push_back({ metric,  metric, -metric});
			vertexs->push_back({-metric,  metric, -metric});

			vertexs->push_back({-metric, -metric,  metric});
			vertexs->push_back({-metric,  metric,  metric});
			vertexs->push_back({ metric,  metric,  metric});
			vertexs->push_back({ metric, -metric,  metric});

			vertexs->push_back({-metric, -metric, -metric});
			vertexs->push_back({ metric, -metric, -metric});
			vertexs->push_back({ metric, -metric,  metric});
			vertexs->push_back({-metric, -metric,  metric});

			vertexs->push_back({+metric, -metric, -metric});
			vertexs->push_back({+metric, +metric, -metric});
			vertexs->push_back({+metric, +metric,  metric});
			vertexs->push_back({+metric, -metric,  metric});

			vertexs->push_back({ metric,  metric, -metric});
			vertexs->push_back({-metric,  metric, -metric});
			vertexs->push_back({-metric,  metric,  metric});
			vertexs->push_back({ metric,  metric,  metric});

			vertexs->push_back({-metric,  metric, -metric});
			vertexs->push_back({-metric, -metric, -metric});
			vertexs->push_back({-metric, -metric,  metric});
			vertexs->push_back({-metric,  metric,  metric});
		
			geometry->setVertexArray(vertexs);
		}

		auto normals = new Vec3Array;
		normals->push_back({0, 0, -1});
		geometry->setNormalArray(normals, Array::BIND_PER_PRIMITIVE_SET);
		
		{
			int iidxs[] = { 0, 4, 8, 12, 16, 20 };
			std::vector<uint32_t> idxs;
			for (int i = 0; i < 6; i++)
			{
				idxs.push_back(iidxs[i]);
				idxs.push_back(iidxs[i] + 1);
				idxs.push_back(iidxs[i] + 2);
				idxs.push_back(iidxs[i]);
				idxs.push_back(iidxs[i] + 2);
				idxs.push_back(iidxs[i] + 3);
			}
			geometry->addPrimitiveSet(new osg::DrawElementsUInt(GL_TRIANGLES, idxs.begin(), idxs.end()));
		}

		addChild(geometry);

		auto ss = geometry->getOrCreateStateSet();
		osg::Program* program = new osg::Program;
		program->addShader(new osg::Shader(osg::Shader::VERTEX, hdrVertSource));
		program->addShader(new osg::Shader(osg::Shader::FRAGMENT, hdrFragSource));
		ss->setAttributeAndModes(program, osg::StateAttribute::ON);

		std::string resPath = "D:\\dev\\lights\\resources\\";
		auto img = osgDB::readImageFile(resPath + "hdr\\newport_loft.hdr");
		img->setPixelFormat(GL_RGB);
		img->setInternalTextureFormat(GL_RGB);
		auto tex = new Texture2D(img);
		tex->setInternalFormat(GL_RGB16F);
		tex->setFilter(Texture::MIN_FILTER, tex->LINEAR);
		tex->setFilter(Texture::MAG_FILTER, tex->LINEAR);
		tex->setWrap(tex->WRAP_S, tex->CLAMP_TO_EDGE);
		tex->setWrap(tex->WRAP_T, tex->CLAMP_TO_EDGE);
		tex->setResizeNonPowerOfTwoHint(false);


		auto envMap = new osg::TextureCubeMap;
		envMap->setTextureSize(512, 512);
		envMap->setInternalFormat(GL_RGBA);
		envMap->setFilter(envMap->MIN_FILTER, envMap->LINEAR);
		envMap->setFilter(envMap->MAG_FILTER, envMap->LINEAR);
		envMap->setWrap(envMap->WRAP_S, envMap->CLAMP_TO_EDGE);
		envMap->setWrap(envMap->WRAP_T, envMap->CLAMP_TO_EDGE);
		envMap->setWrap(envMap->WRAP_R, envMap->CLAMP_TO_EDGE);
		_tex = envMap;

		ss->setTextureAttribute(0, _tex, StateAttribute::ON);
		ss->getOrCreateUniform("envMap", Uniform::SAMPLER_CUBE)->set(0);

		auto lightMap = new osg::TextureCubeMap;
		lightMap->setTextureSize(32, 32);
		lightMap->setInternalFormat(GL_RGBA);
		lightMap->setFilter(lightMap->MIN_FILTER, lightMap->LINEAR);
		lightMap->setFilter(lightMap->MAG_FILTER, lightMap->LINEAR);
		lightMap->setWrap(lightMap->WRAP_S, lightMap->CLAMP_TO_EDGE);
		lightMap->setWrap(lightMap->WRAP_T, lightMap->CLAMP_TO_EDGE);
		lightMap->setWrap(lightMap->WRAP_R, lightMap->CLAMP_TO_EDGE);
		getOrCreateStateSet()->setTextureAttribute(0, lightMap, StateAttribute::ON);
		getOrCreateStateSet()->getOrCreateUniform("irrMap", Uniform::SAMPLER_CUBE)->set(0);
		_texIrr = lightMap;

		{
			auto cam = new Camera;
			cam->setRenderOrder(cam->PRE_RENDER);
			cam->setReferenceFrame(cam->ABSOLUTE_RF);
			cam->setComputeNearFarMode(cam->DO_NOT_COMPUTE_NEAR_FAR);
			cam->setViewport(0, 0, 512, 512);
			cam->setRenderTargetImplementation(cam->FRAME_BUFFER_OBJECT);
			cam->setProjectionMatrix(Matrix::perspective(90.0f, 1.0f, 0.1f, metric * 2));
			cam->addChild(geometry);
			cam->setName("pre render");
			cam->setClearMask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
			auto ss = cam->getOrCreateStateSet();
			osg::Program* program = new Program;
			program->addShader(new Shader(Shader::VERTEX, intVertSource));
			program->addShader(new Shader(Shader::FRAGMENT, intFragSource));
			ss->setAttributeAndModes(program, StateAttribute::OVERRIDE);
			ss->setTextureAttribute(0, tex,  StateAttribute::OVERRIDE);
			ss->getOrCreateUniform("equireTangularMap", Uniform::SAMPLER_2D)->set(0);
			//ss->getOrCreateUniform("color", Uniform::FLOAT_VEC3)->set(Vec3(0, 1, 0));
			_camera = cam;
		}
		{
			auto cam = new Camera;
			cam->setRenderOrder(cam->PRE_RENDER);
			cam->setReferenceFrame(cam->ABSOLUTE_RF);
			cam->setComputeNearFarMode(cam->DO_NOT_COMPUTE_NEAR_FAR);
			cam->setViewport(0, 0, 32, 32);
			cam->setRenderTargetImplementation(cam->FRAME_BUFFER_OBJECT);
			cam->setProjectionMatrix(Matrix::perspective(90.0f, 1.0f, 0.1f, metric * 2));
			cam->addChild(geometry);
			cam->setName("pre light");
			cam->setClearMask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
			auto ss = cam->getOrCreateStateSet();
			osg::Program* program = new Program;
			program->addShader(new Shader(Shader::VERTEX, intVertSource));
			program->addShader(new Shader(Shader::FRAGMENT, irrFragSource));
			ss->setAttributeAndModes(program, StateAttribute::OVERRIDE);
			ss->setTextureAttribute(0, _tex,  StateAttribute::OVERRIDE);
			ss->getOrCreateUniform("environmentMap", Uniform::SAMPLER_2D)->set(0);
			//ss->getOrCreateUniform("color", Uniform::FLOAT_VEC3)->set(Vec3(0, 1, 0));
			_cameraIrr = cam;
		}
	}
}

static int idx = 0;

void TestNode::traverse(NodeVisitor & nv)
{
	if (nv.getVisitorType() == NodeVisitor::CULL_VISITOR)
	{
		auto ss = getOrCreateStateSet();
		auto vp = nv.asCullVisitor()->getViewPoint();
		ss->getOrCreateUniform("camPos", osg::Uniform::FLOAT_VEC3)->set(vp);

		static Matrix captureViews[] =
		{
			Matrix::lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
			Matrix::lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(-1.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)),
			Matrix::lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
			Matrix::lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
			Matrix::lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f), Vec3(0.0f, -1.0f, 0.0f)),
			Matrix::lookAt(Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, -1.0f, 0.0f)),
		};

		if (idx < 6)
		{
			_camera->attach(_camera->COLOR_BUFFER, _tex, 0, idx);
			_camera->setViewMatrix(captureViews[idx]);
			_camera->accept(nv);
			_camera->dirtyAttachmentMap();
			idx++;
		}
		else if (idx < 12) {
			_cameraIrr->attach(_cameraIrr->COLOR_BUFFER, _texIrr, 0, idx - 6);
			_cameraIrr->setViewMatrix(captureViews[idx - 6]);
			_cameraIrr->accept(nv);
			_cameraIrr->dirtyAttachmentMap();
			idx++;
		}
	}
	else if (nv.getVisitorType() == NodeVisitor::UPDATE_VISITOR)
	{
	}
	Group::traverse(nv);
}

void TestNode::drawImplementation(RenderInfo &renderInfo) const
{

}
