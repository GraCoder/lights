#include "testNode.h"

#include <osg/Vec2>
#include <osg/Geometry>
#include <osg/Depth>
#include <osgUtil/CullVisitor>

#include <Windows.h>

#include "inc/common.h"

const std::string vertShader = R"(

#version 330 compatibility

out vec2 uv;

void main()
{
	gl_Position = gl_Vertex;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	uv = gl_MultiTexCoord0.xy;
}

)";


const osg::Vec2 haltonSequence[] = {{0.5f, 1.0f / 3},   {0.25f, 2.0f / 3},  {0.75f, 1.0f / 9},  {0.125f, 4.0f / 9},
                                    {0.625f, 7.0f / 9}, {0.375f, 2.0f / 9}, {0.875f, 5.0f / 9}, {0.0625f, 8.0f / 9}};


TestNode::TestNode()
{
  setCullingActive(false);
  init();
}

void TestNode::init()
{
  _quad = createTexturedQuadGeometry(Vec3(-0.5, -0.5, 0), Vec3(0.5, 0, 0), Vec3(0, 0.5, 0));
  auto clr = new Vec3Array;
  clr->push_back(Vec3(1, 0, 0));
  _quad->setColorArray(clr, Array::BIND_OVERALL);
  auto ss = _quad->getOrCreateStateSet();

  {
    std::string rsDir = RS_DIR;
    std::string vShader = readFile(rsDir + "/glsl/vel_vert.glsl");
    std::string fShader = readFile(rsDir + "/glsl/vel_frag.glsl");

    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(Shader::VERTEX, vShader));
    prg->addShader(new osg::Shader(Shader::FRAGMENT, fShader));
    ss->setAttribute(prg);
  }
  _cameraBuffer = new osg::MatrixfArray(2);
  _cameraBufferBinding = new osg::UniformBufferBinding(0, _cameraBuffer, 0, sizeof(Matrixf) * 2);
  ss->setAttribute(_cameraBufferBinding);

  _cam = new Camera;
  _cam->setReferenceFrame(Transform::RELATIVE_RF);
  _cam->addChild(_quad);
  _cam->setRenderOrder(Camera::PRE_RENDER);

  _clrTex = new osg::Texture2D;
  _clrTex->setInternalFormat(GL_RGBA);
  _clrTex->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
  _clrTex->setFilter(Texture::MAG_FILTER, Texture::LINEAR);

  _depTex = new osg::Texture2D;
  _depTex->setInternalFormat(GL_DEPTH_COMPONENT);
  _depTex->setFilter(Texture::MIN_FILTER, Texture::NEAREST);
  _depTex->setFilter(Texture::MAG_FILTER, Texture::NEAREST);

  _velTex = new osg::Texture2D;
  _velTex->setInternalFormat(GL_RG16F);
  _velTex->setSourceFormat(GL_RG);
  _velTex->setSourceType(GL_FLOAT);
  _velTex->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
  _velTex->setFilter(Texture::MAG_FILTER, Texture::LINEAR);

  _cam->attach(Camera::COLOR_BUFFER0, _clrTex);
  _cam->attach(Camera::DEPTH_BUFFER, _depTex);
  _cam->attach(Camera::COLOR_BUFFER1, _velTex);
  _cam->setRenderTargetImplementation(_cam->FRAME_BUFFER_OBJECT);

  _taaTex1 = new osg::Texture2D;
  _taaTex1->setInternalFormat(GL_RGBA);
  _taaTex1->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
  _taaTex1->setFilter(Texture::MAG_FILTER, Texture::LINEAR);

  _taaTex2 = new osg::Texture2D;
  _taaTex2->setInternalFormat(GL_RGBA);
  _taaTex2->setFilter(Texture::MIN_FILTER, Texture::LINEAR);
  _taaTex2->setFilter(Texture::MAG_FILTER, Texture::LINEAR);

  _taaquad = createTexturedQuadGeometry(Vec3(-1, -1, 0), Vec3(2, 0, 0), Vec3(0, 2, 0));
  _taaquad->setCullingActive(false);
  ss = _taaquad->getOrCreateStateSet();
  ss->setTextureAttributeAndModes(0, _clrTex);
  ss->setTextureAttributeAndModes(1, _taaTex1);
  ss->setTextureAttributeAndModes(2, _taaTex2);
  ss->setTextureAttributeAndModes(3, _depTex);
  ss->setTextureAttributeAndModes(4, _velTex);
  ss->getOrCreateUniform("cur_tex", osg::Uniform::SAMPLER_2D)->set(0);
  ss->getOrCreateUniform("dep_tex", osg::Uniform::SAMPLER_2D)->set(3);
  ss->getOrCreateUniform("vel_tex", osg::Uniform::SAMPLER_2D)->set(4);
  {
    std::string rsDir = RS_DIR;
    std::string taaShader = readFile(rsDir + "/glsl/taa_frag.glsl");

    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(Shader::VERTEX, vertShader));
    prg->addShader(new osg::Shader(Shader::FRAGMENT, taaShader));
    ss->setAttribute(prg);
  }

  _taaCam = new Camera;
  _taaCam->setClearMask(GL_COLOR_BUFFER_BIT);
  _taaCam->setImplicitBufferAttachmentMask(0);
  _taaCam->setReferenceFrame(Transform::ABSOLUTE_RF);
  _taaCam->setRenderOrder(Camera::PRE_RENDER, 1);
  _taaCam->setRenderTargetImplementation(_taaCam->FRAME_BUFFER_OBJECT);
  _taaCam->addChild(_taaquad);

  _ssquad = createTexturedQuadGeometry(Vec3(-1, -1, 0), Vec3(2, 0, 0), Vec3(0, 2, 0));
  //_ssquad->setCullingActive(false);
  ss = _ssquad->getOrCreateStateSet();
  ss->setTextureAttributeAndModes(0, _depTex);
  ss->setTextureAttributeAndModes(1, _taaTex1);
  ss->setTextureAttributeAndModes(2, _taaTex2);
  ss->setTextureAttributeAndModes(3, _velTex);
  ss->getOrCreateUniform("dep_tex", osg::Uniform::SAMPLER_2D)->set(0);
  ss->getOrCreateUniform("vel_tex", osg::Uniform::SAMPLER_2D)->set(3);
  {
    std::string rsDir = RS_DIR;
    std::string outShader = readFile(rsDir + "/glsl/out_frag.glsl");
    auto prg = new osg::Program;
    prg->addShader(new osg::Shader(Shader::VERTEX, vertShader));
    prg->addShader(new osg::Shader(Shader::FRAGMENT, outShader));
    ss->setAttribute(prg);
  }

  addChild(_ssquad);
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

      _velTex->setTextureSize(vp->width(), vp->height());
      _velTex->dirtyTextureObject();

      _taaTex1->setTextureSize(vp->width(), vp->height());
      _taaTex1->dirtyTextureObject();
      _taaTex2->setTextureSize(vp->width(), vp->height());
      _taaTex2->dirtyTextureObject();

      _cam->setViewport(0, 0, vp->width(), vp->height());
      _cam->dirtyAttachmentMap();

      _ssquad->getStateSet()->getOrCreateUniform("texture_size", osg::Uniform::FLOAT_VEC4)->set(osg::Vec4(1.0 / vp->width(), 1.0 / vp->height(), vp->width(), vp->height()));
    }

    int frameNum = cv->getFrameStamp()->getFrameNumber();
    int idx = frameNum % 8;
    auto& halt = haltonSequence[idx];
    osg::Vec2f jit((halt.x() - 0.5) / vp->width(), (halt.y() - 0.5) / vp->height());
    auto ss = _quad->getOrCreateStateSet();
    osg::Matrix proj = *cv->getProjectionMatrix();
    proj(2, 0) += jit.x() * 2;
    proj(2, 1) += jit.y() * 2;
    ss->getOrCreateUniform("taa_proj", osg::Uniform::FLOAT_MAT4)->set(proj);

    _preMatrix = _curMatrix;
    _curMatrix = *cv->getProjectionMatrix();
    _curMatrix.preMult(*cv->getModelViewMatrix());
    _cameraBuffer->at(0) = _preMatrix;
    _cameraBuffer->at(1) = _curMatrix;
    _cameraBuffer->dirty();

    _cam->accept(*cv);

    ss = _taaquad->getOrCreateStateSet();

    bool init = _preMatrix != _curMatrix;
    ss->getOrCreateUniform("init_clr", osg::Uniform::BOOL)->set(init);
    ss->getOrCreateUniform("taa_jitter", osg::Uniform::FLOAT_VEC2)->set(jit);
    if (frameNum % 2) {
      _taaCam->attach(Camera::COLOR_BUFFER, _taaTex1);
      ss->getOrCreateUniform("pre_tex", osg::Uniform::SAMPLER_2D)->set(2);
      ss = _ssquad->getStateSet();
      ss->getOrCreateUniform("clr_tex", osg::Uniform::SAMPLER_2D)->set(1);
    } else {
      _taaCam->attach(Camera::COLOR_BUFFER, _taaTex2);
      ss->getOrCreateUniform("pre_tex", osg::Uniform::SAMPLER_2D)->set(1);
      ss = _ssquad->getStateSet();
      ss->getOrCreateUniform("clr_tex", osg::Uniform::SAMPLER_2D)->set(2);
    }

    _taaCam->dirtyAttachmentMap();

    _taaCam->accept(*cv);

    // Sleep(100);
  }
  osg::Group::traverse(nv);
}
