#pragma once

#include <osg/Drawable>
#include <osg/Geometry>
#include <osg/BufferIndexBinding>


using namespace osg;

class TestNode : public Group {
public:
	TestNode();

	void traverse(NodeVisitor&);

	//void drawImplementation(RenderInfo& /*renderInfo*/) const;
	//BoundingSphere computeBound() const;
private:
	void setNoise(int);


	int _noiseType;
};