#include "VCloudNode.h"
#include <osgUtil/CullVisitor>
#include <osg/MatrixTransform>
#include <osg/BufferTemplate>
#include <osg/BufferIndexBinding>
#include <osg/DispatchCompute>
#include <osg/PatchParameter>
#include <osg/ShapeDrawable>
#include <osg/Texture2D>
#include <osg/Texture3D>
#include <osg/BindImageTexture>

#include <osgDB/ReadFile>

#include <random>
#include <filesystem>
#include <iostream>
#include <fstream>

#include <osgViewer/Imgui/imgui.h>

auto ReadFile = [](const std::string& fileName) {
	std::string dir = __FILE__;
	auto path = std::filesystem::path(dir).parent_path();
	path.append(fileName);
	if (!std::filesystem::exists(path)) {
		path = std::filesystem::path(dir).parent_path();
		path = path.parent_path();
		path.append("glsl");
		path.append(fileName);
	}
	std::ifstream fs(path.string(), std::ios::in);
	std::istreambuf_iterator<char> iter(fs), end;
	std::string shaderSource(iter, end);
	return shaderSource;
};


VCloudNode::VCloudNode()
	: _needComputeNoise(true)
{
	_box = new osg::Box({ 0, 0, 0 }, 200, 200, 50);
	auto node = new osg::ShapeDrawable(_box);
	node->setColor({ 1, 0, 0, 1 });
	addChild(node);
	_boxDrawable = node;

	createNoise();
	createDiscribTex();

	auto ss = node->getOrCreateStateSet();
	ss->setMode(GL_BLEND, 1);
	auto pg = new osg::Program;
	pg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("cloud.vert")));
	pg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("cloud.frag")));
	ss->setAttribute(pg);
	ss->getOrCreateUniform("boxPos", osg::Uniform::FLOAT_VEC4)->set(Vec4(_box->getCenter(), 0));
	ss->getOrCreateUniform("boxLength", osg::Uniform::FLOAT_VEC4)->set(Vec4(_box->getHalfLengths(), 0));
	ss->getOrCreateUniform("noiseTex", osg::Uniform::SAMPLER_3D)->set(1);
	ss->setTextureAttribute(1, _noiseTex);

	{
		//auto node = new osg::ShapeDrawable(_box);
	}
}


void VCloudNode::traverse(NodeVisitor& nv)
{
	if (nv.getVisitorType() == nv.CULL_VISITOR) {

		if (_needComputeNoise) {
			_noise->accept(nv);
			_needComputeNoise = false;
		}

		auto cull = nv.asCullVisitor();
		Matrix matrix = *(cull->getProjectionMatrix());
		matrix.preMult(*cull->getModelViewMatrix());
		matrix = matrix.inverse(matrix);
		auto ss = _boxDrawable->getOrCreateStateSet();
		ss->getOrCreateUniform("invModelViewProjectMatrix", osg::Uniform::FLOAT_MAT4)->set(Matrixf(matrix));
		auto vp = cull->getViewport();
		ss->getOrCreateUniform("screenSize", osg::Uniform::FLOAT_VEC4)->set(osg::Vec4(vp->width(), vp->height(), 0, 0));

		const Vec3 offset = { 2, 2, 2 };
		auto center = _box->getCenter();
		auto minBox = center - _box->getHalfLengths() - offset;
		auto maxBox = center + _box->getHalfLengths() + offset;
		auto camPos = cull->getViewPointLocal();
		camPos < offset;
		if (camPos.x() < minBox.x() || camPos.x() > maxBox.x() ||
			camPos.y() < minBox.y() || camPos.y() > maxBox.y() ||
			camPos.z() < minBox.z() || camPos.z() > maxBox.z()) {
			ss->setMode(GL_DEPTH_TEST, 1);
			ss->setMode(GL_CULL_FACE, 1);
			ss->getOrCreateUniform("uCamPos", osg::Uniform::FLOAT_VEC4)->set(Vec4(camPos, 0));
		} else {
			ss->setMode(GL_DEPTH_TEST, 0);
			ss->setMode(GL_CULL_FACE, 0);
			ss->getOrCreateUniform("uCamPos", osg::Uniform::FLOAT_VEC4)->set(Vec4(camPos, 1));
		}

	} else if (nv.getVisitorType() == nv.UPDATE_VISITOR) {
	}

	Group::traverse(nv);
}

BoundingSphere VCloudNode::computeBound() const
{
	return BoundingSphere(_box->getCenter(), _box->getHalfLengths().length());
}

void VCloudNode::setDepthTexture(osg::Texture2D* tex)
{
	auto ss = _boxDrawable->getOrCreateStateSet();
	ss->setTextureAttribute(0, tex);
	ss->getOrCreateUniform("depTex", osg::Uniform::SAMPLER_2D)->set(0);
}

void VCloudNode::createNoise()
{
	_noiseTex = new osg::Texture3D;
	_noiseTex->setTextureSize(128, 128, 128);
	_noiseTex->setFilter(_noiseTex->MIN_FILTER, _noiseTex->LINEAR);
	_noiseTex->setFilter(_noiseTex->MAG_FILTER, _noiseTex->LINEAR);
	_noiseTex->setInternalFormat(GL_RGBA16F);
	_noiseTex->setSourceFormat(GL_RED);
	_noiseTex->setSourceType(GL_FLOAT);
	_noiseTex->setWrap(_noiseTex->WRAP_S, _noiseTex->MIRROR);
	_noiseTex->setWrap(_noiseTex->WRAP_T, _noiseTex->MIRROR);
	_noiseTex->setWrap(_noiseTex->WRAP_R, _noiseTex->MIRROR);

	auto bind = new osg::BindImageTexture(0, _noiseTex, osg::BindImageTexture::WRITE_ONLY, GL_RGBA16F);
	bind->setIsLayered(true);

	_noise = new osg::DispatchCompute(8, 8, 32);
	_noise->setDataVariance(STATIC);
	auto ss = _noise->getOrCreateStateSet();
	auto pg = new osg::Program;
	pg->addShader(new osg::Shader(osg::Shader::COMPUTE, ReadFile("noise.glsl")));
	pg->addShader(new osg::Shader(osg::Shader::COMPUTE, ReadFile("noise.comp")));
	ss->setAttributeAndModes(pg);
	//ss->addUniform(new osg::Uniform("noiseTex", int(0)));
	//ss->setTextureAttribute(0, _noiseTex);
	ss->setAttribute(bind);
}

void VCloudNode::createDiscribTex()
{
	std::string imagePath = __FILE__;
	imagePath = std::filesystem::path(imagePath).parent_path().string();
	imagePath += "//cloud_ds.png";
	_dsTex = new osg::Texture2D;
	auto img = osgDB::readImageFile(imagePath);
	_dsTex->setImage(img);
	_dsTex->setWrap(_dsTex->WRAP_S, _dsTex->MIRROR);
	_dsTex->setWrap(_dsTex->WRAP_T, _dsTex->MIRROR);
	_dsTex->setFilter(_dsTex->MIN_FILTER, _dsTex->NEAREST);
	_dsTex->setFilter(_dsTex->MAG_FILTER, _dsTex->NEAREST);
	auto ss = _boxDrawable->getOrCreateStateSet();
	ss->addUniform(new osg::Uniform("disTex", 2));
	ss->setTextureAttribute(2, _dsTex);
}
