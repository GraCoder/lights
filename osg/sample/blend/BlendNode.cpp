#include "BlendNode.h"
#include <osg/Geometry>
#include <osg/BlendFunc>
#include <osg/BlendEquation>
#include <osg/Point>
#include <osgUtil/CullVisitor>

#include <fstream>
#include <filesystem>

osg::Geometry *create_quad(const osg::Vec3 &pt, const osg::Vec4 &clr)
{
  osg::Geometry *geom = new osg::Geometry;
  geom->setUseVertexBufferObjects(true);

  osg::Vec3Array *vertices = new osg::Vec3Array;
  vertices->push_back(osg::Vec3(pt.x() - 5, pt.y(), pt.z() - 5));
  vertices->push_back(osg::Vec3(pt.x() + 5, pt.y(), pt.z() - 5));
  vertices->push_back(osg::Vec3(pt.x() + 5, pt.y(), pt.z() + 5));
  vertices->push_back(osg::Vec3(pt.x() - 5, pt.y(), pt.z() + 5));
  geom->setVertexArray(vertices);

  auto clrs = new osg::Vec4Array;
  clrs->push_back(clr);
  geom->setColorArray(clrs, osg::Array::BIND_OVERALL);

  auto pri = new osg::DrawElementsUShort(GL_TRIANGLES);
  pri->push_back(0); pri->push_back(1); pri->push_back(2);
  pri->push_back(0); pri->push_back(2); pri->push_back(3);
  geom->addPrimitiveSet(pri);

  return geom;
}

osg::Geometry *create_points(const osg::Vec3 &pt, const osg::Vec4 &clr)
{
  osg::Geometry *geom = new osg::Geometry;
  geom->setUseVertexBufferObjects(true);

  int count = 11;

  osg::Vec3Array *vertices = new osg::Vec3Array;
  for (int i = 0; i < count; i++) {
    vertices->push_back(osg::Vec3(pt.x() - (i - 5) * 8, pt.y(), pt.z()));
  }
  geom->setVertexArray(vertices);

  auto clrs = new osg::Vec4Array;
  clrs->push_back(clr);
  geom->setColorArray(clrs, osg::Array::BIND_OVERALL);

  auto pri = new osg::DrawArrays(GL_POINTS, 0, count);
  geom->addPrimitiveSet(pri);

  return geom;
}

BlendNode::BlendNode()
{
  addChild(create_points({0, 0.2, 4}, {1, 0, 0, 0.5}));
  addChild(create_points({0, 0, 0}, {0, 1, 0, 0.5}));
  addChild(create_points({0, -0.2, -4}, {0, 0, 1, 0.5}));
  addChild(create_points({0, -0.3, -6}, {0.5, 0.5, 0.5, 0.5}));

  auto ss = getOrCreateStateSet();

  {
    ss->setMode(GL_PROGRAM_POINT_SIZE, 1);
  }

  {
    auto b = new osg::BlendFunc;
    ss->setAttributeAndModes(b);
  }
  {
    auto e = new osg::BlendEquation;
    e->setEquation(osg::BlendEquation::Equation(0x9294));
    ss->setAttributeAndModes(e);
  }

	auto ReadFile = [](const std::string& fileName) {
		std::string dir = __FILE__;
		auto path = std::filesystem::path(dir).parent_path();
		path.append(fileName);
		if (!std::filesystem::exists(path)) {
			path = std::filesystem::path(dir).parent_path();
			path = path.parent_path();
			path.append("glsl");
			path.append(fileName);
		}
		std::ifstream fs(path.string(), std::ios::in);
		std::istreambuf_iterator<char> iter(fs), end;
		std::string shaderSource(iter, end);
		return shaderSource;
	};

  auto prg = new osg::Program;
  prg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("blend.vert")));
  prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("blend.frag")));
  ss->setAttribute(prg);
}

BlendNode::~BlendNode()
{
}

void BlendNode::traverse(osg::NodeVisitor &nv)
{
  Base::traverse(nv);

  if (nv.getVisitorType() == osg::NodeVisitor::CULL_VISITOR)
  {
    auto cv = static_cast<osgUtil::CullVisitor *>(&nv);
    auto vp = cv->getViewport();

    auto ss = getOrCreateStateSet();
    ss->getOrCreateUniform("blend_factor", osg::Uniform::FLOAT_VEC4)->set(osg::Vec4(vp->width(), vp->height(), 0, 0));
  }
}
