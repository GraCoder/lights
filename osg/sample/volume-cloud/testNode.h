#pragma once

#include <osg/Drawable>
#include <osg/Geometry>
#include <osg/BufferIndexBinding>


using namespace osg;

class VCloudNode;


class TestNode : public Group {
public:
	TestNode();

	void traverse(NodeVisitor&);
private:
	osg::ref_ptr<osg::Camera>		_camera;

	osg::ref_ptr<VCloudNode>		_cloudNode;
};