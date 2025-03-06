#include "testNode.h"
#include <osgUtil/CullVisitor>
#include <osg/MatrixTransform>
#include <osg/BufferTemplate>
#include <osg/BufferIndexBinding>
#include <osg/DispatchCompute>
#include <osg/PatchParameter>

#include <random>
#include <filesystem>
#include <iostream>
#include <fstream>

#include <osgViewer/Imgui/imgui.h>

const int grasssz = 512;

TestNode::TestNode()
{
	_noiseType = 0;
	setCullingActive(false);
	setNumChildrenRequiringUpdateTraversal(1);
	{
		auto vertexs = new osg::Vec3Array;
		vertexs->push_back({-1, -1, 0});
		vertexs->push_back({ 1, -1, 0});
		vertexs->push_back({ 1,  1, 0});
		vertexs->push_back({-1, -1, 0});
		vertexs->push_back({ 1,  1, 0});
		vertexs->push_back({-1,  1, 0});
		auto clrs = new osg::Vec3Array;
		clrs->push_back({ 0, 0.4, 0 });
		auto geom = new osg::Geometry;
		geom->setVertexArray(vertexs);
		geom->setColorArray(clrs, osg::Array::BIND_OVERALL);
		geom->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, 6));
		addChild(geom);
		geom->setCullingActive(false);
	}

	auto ss = getOrCreateStateSet();
	ss->getOrCreateUniform("repNum", osg::Uniform::FLOAT)->set(8.f);
	ss->getOrCreateUniform("time", osg::Uniform::FLOAT)->set(0.f);

	setNoise(0);
}


void TestNode::traverse(NodeVisitor& nv)
{
	if (nv.getVisitorType() == nv.CULL_VISITOR) {
		auto cv = nv.asCullStack();
		auto vp = cv->getViewport();
		int width = vp->width();
		int height = vp->height();
		auto ss = getOrCreateStateSet();
		ss->getOrCreateUniform("screenSize", osg::Uniform::INT_VEC2)->set(width, height);
		//ss->getOrCreateUniform("time", osg::Uniform::FLOAT)->set(1.f);
	} else if (nv.getVisitorType() == nv.UPDATE_VISITOR) {
		auto ss = getOrCreateStateSet();

		ImGui::Begin("xxx");

		static bool timec = true;
		ImGui::Checkbox("time", &timec);
		static float timev = 0;
		ImGui::SameLine();
		ImGui::SliderFloat("timeValue", &timev, 0, 1000);
		if (timec) {
			timev = nv.getFrameStamp()->getReferenceTime();
		}
		ss->getOrCreateUniform("time", osg::Uniform::FLOAT)->set(timev);

		const char* noiseType[] = {"simplex", "worley"};
		const char* noiseTypeValue = noiseType[_noiseType];
		if (ImGui::BeginCombo("noise_type", noiseTypeValue, 0)) {
			for (int n = 0; n < IM_ARRAYSIZE(noiseType); n++) {
				const bool is_selected = (_noiseType== n);
				if (ImGui::Selectable(noiseType[n], is_selected)) {
					_noiseType = n;
					setNoise(_noiseType);
				}

				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Separator();
		ImGui::NewLine();
		ImGui::NewLine();

		if (_noiseType == 0) {

			static osg::Vec2 offset;
			ImGui::SliderFloat("xoffset", &offset[0], 0, 100);
			ImGui::SliderFloat("yoffset", &offset[1], 0, 100);
			ss->getOrCreateUniform("offset", osg::Uniform::FLOAT_VEC2)->set(offset);

			static float repNum = 8;
			if (ImGui::SliderFloat("repNum", &repNum, 8, 64)) {
				auto ss = getOrCreateStateSet();
				ss->getOrCreateUniform("repNum", osg::Uniform::FLOAT)->set(repNum);
			}

			const char* items[] = { "simplex", "fbm", "fbm_rot", "fbm-abs",
				"fbm-abs-rot", "fbm-sin", "fbm-sin-rot"};
			static int item_current_idx = 0;
			const char* combo_preview_value = items[item_current_idx];  // Pass in the preview value visible before opening the combo (it could be anything)
			if (ImGui::BeginCombo("combo 1", combo_preview_value, 0)) {
				for (int n = 0; n < IM_ARRAYSIZE(items); n++) {
					const bool is_selected = (item_current_idx == n);
					if (ImGui::Selectable(items[n], is_selected)) {
						item_current_idx = n;
						auto ss = getOrCreateStateSet();
						ss->getOrCreateUniform("cate", osg::Uniform::INT)->set(item_current_idx);
					}

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		} else if (_noiseType == 1) {
			static osg::Vec3 uvw = {0, 0, 1};
			ImGui::SliderFloat("u", &uvw[0], -1, 1);
			ImGui::SliderFloat("vulkan", &uvw[1], 0, 1);
			ImGui::SliderFloat("w", &uvw[2], 0, 1);
			ss->getOrCreateUniform("uvw", osg::Uniform::FLOAT_VEC3)->set(uvw);

			static float repNum = 4;
			if (ImGui::SliderFloat("repNum", &repNum, 1, 64)) {
				auto ss = getOrCreateStateSet();
				ss->getOrCreateUniform("repNum", osg::Uniform::FLOAT)->set(repNum);
			}

			const char* items[] = { "2d-noise", "2d-circle", "2d-poly", "2d-fbm", "3d-circle", "3d-circle-fbm"};
			static int item_current_idx = 0;
			const char* combo_preview_value = items[item_current_idx];  // Pass in the preview value visible before opening the combo (it could be anything)
			if (ImGui::BeginCombo("combo 1", combo_preview_value, 0)) {
				for (int n = 0; n < IM_ARRAYSIZE(items); n++) {
					const bool is_selected = (item_current_idx == n);
					if (ImGui::Selectable(items[n], is_selected)) {
						item_current_idx = n;
						auto ss = getOrCreateStateSet();
						ss->getOrCreateUniform("cate", osg::Uniform::INT)->set(item_current_idx);
					}

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
		ImGui::End();
	}

	Group::traverse(nv);
}

void TestNode::setNoise(int idx)
{
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

	auto ss = getOrCreateStateSet();
	if (idx == 0) {
		auto prg = new osg::Program;
		prg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("noise.vert")));
		prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("simplex.frag")));
		prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("noise.glsl")));
		ss->setAttribute(prg);
	} else {
		auto prg = new osg::Program;
		prg->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("noise.vert")));
		prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("worley.frag")));
		prg->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("noise.glsl")));
		ss->setAttribute(prg);
	}
}
