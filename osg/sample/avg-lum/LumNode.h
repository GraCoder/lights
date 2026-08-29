#pragma once

#include <osg/Group>
#include <osg/Camera>
#include <osg/Texture2D>

class LumNode : public osg::Group {
  typedef osg::Group Base;

public:
  LumNode();
  ~LumNode();

  void updateGeometry();

  void traverse(osg::NodeVisitor &nv) override;

private:
  int _pointNum = 10000;
  float _lumin = 0;

  osg::ref_ptr<osg::Camera> _cam;
  osg::ref_ptr<osg::Texture2D> _texture;
};