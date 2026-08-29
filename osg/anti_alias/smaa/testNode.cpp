#include "testNode.h"

#include <format>
//#include <boost/filesystem.hpp>
#include <filesystem>
#include <fstream>

#include <osg/Geometry>
#include <osgDB/WriteFile>
#include <osgUtil/CullVisitor>

#include <osgViewer/imgui/imgui.h>

#include "AreaTex.h"
#include "inc/common.h"

const std::string vertShader = R"(

#version 330 compatibility

out vec4 clr;

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	clr = gl_Color;
}

)";

const std::string fragShader = R"(

#version 330 compatibility

in vec4 clr;

void main()
{
	vec4 clr_tmp = clr;
	clr_tmp.w = 0.213 * clr.r + 0.715 * clr.g + 0.072 * clr.b;
	gl_FragColor = clr_tmp;
}

)";

const std::string svShader = R"(

#version 330 compatibility

out vec4 clr;

void main()
{
	gl_Position = gl_Vertex;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	clr = gl_Color;
}

)";

TestNode::TestNode()
{
  setNumChildrenRequiringUpdateTraversal(1);
  setCullingActive(false);
  init();
}

void TestNode::init()
{
  //_quad = createTexturedQuadGeometry(Vec3(0, -0.5, 0), Vec3(0.5, 0.5, 0), Vec3(-0.5, 0.5, 0));
  _quad = createTexturedQuadGeometry(Vec3(-0.5, -0.5, 0), Vec3(1, 0, 0), Vec3(0, 1, 0));
  auto clr = new Vec3Array;
  clr->push_back(Vec3(1, 0, 0));
  // clr->push_back(Vec3(0, 1, 0));
  // clr->push_back(Vec3(0, 0, 1));
  // clr->push_back(Vec3(1, 1, 0));
  _quad->setColorArray(clr, Array::BIND_OVERALL);
  auto ss = _quad->getOrCreateStateSet();
  auto prg = new osg::Program;
  prg->addShader(new osg::Shader(Shader::VERTEX, vertShader));
  prg->addShader(new osg::Shader(Shader::FRAGMENT, fragShader));
  ss->setAttribute(prg);
  _quad->setCullingActive(false);

  _cam = new Camera;
  _cam->addChild(_quad);
  _cam->setRenderOrder(Camera::PRE_RENDER);
  _cam->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  _cam->setClearColor(osg::Vec4(0, 0, 0, 0));
  _clrTex = new osg::Texture2D;
  _clrTex->setInternalFormat(GL_RGBA);
  _clrTex->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
  _clrTex->setFilter(Texture::MAG_FILTER, Texture::LINEAR);
  _depTex = new osg::Texture2D;
  _depTex->setInternalFormat(GL_DEPTH_COMPONENT);
  _depTex->setFilter(Texture::MIN_FILTER, Texture::NEAREST);
  _depTex->setFilter(Texture::MAG_FILTER, Texture::NEAREST);
  _cam->attach(Camera::COLOR_BUFFER, _clrTex);
  _cam->attach(Camera::DEPTH_BUFFER, _depTex);
  _cam->setRenderTargetImplementation(_cam->FRAME_BUFFER_OBJECT);

  _ssquad = createTexturedQuadGeometry(Vec3(-1, -1, 0), Vec3(2, 0, 0), Vec3(0, 2, 0));

  _edgeTex = new osg::Texture2D;
  _edgeTex->setInternalFormat(GL_RG);
  _edgeTex->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
  _edgeTex->setFilter(Texture::MAG_FILTER, Texture::LINEAR);
  _edgePass = new Camera;
  _edgePass->addChild(_ssquad);
  _edgePass->setRenderOrder(_edgePass->PRE_RENDER, 1);
  _edgePass->setRenderTargetImplementation(_edgePass->FRAME_BUFFER_OBJECT);
  _edgePass->setImplicitBufferAttachmentMask(0);
  _edgePass->attach(_edgePass->COLOR_BUFFER, _edgeTex);
  _edgePass->setClearMask(GL_COLOR_BUFFER_BIT);
  _edgePass->setClearColor(Vec4(0, 0, 0, 0));
  {
    auto ss = _edgePass->getOrCreateStateSet();
    ss->setTextureAttributeAndModes(0, _clrTex, osg::StateAttribute::OVERRIDE);
    ss->addUniform(new osg::Uniform("clr_tex", 0), osg::StateAttribute::OVERRIDE);

    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(Shader::VERTEX, svShader));
    prg->addShader(new osg::Shader(Shader::FRAGMENT, readFile(std::string(RS_DIR) + "/smaa_edge.frag")));
    ss->setAttribute(prg, osg::StateAttribute::OVERRIDE);
  }

  _blendTex = new osg::Texture2D;
  _blendTex->setInternalFormat(GL_RGBA8_SNORM);
  //_blendTex->setSourceFormat(GL_FLOAT);
  _blendTex->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
  _blendTex->setFilter(Texture::MAG_FILTER, Texture::LINEAR);
  _blendFactor = new Camera;
  _blendFactor->setClearMask(GL_COLOR_BUFFER_BIT);
  _blendFactor->setClearColor(Vec4(0, 0, 0, 0));
  _blendFactor->addChild(_ssquad);
  _blendFactor->setRenderOrder(_blendFactor->PRE_RENDER, 2);
  _blendFactor->setRenderTargetImplementation(_blendFactor->FRAME_BUFFER_OBJECT);
  _blendFactor->setImplicitBufferAttachmentMask(0);
  _blendFactor->attach(_blendFactor->COLOR_BUFFER, _blendTex);

  auto img = new osg::Image;
  img->setImage(AREATEX_WIDTH, AREATEX_HEIGHT, 0, GL_RG, GL_RG, GL_UNSIGNED_BYTE, (unsigned char*)areaTexBytes, osg::Image::NO_DELETE);
  img->flipVertical();
  _areaTex = new osg::Texture2D(img);
  _areaTex->setResizeNonPowerOfTwoHint(false);
  _areaTex->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
  _areaTex->setFilter(Texture::MAG_FILTER, Texture::LINEAR);

  {
    auto ss = _blendFactor->getOrCreateStateSet();
    ss->setTextureAttributeAndModes(0, _edgeTex, osg::StateAttribute::OVERRIDE);
    ss->addUniform(new osg::Uniform("edge_tex", 0), osg::StateAttribute::OVERRIDE);

    ss->setTextureAttributeAndModes(1, _areaTex, osg::StateAttribute::OVERRIDE);
    ss->addUniform(new osg::Uniform("area_tex", 1), osg::StateAttribute::OVERRIDE);

    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(Shader::VERTEX, svShader));

