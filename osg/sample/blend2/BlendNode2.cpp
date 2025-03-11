#include "BlendNode2.h"
#include <osg/Geometry>
#include <osg/Depth>
#include <osg/BlendFunc>
#include <osg/BlendEquation>
#include <osg/Point>
#include <osgUtil/CullVisitor>

#include <fstream>
#include <filesystem>

osg::Vec3 clrs[] = {{120, 255, 50}, {255, 12, 32},   {210, 150, 60}, {255, 255, 0}, {0, 255, 0}, {0, 255, 255},
                    {30, 180, 255}, {100, 150, 255}, {199, 21, 133}, {50, 4, 224}

};

std::string ReadFile(const std::string &fileName)
{
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

BlendNode2::BlendNode2()
{
  _quads = new osg::Group;
  for (int i = 0; i < 6; i++) {
    auto geo = osg::createTexturedQuadGeometry(osg::Vec3(i * 0.2, i, i * 0.2), osg::Vec3(1, 0, 0), osg::Vec3(0, 0, 1));
    auto clr = new osg::Vec4Array();
    clr->push_back(osg::Vec4(clrs[i]/255.0, 0.5));
    geo->setColorArray(clr, osg::Array::BIND_OVERALL);

    geo->setName(std::to_string(i));
    _quads->addChild(geo);
  }

  //method_normal();
  //method_meshkin();
  method_bavoil_myers();
}

BlendNode2::~BlendNode2() {}

void BlendNode2::traverse(osg::NodeVisitor &nv)
{
  Base::traverse(nv);

  if (nv.getVisitorType() == osg::NodeVisitor::CULL_VISITOR) {
    auto cv = static_cast<osgUtil::CullVisitor *>(&nv);
    auto vp = cv->getViewport();

    if (_width != vp->width() || _height != vp->height()) {
      if (_texture1)
        _texture1->setTextureSize(vp->width(), vp->height());
      _width = vp->width();
      _height = vp->height();
    }

    auto ss = getOrCreateStateSet();
    ss->getOrCreateUniform("blend_factor", osg::Uniform::FLOAT_VEC4)->set(osg::Vec4(vp->width(), vp->height(), 0, 0));

    _prjMatrix = *cv->getProjectionMatrix();
    _viewMatrix = *cv->getModelViewMatrix();

    if(_camera)
      _camera->setViewport(0, 0, _width, _height);
  }
}

class Callback : public osg::Camera::DrawCallback {
  osg::Matrix &_prjMatrix;
  osg::Matrix &_viewMatrix;

public:
  Callback(osg::Matrix &m1, osg::Matrix &m2) 
    : _viewMatrix(m1)
    , _prjMatrix(m2)
  {
  }

  void operator()(osg::RenderInfo &renderInfo) const 
  { 
    renderInfo.getState();
    auto cam = renderInfo.getCurrentCamera();
    cam->setViewMatrix(_viewMatrix);
    cam->setProjectionMatrix(_prjMatrix);
  }
};

void BlendNode2::method_normal() 
{
  addChild(_quads);

  auto ss = getOrCreateStateSet();

  ss->setRenderBinDetails(0, "DepthSortedBin");

  {
    auto b = new osg::BlendFunc;
    ss->setAttributeAndModes(b);
  }
  {
    auto d = new osg::Depth;
    ss->setAttributeAndModes(d, 0);
  }

  auto prg = new osg::Program;
  prg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("blend.vert")));
  prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("blend.frag")));
  ss->setAttribute(prg);
}

void BlendNode2::method_meshkin()
{
  _camera = new osg::Camera;
  _camera->setName("PRE");
  _camera->setClearColor(osg::Vec4(0, 0, 0, 0));
  _camera->setRenderOrder(osg::Camera::PRE_RENDER);
  _camera->setReferenceFrame(osg::Camera::RELATIVE_RF);
  _camera->addChild(_quads);
  _camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);

  _texture1 = new osg::Texture2D;
  _texture1->setInternalFormat(GL_RGBA);

  _camera->attach(osg::Camera::COLOR_BUFFER, _texture1);
  addChild(_camera);

  //_camera->addInitialDrawCallback(new Callback(_viewMatrix, _prjMatrix));

  {
    auto ss = _camera->getOrCreateStateSet(); 

    auto b = new osg::BlendFunc;
    ss->setAttributeAndModes(b, 0);

    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("blend.vert")));
    prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("meshkin1.frag")));
    ss->setAttribute(prg);
  }

  {
    auto grp = new osg::Group;
    grp->addChild(_quads);
    insertChild(0, grp);

    auto ss = grp->getOrCreateStateSet();

    {
      auto b = new osg::BlendFunc(GL_ONE, GL_ONE);
      ss->setAttributeAndModes(b);
    }
    {
      auto d = new osg::Depth;
      ss->setAttributeAndModes(d, 0);
    }

    ss->setTextureAttributeAndModes(0, _texture1);
    ss->getOrCreateUniform("c0_tex", osg::Uniform::SAMPLER_2D)->set(0);

    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("blend.vert")));
    prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("meshkin2.frag")));
    ss->setAttribute(prg);
  }
}

void BlendNode2::method_bavoil_myers() 
{
  _camera = new osg::Camera;
  _camera->setClearColor(osg::Vec4(0, 0, 0, 0));
  _camera->setRenderOrder(osg::Camera::PRE_RENDER);
  _camera->setReferenceFrame(osg::Camera::RELATIVE_RF);
  _camera->addChild(_quads);
  _camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);

  _texture1 = new osg::Texture2D;
  _texture1->setInternalFormat(GL_RGBA16F);
  _texture2 = new osg::Texture2D;
  _texture2->setInternalFormat(GL_R16F);

  _camera->attach(osg::Camera::COLOR_BUFFER0, _texture1);
  _camera->attach(osg::Camera::COLOR_BUFFER1, _texture2);
  addChild(_camera);

  //_camera->addInitialDrawCallback(new Callback(_viewMatrix, _prjMatrix));

  {
    auto ss = _camera->getOrCreateStateSet();
    auto b = new osg::BlendFunc(GL_ONE, GL_ONE);
    ss->setAttributeAndModes(b);

    ss->setMode(GL_DEPTH_TEST, 0);

    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("blend.vert")));
    prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("bavoil1.frag")));
    ss->setAttribute(prg);
  }

  auto geo = osg::createTexturedQuadGeometry(osg::Vec3(-1, -1, 0), osg::Vec3(2, 0, 0), osg::Vec3(0, 2, 0));
  geo->setCullingActive(0);
  geo->setComputeBoundingBoxCallback(new osg::Drawable::ComputeBoundingBoxCallback);
  addChild(geo);
  {
    auto ss = geo->getOrCreateStateSet();

    {
      auto b = new osg::BlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
      ss->setAttributeAndModes(b);
    }

    ss->setTextureAttributeAndModes(0, _texture1);
    ss->getOrCreateUniform("c0_tex", osg::Uniform::SAMPLER_2D)->set(0);
    ss->setTextureAttributeAndModes(1, _texture2);
    ss->getOrCreateUniform("c1_tex", osg::Uniform::SAMPLER_2D)->set(1);

    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("bavoil2.vert")));
    prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("bavoil2.frag")));
    ss->setAttribute(prg);
  }
}
