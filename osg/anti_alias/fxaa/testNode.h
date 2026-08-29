#pragma once

#include <osg/Group>
#include <osg/Texture2D>

using namespace osg;

class TestNode : public osg::Group {
public:
	TestNode();

private:
	void init();

	void traverse(osg::NodeVisitor &) override;
private:
	
	ref_ptr<Geometry> _quad, _ssquad;

	ref_ptr<Texture2D> _clrTex, _depTex;
	ref_ptr<Camera> _cam;
};