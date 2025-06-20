#include "LumNode.h"
#include <osg/Geometry>
#include <osg/BlendFunc>
#include <osg/BlendEquation>
#include <osg/Point>
#include <osgUtil/CullVisitor>
#include <osgUtil/UpdateVisitor>
#include <osg/DispatchCompute>

#include <osgViewer/imgui/imgui.h>

#include <random>

#include <fstream>
#include <filesystem>

LumNode::LumNode()
{
  setNumChildrenRequiringUpdateTraversal(1);

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

  {
    _cam = new osg::Camera;
    addChild(_cam);

    _texture = new osg::Texture2D;
    _texture->setName("SSTex");
    _texture->setInternalFormat(GL_RGBA8);
    _texture->setSourceFormat(GL_RGBA);
    _texture->setSourceType(GL_UNSIGNED_BYTE);
    _texture->setFilter(_texture->MIN_FILTER, _texture->NEAREST);
    _texture->setFilter(_texture->MAG_FILTER, _texture->NEAREST);

    //auto depTex = new osg::Texture2D;
    //depTex->setInternalFormat(GL_DEPTH_COMPONENT32);
    //depTex->setSourceFormat(GL_DEPTH_COMPONENT);
    //depTex->setSourceType(GL_FLOAT);
    //depTex->setUseHardwareMipMapGeneration(false);
    //depTex->setNumMipmapLevels(0);

    _cam->setViewport(0, 0, 1, 1);
    _cam->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    _cam->setRenderOrder(osg::Camera::PRE_RENDER);
    _cam->attach(osg::Camera::COLOR_BUFFER, _texture);
    //_cam->attach(osg::Camera::DEPTH_BUFFER, depTex);

    auto ss = _cam->getOrCreateStateSet();
    ss->setMode(GL_PROGRAM_POINT_SIZE, 1);

    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("avglum.vert")));
    prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("avglum.frag")));
    ss->setAttribute(prg);
  }

  {
    auto ssrect = osg::createTexturedQuadGeometry({-1, -1, 0}, {2, 0, 0}, {0, 2, 0}); 
    ssrect->setName("ssRect");
    ssrect->setInitialBound(osg::BoundingBox(0, 0, 0, 100, 100, 100));
    ssrect->setCullingActive(false);
    auto ss = ssrect->getOrCreateStateSet();
    ss->setTextureAttributeAndModes(0, _texture);
    ss->getOrCreateUniform("clr_tex", osg::Uniform::SAMPLER_2D)->set(0);

    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("ssrect.vert")));
    prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("ssrect.frag")));
    ss->setAttribute(prg);

    addChild(ssrect);
  }


  updateGeometry();
}

LumNode::~LumNode()
{}

void LumNode::updateGeometry() 
{
  _cam->removeChildren(0, getNumChildren());

  auto geo = new osg::Geometry;
  geo->setName("Points");

  auto vertArray = new osg::Vec3Array;
  auto clrArray = new osg::Vec3Array;
  vertArray->reserve(_pointNum);
  clrArray->reserve(_pointNum);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> ver(0.f, 100.f);
  std::uniform_real_distribution<float> clr(0.f, 1.f);
  for(int i = 0; i < _pointNum; i++) {
    vertArray->push_back(osg::Vec3(ver(gen), ver(gen), ver(gen)));
    clrArray->push_back(osg::Vec3(clr(gen), clr(gen), clr(gen)));
  }

  geo->setVertexArray(vertArray);
  geo->setColorArray(clrArray, osg::Array::BIND_PER_VERTEX);

  geo->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, _pointNum));

  _cam->addChild(geo);
  //addChild(geo);
}

void LumNode::traverse(osg::NodeVisitor &nv)
{
  Base::traverse(nv);

  if (nv.getVisitorType() == osg::NodeVisitor::UPDATE_VISITOR) {
    auto uv = static_cast<osgUtil::UpdateVisitor *>(&nv);
    auto ctx = ImGui::GetCurrentContext();
    if (ctx) {
      ImGui::Begin("Info");
      if (ImGui::InputInt("Point Number", &_pointNum, ImGuiInputTextFlags_EnterReturnsTrue)) {
        updateGeometry();
      }
      ImGui::LabelText("Lumin", "%f", &_lumin);
      ImGui::End();
    }
  } else if (nv.getVisitorType() == osg::NodeVisitor::CULL_VISITOR) {
    auto cv = static_cast<osgUtil::CullVisitor *>(&nv);
    auto vp = cv->getViewport();
    auto vp1 = _cam->getViewport();
    if (vp->width() != vp1->width() || vp->height() != vp1->height())
      _cam->resize(vp->width(), vp->height());
  }
}
