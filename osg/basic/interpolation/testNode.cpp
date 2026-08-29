#include "testNode.h"
#include <osgUtil/CullVisitor>
#include <osg/MatrixTransform>


static const char *vertSource = R"(

#version 330 compatibility

void main(void)
{
  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

)";

static const char *fragSource = R"(

#version 330 compatibility


void main(void)
{
	gl_FragColor = vec4(vec3(1, 0, 0), 1.0 );
}

)" ;

TestNode::TestNode()
{	
  {
    auto line = new osg::Geometry;
    addChild(line);
    auto v = new osg::Vec3Array;
    v->push_back(osg::Vec3(-10, 0, 0));
    v->push_back(osg::Vec3(-10, 10, 0));
    v->push_back(osg::Vec3(-5, 10, 0));
    v->push_back(osg::Vec3(-5, 0, 0));
    v->push_back(osg::Vec3(-8, 0, 0));
    line->setVertexArray(v);
    line->addPrimitiveSet(new osg::DrawArrays(GL_LINE_STRIP, 0, 5));
  }
}

void TestNode::traverse(NodeVisitor & nv)
{
	Group::traverse(nv);
}

void TestNode::drawImplementation(RenderInfo &renderInfo) const
{
}
