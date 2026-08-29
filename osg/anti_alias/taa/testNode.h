#pragma once

#include <osg/Group>
#include <osg/Texture2D>
#include <osg/BufferIndexBinding>

using namespace osg;

class TestNode : public osg::Group {
public:
	TestNode();

private:
	void init();

	void traverse(osg::NodeVisitor &) override;

private:
	
	ref_ptr<Geometry> _quad, _taaquad, _ssquad;

	ref_ptr<Texture2D> _clrTex, _depTex;
	ref_ptr<Texture2D> _velTex;
	ref_ptr<Texture2D> _taaTex1, _taaTex2;
	ref_ptr<Camera> _cam, _taaCam;

	osg::Matrix _preMatrix, _curMatrix;
	ref_ptr<osg::MatrixfArray> _cameraBuffer;
	ref_ptr<UniformBufferBinding> _cameraBufferBinding;
};