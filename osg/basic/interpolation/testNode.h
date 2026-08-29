#pragma once

#include <osg/Drawable>
#include <osg/Geometry>


using namespace osg;

class TestNode : public Group {
public:
	TestNode();

	void traverse(NodeVisitor &);
	void drawImplementation(RenderInfo& /*renderInfo*/) const;

};