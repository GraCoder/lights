#include "testNode.h"
#include <osgUtil/CullVisitor>
#include <osg/MatrixTransform>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/Texture2D>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgGA/EventVisitor>
#include <osgGA/GUIEventAdapter>
#include <osgUtil/CullVisitor>

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

GLfloat lerp(GLfloat a, GLfloat b, GLfloat f)
{
	return a + f * (b - a);
}

TestNode::TestNode()
{
	setNumChildrenRequiringUpdateTraversal(1);
	setNumChildrenRequiringEventTraversal(1);

	//auto grp = new osg::Group;
	//grp->addChild(new osg::ShapeDrawable(new osg::Box({-25,-25, 6 }, 10, 10, 10)));
	//grp->addChild(new osg::ShapeDrawable(new osg::Box({ 25,-25, 6 }, 10, 10, 10)));
	//grp->addChild(new osg::ShapeDrawable(new osg::Box({ 25, 25, 6 }, 10, 10, 10)));
	//grp->addChild(new osg::ShapeDrawable(new osg::Box({-25, 25, 6 }, 10, 10, 10)));
	//grp->addChild(new osg::ShapeDrawable(new osg::Box({ 0, 0, 0 }, 100, 100, 2)));
	
	std::string modePath;
#if 0
	modePath = "D:\\tmp\\Vulkan\\data\\models\\sponza\\sponza.gltf";
#else
	auto path = std::filesystem::path(__FILE__).parent_path();
	modePath = path.string() + "/untitled.obj";
#endif
	auto grp = osgDB::readNodeFile(modePath);

	auto ss = grp->getOrCreateStateSet();
	auto pg = new osg::Program;
	pg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("geom.vert")));
	pg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("geom.frag")));
	ss->setAttribute(pg, osg::StateAttribute::OVERRIDE);

	int sz = 1024;

	_cam = new osg::Camera;
	_cam->setName("deferCam");
	_cam->addChild(grp);
	_colorTex = createTexture(sz, sz);
	_normalTex = createTexture(sz, sz);
	_posTex = createTexture(sz, sz);
	_cam->attach(_cam->COLOR_BUFFER0, _colorTex);
	_cam->attach(_cam->COLOR_BUFFER1, _normalTex);
	_cam->attach(_cam->COLOR_BUFFER2, _posTex);
	_cam->setReferenceFrame(_cam->RELATIVE_RF);
	_cam->setRenderOrder(_cam->PRE_RENDER);
	_cam->setRenderTargetImplementation(_cam->FRAME_BUFFER_OBJECT);

	_quad = osg::createTexturedQuadGeometry(osg::Vec3(-1, -1, 0), { 2, 0, 0 }, { 0, 2, 0 });
	{
		_ssaoCam = new osg::Camera;
		_ssaoCam->setName("ssaoCam");
		_ssaoCam->addChild(_cam);
		_ssaoCam->addChild(_quad);

		_ssaoTex = createTexture(sz, sz);
		_ssaoTex->setInternalFormat(GL_R16F);
		_ssaoCam->attach(_ssaoCam->COLOR_BUFFER, _ssaoTex);
		_ssaoCam->setReferenceFrame(_ssaoCam->RELATIVE_RF);
		_ssaoCam->setRenderOrder(_ssaoCam->PRE_RENDER);
		_ssaoCam->setRenderTargetImplementation(_cam->FRAME_BUFFER_OBJECT);

		pg = new osg::Program;
		pg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("ssao.vert")));
		pg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("ssao.frag")));
		auto ss = _ssaoCam->getOrCreateStateSet();
		ss->setAttribute(pg);
		ss->setTextureAttribute(10, _posTex);
		ss->addUniform(new osg::Uniform("tex0", 10));
		ss->setTextureAttribute(11, _normalTex);
		ss->addUniform(new osg::Uniform("tex1", 11));

		std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0);
		std::default_random_engine generator;
		std::vector<osg::Vec3> ssaoKernel;
		for (GLuint i = 0; i < 64; ++i) {
			osg::Vec3 sample(
				randomFloats(generator) * 2.0 - 1.0,
				randomFloats(generator) * 2.0 - 1.0,
				randomFloats(generator)
			);
			sample.normalize();
			sample *= randomFloats(generator);
			GLfloat scale = GLfloat(i) / 64.0;
			scale = lerp(0.1f, 1.0f, scale * scale);
			sample *= scale;
			ssaoKernel.push_back(sample);
		}

		auto ssaoNoise = new Vec3[16];
		for (GLuint i = 0; i < 16; i++) {
			osg::Vec3 noise(
				randomFloats(generator) * 2.0 - 1.0,
				randomFloats(generator) * 2.0 - 1.0,
				0.0f);
			ssaoNoise[i] = noise;
		}
		auto noiseTex = new osg::Texture2D;
		noiseTex->setTextureSize(4, 4);
		auto image = new osg::Image;
		image->setImage(4, 4, 1, GL_RGB16F, GL_RGB, GL_FLOAT, (unsigned char*)ssaoNoise, osg::Image::USE_NEW_DELETE);
		//osgDB::writeImageFile(*image, "noise.tif");
		noiseTex->setImage(image);
		noiseTex->setNumMipmapLevels(0);

		ss->setTextureAttribute(12, noiseTex);
		ss->addUniform(new osg::Uniform("tex2", 12));

		auto uniform = new osg::Vec3ArrayUniform("samples");
		uniform->setArray(ssaoKernel);
		ss->addUniform(uniform);
	}

	{
		_blurCam = new osg::Camera;
		_blurCam->setName("blur");
		_blurCam->addChild(_ssaoCam);
		_blurCam->addChild(_quad);

		_blurTex = createTexture(sz, sz);
		_blurTex->setInternalFormat(GL_R16F);
		_blurCam->attach(_blurCam->COLOR_BUFFER, _blurTex);
		_blurCam->setReferenceFrame(_blurCam->ABSOLUTE_RF);
		_blurCam->setRenderOrder(_blurCam->PRE_RENDER);
		_blurCam->setRenderTargetImplementation(_cam->FRAME_BUFFER_OBJECT);
		_blurCam->setViewport(0, 0, sz, sz);

		pg = new osg::Program;
		pg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("ssao.vert")));
		pg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("blur.frag")));
		auto ss = _blurCam->getOrCreateStateSet();
		ss->setAttribute(pg);
		ss->setTextureAttribute(13, _ssaoTex);
		ss->addUniform(new osg::Uniform("ssaoTex", 13));
	}

	addChild(_blurCam);
	//addChild(_ssaoCam);

	{
		_node = new osg::Group;
		_node->addChild(_quad);
		addChild(_node);
		auto ss = _node->getOrCreateStateSet();
		ss->setTextureAttribute(14, _posTex);
		ss->addUniform(new osg::Uniform("light_pos", 14));
		ss->setTextureAttribute(15, _normalTex);
		ss->addUniform(new osg::Uniform("light_norm", 15));
		ss->setTextureAttribute(16, _colorTex);
		ss->addUniform(new osg::Uniform("light_albedo", 16));
		ss->setTextureAttribute(17, _ssaoTex);
		ss->addUniform(new osg::Uniform("light_ssao", 17));

		pg = new osg::Program;
		pg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("ssao.vert")));
		pg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("light.frag")));
		ss->setAttribute(pg);
	}
}


