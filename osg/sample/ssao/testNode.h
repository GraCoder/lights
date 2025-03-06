#pragma once

#include <osg/Drawable>
#include <osg/Geometry>
#include <osg/BufferIndexBinding>


using namespace osg;

namespace osg {
class Texture2D;
}

class TestNode : public Group {
public:
	TestNode();

	void traverse(NodeVisitor&);

	//void drawImplementation(RenderInfo& /*renderInfo*/) const;
	//BoundingSphere computeBound() const;
	osg::Texture2D* createTexture(int, int);
private:
	osg::ref_ptr<osg::Camera> _cam, _ssaoCam, _blurCam;

	osg::ref_ptr<osg::Geometry> _quad;

	osg::ref_ptr<osg::Texture2D> _colorTex, _normalTex, _posTex;

	osg::ref_ptr<osg::Texture2D> _ssaoTex, _blurTex;

	osg::ref_ptr<osg::Group> _node;
};