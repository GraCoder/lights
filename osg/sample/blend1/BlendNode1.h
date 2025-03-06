#pragma once

#include <osg/Group>

class BlendNode1 : public osg::Group {
  typedef osg::Group Base;
public:
  BlendNode1();
  ~BlendNode1();

  void traverse(osg::NodeVisitor &nv) override;

private:
  int _width = 0;
  int _height = 0;
};