void TestNode::traverse(NodeVisitor& nv)
{
	static bool ssao_on = true;
	if (nv.getVisitorType() == nv.CULL_VISITOR) {
		auto cv = nv.asCullVisitor();
		auto vp = cv->getViewport();
		int width = vp->width();
		int height = vp->height();

		auto ss = _ssaoCam->getOrCreateStateSet();
		ss->getOrCreateUniform("projection", osg::Uniform::FLOAT_MAT4)->set(
			osg::Matrixf(*cv->getProjectionMatrix()));

		_blurCam->setProjectionMatrix(*cv->getProjectionMatrix());
		_blurCam->setViewMatrix(*cv->getModelViewMatrix());
		//_blurCam->accept(nv);

		ss = _node->getOrCreateStateSet();
		ss->addUniform(new osg::Uniform("ssao_on", ssao_on));
	} else if (nv.getVisitorType() == nv.UPDATE_VISITOR) {
		ImGui::Begin("xxx");
    ImGui::SetWindowPos(ImVec2(0, 0));
    ImGui::SetWindowSize(ImVec2(400, 200));
		ImGui::Checkbox("ssao_on", &ssao_on);
		ImGui::End();
	} else if (nv.getVisitorType() == nv.EVENT_VISITOR) {
		auto ev = nv.asEventVisitor();
		auto evs = ev->getEvents();
		auto ea = evs.begin()->get()->asGUIEventAdapter();
		if (ea->getEventType() == ea->RESIZE) {
		}
	}

	Group::traverse(nv);
}

osg::Texture2D*
TestNode::createTexture(int w, int h)
{
	auto tex = new osg::Texture2D;
	tex->setTextureSize(w, h);
	tex->setInternalFormat(GL_RGB16F);
	tex->setFilter(tex->MIN_FILTER, tex->LINEAR);
	tex->setNumMipmapLevels(0);
	return tex;
}
