#include "testNode.h"
#include "VCloudNode.h"
#include <osgUtil/CullVisitor>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/Texture2D>

#include <random>
#include <filesystem>
#include <iostream>
#include <fstream>

#include <osgViewer/Imgui/imgui.h>

static const char* defVertSource = R"(

#version 330 compatibility

out vec3 fragPos;
out vec3 normal;

void main(void)
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

	fragPos = gl_Vertex.xyz;
	normal = gl_Normal;
}

)";

static const char* defFragSource = R"(

#version 330 compatibility

in vec3 fragPos;
in vec3 normal;

uniform vec3 viewPos;

void main(void)
{
	const vec3 objectColor = vec3(0.5, 0.6, 0.7);
	const vec3 lightColor = vec3(1, 1, 1);
	const vec3 lightPos = vec3(100, 0, 100);
	float ambientStrentch = 0.1;
	vec3 ambient = ambientStrentch * lightColor;

	vec3 norm = normalize(normal);
	vec3 lightDir = normalize(lightPos - fragPos);
	float diff = max(dot(norm, lightDir), 0);
	vec3 diffuse = diff * lightColor;

	float specularStrength = 0.5;
	vec3 viewDir = normalize(viewPos - fragPos);
	vec3 hfDir = normalize(viewDir + lightDir);
	float spec = pow(max(dot(norm, hfDir), 0), 32);
	vec3 specular = specularStrength * spec * lightColor;

	vec3 result = (ambient + diffuse + specular) * objectColor;
	gl_FragColor = vec4(result, 1.0 );
}

)";

const char vertexSource[] = R"(

#version 330 compatibility

void main()
{
	gl_Position = gl_Vertex;
	gl_TexCoord[0] = gl_MultiTexCoord0;
}
)";

const char fragSource[] = R"(

#version 330 compatibility

uniform sampler2D clrTex;
uniform sampler2D depTex;


void main()
{
	vec2 uv = gl_TexCoord[0].st;
	gl_FragColor = texture(clrTex, uv);
	gl_FragDepth = texture(depTex, uv).r;
}
)";

TestNode::TestNode()
{
	setCullingActive(false);
	setNumChildrenRequiringUpdateTraversal(1);
	auto geo = osg::createTexturedQuadGeometry({ -1, -1, 0 }, { 2, 0, 0 }, { 0, 2, 0 });
	addChild(geo);

	auto ss = geo->getOrCreateStateSet();
	auto pg = new osg::Program;
	pg->addShader(new osg::Shader(osg::Shader::VERTEX, vertexSource));
	pg->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragSource));
	ss->setAttribute(pg);
	ss->getOrCreateUniform("clrTex", osg::Uniform::SAMPLER_2D)->set(0);
	ss->getOrCreateUniform("depTex", osg::Uniform::SAMPLER_2D)->set(1);

	auto clrTex = new osg::Texture2D;
	clrTex->setTextureSize(2048, 2048);
	clrTex->setInternalFormat(GL_RGBA);
	auto depTex = new osg::Texture2D;
	depTex->setTextureSize(2048, 2048);
	depTex->setInternalFormat(GL_DEPTH_COMPONENT);
	depTex->setFilter(osg::Texture2D::MIN_FILTER, depTex->LINEAR);
	depTex->setFilter(osg::Texture2D::MAG_FILTER, depTex->LINEAR);

	ss->setTextureAttribute(0, clrTex);
	ss->setTextureAttributeAndModes(1, depTex, osg::StateAttribute::OVERRIDE);

	{
		auto cam = new osg::Camera;
		//cam->setClearColor({1, 0, 0, 1});
		cam->setRenderTargetImplementation(cam->FRAME_BUFFER_OBJECT);
		cam->setRenderOrder(cam->PRE_RENDER);
		cam->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
		cam->setViewport(0, 0, 2048, 2048);
		cam->attach(osg::Camera::COLOR_BUFFER, clrTex);
		cam->attach(osg::Camera::DEPTH_BUFFER, depTex);
		auto pg = new osg::Program;
		pg->addShader(new osg::Shader(osg::Shader::VERTEX, defVertSource));
		pg->addShader(new osg::Shader(osg::Shader::FRAGMENT, defFragSource));
		cam->getOrCreateStateSet()->setAttribute(pg);
		_camera = cam;
	}
	
	auto grp = new osg::Group;
	grp->addChild(new osg::ShapeDrawable(new osg::Sphere({ -10, 0, 0 }, 5)));
	grp->addChild(new osg::ShapeDrawable(new osg::Box({ 10, 0, 0 }, 10)));
	_camera->addChild(grp);

	addChild(grp);

	_cloudNode = new VCloudNode;
	addChild(_cloudNode);
	_cloudNode->setDepthTexture(depTex);
}


void TestNode::traverse(NodeVisitor& nv)
{
	if (nv.getVisitorType() == nv.CULL_VISITOR) {
		auto cul = nv.asCullVisitor();	
		_camera->setViewMatrix(*cul->getModelViewMatrix());
		_camera->setProjectionMatrix(*cul->getProjectionMatrix());
		_camera->getOrCreateStateSet()->
			getOrCreateUniform("camPos", osg::Uniform::FLOAT_VEC3)->set(cul->getEyePoint());
		_camera->accept(nv);

		getChild(0)->accept(nv);
		_cloudNode->accept(nv);
	} else if (nv.getVisitorType() == nv.UPDATE_VISITOR) {
		Group::traverse(nv);
	} else
		Group::traverse(nv);
}
