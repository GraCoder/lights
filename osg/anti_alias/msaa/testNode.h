#pragma once

#include <osg/Group>
#include <osg/Texture2D>
#include <osg/Texture2DMultisample>
#include <osg/FrameBufferObject>

using namespace osg;

class TestNode : public osg::Group {
public:
  TestNode();

  void prepareFBO();

  void applyRenderInfo(RenderInfo &);

private:
  void init();

  void traverse(osg::NodeVisitor &) override;

private:
  ref_ptr<Geometry> _quad, _ssquad;

  // ref_ptr<Texture2D> _clrTex, _depTex;
  ref_ptr<Texture2DMultisample> _clrTex, _depTex;
  ref_ptr<Camera> _cam;

  ref_ptr<FrameBufferObject> _fbo;
};