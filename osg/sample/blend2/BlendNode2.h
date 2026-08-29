#pragma once

#include <osg/Group>
#include <osg/Camera>
#include <osg/Texture2D>

class BlendNode2 : public osg::Group {
  typedef osg::Group Base;

public:
  BlendNode2();
  ~BlendNode2();

  void traverse(osg::NodeVisitor &nv) override;

private:

  void method_normal();
  void method_meshkin();
  void method_bavoil_myers();
  void method_bavoil_myers_new();
  void method_bavoil_myers_new1();

private:
  int _width = 0;
  int _height = 0;

  osg::Matrix _viewMatrix, _prjMatrix;

  osg::ref_ptr<osg::Group> _quads;

  osg::ref_ptr<osg::Camera> _camera;
  osg::ref_ptr<osg::Texture2D> _texture1;
  osg::ref_ptr<osg::Texture2D> _texture2;
};