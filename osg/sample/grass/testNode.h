#pragma once

#include <osg/Drawable>
#include <osg/Geometry>
#include <osg/BufferIndexBinding>


using namespace osg;

struct Blade {
	// Position and direction
	osg::Vec4 v0;
	// Bezier point and height
	osg::Vec4 v1;
	// Physical model guide and width
	osg::Vec4 v2;
	// Up vector and stiffness coefficient
	osg::Vec4 up;

	Blade(const osg::Vec4& iv0, const osg::Vec4& iv1, const osg::Vec4& iv2,
		const osg::Vec4& iup)
		: v0{ iv0 }, v1{ iv1 }, v2{ iv2 }, up{ iup }
	{
	}
};

struct BladesNum {
	std::uint32_t vertexCount = 0;
	std::uint32_t instanceCount = 1;
	std::uint32_t firstVertex = 0;
	std::uint32_t firstInstance = 0;
};


class TestNode : public Group {
public:
	TestNode();

	void traverse(NodeVisitor&);
	void drawImplementation(RenderInfo& /*renderInfo*/) const;

	BoundingSphere computeBound() const;

private:
	osg::ref_ptr<osg::Node> _cmpNode;

	osg::ref_ptr<osg::UniformBufferBinding> _ubo;
	osg::ref_ptr<osg::ShaderStorageBufferBinding> _nbo;

	float m_wind_mag, m_wind_length, m_wind_period;
};