#if 0
    char bfShaderOut[70000] = {0};
    auto smaa = readFile(std::string(RS_DIR) + "/smaa.h");
    sprintf(bfShaderOut, bfShader, smaa.c_str());
    std::string bfsShader = bfShaderOut;
#else
    std::string bfsShader = readFile(std::string(RS_DIR) + "/smaa_weight.frag");
#endif

    // std::ofstream fout("test.frag", std::ios::out);
    // fout << smaa; fout.close();
    // osgDB::writeImageFile(*_areaTex->getImage(), "area.png");

    prg->addShader(new osg::Shader(Shader::FRAGMENT, bfsShader));
    ss->setAttribute(prg, osg::StateAttribute::OVERRIDE);
  }

  ss = _ssquad->getOrCreateStateSet();
  ss->setTextureAttributeAndModes(0, _clrTex);
  ss->setTextureAttributeAndModes(1, _depTex);
  ss->setTextureAttributeAndModes(2, _blendTex);
  ss->setTextureAttributeAndModes(3, _edgeTex);
  ss->getOrCreateUniform("clr_tex", Uniform::SAMPLER_2D)->set(0);
  ss->getOrCreateUniform("dep_tex", Uniform::SAMPLER_2D)->set(1);
  ss->getOrCreateUniform("blend_tex", Uniform::SAMPLER_2D)->set(2);
  ss->getOrCreateUniform("edge_tex", Uniform::SAMPLER_2D)->set(3);
  _ssquad->setCullingActive(false);
  {
    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(Shader::VERTEX, svShader));
    prg->addShader(new osg::Shader(Shader::FRAGMENT, readFile(std::string(RS_DIR) + "/smaa_blend.frag")));
    ss->setAttribute(prg);
  }
  addChild(_ssquad);

  test();
}

void TestNode::traverse(NodeVisitor& nv)
{
  auto cv = nv.asCullVisitor();

  if (cv) {
    auto vp = cv->getViewport();
    if (_clrTex->getTextureWidth() != vp->width() || _clrTex->getTextureHeight() != vp->height()) {
      _clrTex->setTextureSize(vp->width(), vp->height());
      _clrTex->dirtyTextureObject();
      _depTex->setTextureSize(vp->width(), vp->height());
      _depTex->dirtyTextureObject();
      _edgeTex->setTextureSize(vp->width(), vp->height());
      _edgeTex->dirtyTextureObject();
      _blendTex->setTextureSize(vp->width(), vp->height());
      _blendTex->dirtyTextureObject();

      _cam->dirtyAttachmentMap();
      _edgePass->dirtyAttachmentMap();
      _blendFactor->dirtyAttachmentMap();

      auto ss = _ssquad->getOrCreateStateSet();
      ss->getOrCreateUniform("texture_size", osg::Uniform::FLOAT_VEC4)->set(osg::Vec4(1.0 / vp->width(), 1.0 / vp->height(), vp->width(), vp->height()));
    }
    _cam->accept(*cv);
    _edgePass->accept(*cv);
    _blendFactor->accept(*cv);
  } else if (nv.asUpdateVisitor()) {
    static float xx = 0;
    ImGui::Begin("smaa");
    if (ImGui::Checkbox("diag weight", &_diag_weight)) {
      test();
    }
    ImGui::End();
  }
  osg::Group::traverse(nv);
}

void TestNode::test()
{
  auto ss = _ssquad->getOrCreateStateSet();
  constexpr char DIAG_DETE[] = "DIAG_DETECTION";
  if (_diag_weight)
    ss->setDefine(DIAG_DETE);
  else
    ss->removeDefine(DIAG_DETE);
}
