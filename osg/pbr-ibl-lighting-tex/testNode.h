#pragma once

#include <osg/Drawable>
#include <osg/Geometry>


using namespace osg;

class TestNode : public Group {
public:
	TestNode();

	void traverse(NodeVisitor &);
	void drawImplementation(RenderInfo& /*renderInfo*/) const;

private:
	ref_ptr<Node> _node;

	ref_ptr<Camera> _camera, _cameraIrr;
	ref_ptr<Texture> _tex, _texIrr;

};