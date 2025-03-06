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

	void test();

private:
	
	ref_ptr<Geometry> _quad, _ssquad;

	ref_ptr<Texture2D> _clrTex, _depTex, _edgeTex, _blendTex;
	ref_ptr<Camera> _cam, _edgePass, _blendFactor;

	ref_ptr<Texture2D> _areaTex;

	bool _diag_weight = true;
};