#include "BlendNode1.h"
#include <osg/Geometry>
#include <osg/BlendFunc>
#include <osg/BlendEquation>
#include <osg/Point>
#include <osgUtil/CullVisitor>

#include <fstream>
#include <filesystem>

const osg::Vec3 vertices[] = {
  {-1, -1, -1}, {-1, 1, -1},  {1, 1, -1},  {1, -1, -1}, 
  {-1, -1, 1}, {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},

  {-1, -1, -1}, {1, -1, -1},  {1, -1, 1},  {-1, -1, 1}, 
  {1, 1, -1},  {-1, 1, -1}, {-1, 1, 1}, {1, 1, 1},

  {-1, 1, -1},  {-1, -1, -1}, {-1, -1, 1}, {-1, 1, 1},
  {1, -1, -1}, {1, 1, -1},  {1, 1, 1},  {1, -1, 1}
};

const osg::Vec4 colors[] = {
  {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
  {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
  {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1},
};

osg::ref_ptr<osg::Geometry> create_cube() 
{
  osg::ref_ptr<osg::Geometry> geo = new osg::Geometry;
  geo->setVertexArray(new osg::Vec3Array(vertices, vertices + 24));

  auto clr_arr = new osg::Vec4Array(colors, colors + 24);
  geo->setColorArray(clr_arr, osg::Array::BIND_PER_VERTEX);

  geo->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 24));

  return geo;
}


BlendNode1::BlendNode1()
{
  auto cube1 = create_cube();
  addChild(cube1);

  auto &clr = *static_cast<osg::Vec4Array*>(cube1->getColorArray());
  clr[8].set(1, 0, 0, 0.5); clr[9].set(1, 0, 0, 0.5); clr[10].set(1, 0, 0, 0.5); clr[11].set(1, 0, 0, 0.5);
  clr[12].set(0, 1, 0, 0.5); clr[13].set(0, 1, 0, 0.5); clr[14].set(0, 1, 0, 0.5); clr[15].set(0, 1, 0, 0.5);

  auto ss = getOrCreateStateSet();

  {
    auto b = new osg::BlendFunc;
    ss->setAttributeAndModes(b);
  }
  //{
  //  auto e = new osg::BlendEquation;
  //  e->setEquation(osg::BlendEquation::Equation(0x9294));
  //  ss->setAttributeAndModes(e);
  //}

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

BlendNode1::~BlendNode1()
{
}

void BlendNode1::traverse(osg::NodeVisitor &nv)
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
