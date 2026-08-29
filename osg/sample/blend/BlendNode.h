#pragma once

#include <osg/Group>

class BlendNode : public osg::Group {
  typedef osg::Group Base;
public:
  BlendNode();
  ~BlendNode();

  void traverse(osg::NodeVisitor &nv) override;

private:
  int _width = 0;
  int _height = 0;
};