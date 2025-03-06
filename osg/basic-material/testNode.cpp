#include "testNode.h"
#include <osgUtil/CullVisitor>
#include <osg/MatrixTransform>


static const char *vertSource = R"(

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

static const char *fragSource = R"(

#version 330 compatibility

in vec3 fragPos;
in vec3 normal;

uniform vec3 viewPos;
uniform vec3 objectColor;
uniform vec3 lightColor;
uniform vec3 lightPos;


void main(void)
{
	float ambientStrentch = 0.1;
	vec3 ambient = ambientStrentch * lightColor;

	vec3 norm = normalize(normal);
	vec3 lightDir = normalize(lightPos - fragPos);
	float diff = max(dot(norm, lightDir), 0);
	vec3 diffuse = diff * lightColor;

	float specularStrength = 0.5;
	vec3 viewDir = normalize(viewPos - fragPos);
	vec3 reflectDir = reflect(-lightDir, norm);
	float spec = pow(max(dot(viewDir, reflectDir), 0), 32);
	vec3 specular = specularStrength * spec * lightColor;

	vec3 result = (ambient + diffuse + specular) * objectColor;
	gl_FragColor = vec4(result, 1.0 );
}

)" ;

class CubeCullCallback : public DrawableCullCallback {
public:
	bool cull(osg::NodeVisitor* nv, osg::Drawable* drawable, osg::RenderInfo* renderInfo) const
	{
		auto geo = drawable->asGeometry();
		auto ss = geo->getOrCreateStateSet();
		auto xx = nv->getEyePoint();
		ss->getOrCreateUniform("viewPos", osg::Uniform::FLOAT_VEC3)->set(nv->getViewPoint());

		return DrawableCullCallback::cull(nv, drawable, renderInfo);
	}
};

TestNode::TestNode()
{	
	float vertices[] = {
		-0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f, 0.5f, -0.5f,
		0.5f, 0.5f, -0.5f,
		-0.5f, 0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,

		-0.5f, -0.5f, 0.5f,
		0.5f, -0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,
		-0.5f, 0.5f, 0.5f,
		-0.5f, -0.5f, 0.5f,

		-0.5f, 0.5f, 0.5f,
		-0.5f, 0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, 0.5f,
		-0.5f, 0.5f, 0.5f,

		0.5f, 0.5f, 0.5f,
		0.5f, 0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,

		-0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, 0.5f,
		0.5f, -0.5f, 0.5f,
		-0.5f, -0.5f, 0.5f,
		-0.5f, -0.5f, -0.5f,

		-0.5f, 0.5f, -0.5f,
		0.5f, 0.5f, -0.5f,
		0.5f, 0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,
		-0.5f, 0.5f, 0.5f,
		-0.5f, 0.5f, -0.5f,
	};

	float normals[] = {
		0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, -1.0f,

		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,

		-1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,

		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,

		0.0f, -1.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, -1.0f, 0.0f,

		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f
	};

	std::vector<osg::Vec3> tmp;
	tmp.resize(sizeof(vertices) / 12);
	memcpy(&tmp[0], vertices, sizeof(vertices));
	auto vets = new osg::Vec3Array;
	vets->assign(tmp.begin(), tmp.end());
	memcpy(&tmp[0], normals, sizeof(normals));
	auto nols = new osg::Vec3Array;
	nols->assign(tmp.begin(), tmp.end());

	auto cube1 = new osg::Geometry;
	cube1->setVertexArray(vets);
	cube1->setNormalArray(nols, osg::Array::BIND_PER_VERTEX);
	cube1->addPrimitiveSet(new DrawArrays(GL_TRIANGLES, 0, tmp.size()));
	cube1->setUseVertexBufferObjects(true);

	auto trans = new osg::MatrixTransform;
	trans->setMatrix(osg::Matrix::translate(5, 0, 0));
	auto cube2 = (osg::Geometry *)cube1->clone(osg::CopyOp::DEEP_COPY_ALL);

	auto ss = cube1->getOrCreateStateSet();
	osg::Program* program = new osg::Program;
	program->addShader(new osg::Shader(osg::Shader::VERTEX, vertSource));
	program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragSource));
	ss->setAttributeAndModes(program, osg::StateAttribute::ON);
	ss->getOrCreateUniform("objectColor", osg::Uniform::FLOAT_VEC3)->set(osg::Vec3(1.0, 0.5, 1));
	ss->getOrCreateUniform("lightColor", osg::Uniform::FLOAT_VEC3)->set(osg::Vec3(1.0, 1, 1));
	ss->getOrCreateUniform("lightPos", osg::Uniform::FLOAT_VEC3)->set(osg::Vec3(2, 0, 2.0));
	cube1->setCullCallback(new CubeCullCallback);
	addChild(cube1);

	ss = cube2->getOrCreateStateSet();
	//ss->getOrCreateUniform("objectColor", osg::Uniform::FLOAT_VEC3)->set(osg::Vec3(1.0, 1, 1));
	//ss->getOrCreateUniform("lightColor", osg::Uniform::FLOAT_VEC3)->set(osg::Vec3(1.0, 1, 1));
	trans->addChild(cube2);
	addChild(trans);
}

void TestNode::traverse(NodeVisitor & nv)
{
	Group::traverse(nv);
}

void TestNode::drawImplementation(RenderInfo &renderInfo) const
{
}
