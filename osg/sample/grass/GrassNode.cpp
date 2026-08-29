#include <osg/BufferIndexBinding>
#include <osg/BufferTemplate>
#include <osg/DispatchCompute>
#include <osg/MatrixTransform>
#include <osg/PatchParameter>
#include <osg/PrimitiveSetIndirect>
#include <osgUtil/CullVisitor>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

#include <osgViewer/Imgui/imgui.h>

#include "GrassNode.h"

#define GL_ARRAY_BUFFER 0x8892

const int grasssz = 1024;

class GrassGeometry : public osg::Drawable {
public:
  GrassGeometry()
  {
    setCullingActive(false);
    // setNodeMask(0);
    auto ss = getOrCreateStateSet();
    ss->setMode(GL_CULL_FACE, 0);
  }

  ~GrassGeometry() {}

  virtual BoundingBox computeBoundingBox() const override
  {
    osg::Vec3d pt(grasssz, grasssz, grasssz);
    return osg::BoundingBox(-pt, pt);
  }

  void setBufferObject(osg::ShaderStorageBufferObject *blade, osg::ShaderStorageBufferObject *draw) 
  { 
    _vbo = new osg::VertexBufferObject;
    _dbo = new osg::DrawIndirectBufferObject;

    _bladeVertex = blade;
    _bladeDraw = draw;
  }

  virtual VertexArrayState *createVertexArrayStateImplementation(RenderInfo &renderInfo) const override
  {
    State *state = renderInfo.getState();

    VertexArrayState *vas = new osg::VertexArrayState(state);

    vas->assignVertexAttribArrayDispatcher(4);
    vas->generateVertexArrayObject();

    auto ctxId = renderInfo.getContextID();
    _vbo->setGLBufferObject(ctxId, new osg::GLBufferObject(ctxId, _vbo, _bladeVertex->getGLBufferObject(ctxId)->getGLObjectID()));
    _dbo->setGLBufferObject(ctxId, new osg::GLBufferObject(ctxId, _dbo, _bladeDraw->getGLBufferObject(ctxId)->getGLObjectID()));

    return vas;
  }

  void drawImplementation(RenderInfo &renderInfo) const
  {
    auto state = renderInfo.getState();
    auto contextId = renderInfo.getContextID();
    auto ext = state->get<GLExtensions>();

    auto vas = state->getCurrentVertexArrayState();

    {
      auto glBuf = _vbo->getGLBufferObject(contextId);
      if (glBuf->isDirty()) {
        vas->bindVertexBufferObject(glBuf);
        vas->setVertexAttribArray(*state, 0, 4, GL_FLOAT, sizeof(Blade), reinterpret_cast<void *>(offsetof(Blade, v0)), GL_FALSE);
        vas->setVertexAttribArray(*state, 1, 4, GL_FLOAT, sizeof(Blade), reinterpret_cast<void *>(offsetof(Blade, v1)), GL_FALSE);
        vas->setVertexAttribArray(*state, 2, 4, GL_FLOAT, sizeof(Blade), reinterpret_cast<void *>(offsetof(Blade, v2)), GL_FALSE);
        vas->setVertexAttribArray(*state, 3, 4, GL_FLOAT, sizeof(Blade), reinterpret_cast<void *>(offsetof(Blade, up)), GL_FALSE);
      }
    }

    {
      auto glBuf = _dbo->getGLBufferObject(contextId);
      if(glBuf->isDirty())
        vas->bindVertexBufferObject(glBuf);
    }

    ext->glDrawArraysIndirect(GL_PATCHES, reinterpret_cast<void *>(0));
    // ext->glDrawArraysIndirect(GL_POINTS, reinterpret_cast<void*>(0));
  }

private:
  osg::ref_ptr<osg::VertexBufferObject> _vbo;
  osg::ref_ptr<osg::DrawIndirectBufferObject> _dbo;

  osg::ref_ptr<osg::ShaderStorageBufferObject> _bladeVertex;
  osg::ref_ptr<osg::ShaderStorageBufferObject> _bladeDraw;
};

class ComputeNode : public DispatchCompute {
public:
  ComputeNode(int x, int y, int z)
    :osg::DispatchCompute(x, y, z)
  {
    setName("COMP_NODE");
    setUseVertexArrayObject(true);
    setUseVertexBufferObjects(true);
  }


  virtual VertexArrayState *createVertexArrayStateImplementation(RenderInfo &renderInfo) const override
  {
    State *state = renderInfo.getState();

    VertexArrayState *vas = new osg::VertexArrayState(state);

    vas->assignVertexAttribArrayDispatcher(3);

    if (state->useVertexArrayObject(_useVertexArrayObject)) {
      vas->generateVertexArrayObject();
    } else {
    }
    return vas;
  }

  void drawImplementation(RenderInfo &renderInfo) const
  {
    auto state = renderInfo.getState();
    auto contextId = renderInfo.getContextID();
    auto ext = renderInfo.getState()->get<GLExtensions>();
    auto vas = state->getCurrentVertexArrayState();

    {
      auto glBuf = _ubo->getBufferObject()->getOrCreateGLBufferObject(contextId);
      if (glBuf->isDirty()) {
        glBuf->compileBuffer();
        ext->glBindBufferBase(_ubo->getBufferObject()->getTarget(), 0, glBuf->getGLObjectID());
      }
    }

    {
      auto glBuf = _bladeIn->getBufferObject()->getOrCreateGLBufferObject(contextId);
      if (glBuf->isDirty()) {
        glBuf->compileBuffer();
        ext->glBindBufferRange(_bladeIn->getBufferObject()->getTarget(), 1, glBuf->getGLObjectID(), 0, _bladeIn->getTotalDataSize());
      }
    }

    {
      auto glBuf = _bladeOut->getOrCreateGLBufferObject(contextId);
      if (glBuf->isDirty()) {
        glBuf->compileBuffer();
        ext->glBindBufferBase(_bladeOut->getTarget(), 2, glBuf->getGLObjectID());
      }
    }

    {
      auto glBuf = _nbo->getBufferObject()->getOrCreateGLBufferObject(contextId);
      if (glBuf->isDirty()) {
        glBuf->compileBuffer();
        ext->glBindBufferBase(_nbo->getBufferObject()->getTarget(), 3, glBuf->getGLObjectID());
      }
    }

    DispatchCompute::drawImplementation(renderInfo);
  }

  osg::ref_ptr<osg::MatrixfArray> _ubo;
  osg::ref_ptr<osg::BufferTemplate<std::vector<Blade>>> _bladeIn;
  osg::ref_ptr<osg::ShaderStorageBufferObject> _bladeOut;
  osg::ref_ptr<osg::BufferTemplate<BladesNum>> _nbo;
};

GrassNode::GrassNode()
{
  setCullingActive(false);
  setNumChildrenRequiringUpdateTraversal(1);

  m_wind_mag = 2;
  m_wind_length = 2;
  m_wind_period = 2;

  std::vector<Blade> blades;
  std::random_device device;
  std::mt19937 gen(device());
  std::uniform_real_distribution<float> orientation_dis(0, osg::PI);
  std::uniform_real_distribution<float> height_dis(0.6f, 1.2f);
  std::uniform_real_distribution<float> dis(-1, 1);

  blades.reserve(grasssz * grasssz);
  for (int i = -grasssz; i < grasssz; ++i) {
    for (int j = -grasssz; j < grasssz; ++j) {
      const auto x = (static_cast<float>(j) + dis(gen)) * 0.5f;
      const auto y = (static_cast<float>(i) + dis(gen)) * 0.5f;
      const auto blade_height = height_dis(gen);

      blades.emplace_back(osg::Vec4(x, y, 0, orientation_dis(gen)), osg::Vec4(x, y, blade_height / 2.0, blade_height), osg::Vec4(x, y, blade_height, 0.2f),
                          osg::Vec4(0, 0, 1, 0.7f + dis(gen) * 0.3f));
    }
  }

  auto cameraBuffer = new osg::MatrixfArray(2);
  cameraBuffer->setBufferObject(new osg::UniformBufferObject);
  _ubo = cameraBuffer;

  auto sz = blades.size() * sizeof(Blade);
  auto bladeBufferIn = new osg::BufferTemplate<std::vector<Blade>>;
  bladeBufferIn->setData(blades);
  auto bladeInObj = new osg::ShaderStorageBufferObject;
  bladeInObj->setUsage(GL_DYNAMIC_COPY);
  bladeBufferIn->setBufferObject(bladeInObj);

  auto bladeOut = new osg::ShaderStorageBufferObject;
  bladeOut->setUsage(GL_STREAM_DRAW);
  bladeOut->getProfile()._size = sz;

  auto bladeBufferNum = new osg::BufferTemplate<BladesNum>;
  auto bladeNumObj = new osg::ShaderStorageBufferObject;
  bladeNumObj->setUsage(GL_DYNAMIC_DRAW);
  bladeBufferNum->setBufferObject(bladeNumObj);
  _nbo = bladeBufferNum;

  auto ReadFile = [](const std::string &fileName) {
    std::string dir = __FILE__;
    auto path = std::filesystem::path(dir).parent_path();
    path.append(fileName);
    std::ifstream fs(path.string(), std::ios::in);
    std::istreambuf_iterator<char> iter(fs), end;
    std::string shaderSource(iter, end);
    return shaderSource;
  };

  {
    auto comShaderSource = ReadFile("grass.comp");
    auto program = new osg::Program;
    program->addShader(new osg::Shader(osg::Shader::COMPUTE, comShaderSource));

    auto srcNode = new ComputeNode(std::max<int>(2 * grasssz / 32, 1), std::max<int>(2 * grasssz / 32, 1), 1);
    srcNode->setDataVariance(osg::Object::DYNAMIC);
    auto ss = srcNode->getOrCreateStateSet();
    ss->setAttribute(program);

    srcNode->_ubo = cameraBuffer;
    srcNode->_bladeIn = bladeBufferIn;
    srcNode->_bladeOut = bladeOut;
    srcNode->_nbo = bladeBufferNum;

    _cmpNode = srcNode;

    addChild(srcNode);
  }

  {
    auto node = new GrassGeometry;
    auto ss = node->getStateSet();
    auto program = new osg::Program;
    program->addShader(new osg::Shader(osg::Shader::VERTEX, ReadFile("grass.vert")));
    program->addShader(new osg::Shader(osg::Shader::FRAGMENT, ReadFile("grass.frag")));
    program->addShader(new osg::Shader(osg::Shader::TESSCONTROL, ReadFile("grass.tesc")));
    program->addShader(new osg::Shader(osg::Shader::TESSEVALUATION, ReadFile("grass.tese")));
    ss->setAttribute(program);
    ss->setAttribute(new osg::PatchParameter(1));
    node->setBufferObject(bladeOut, bladeNumObj);
    addChild(node);

    // program->addBindUniformBlock("CameraBufferObject", 0);
  }
}

void GrassNode::traverse(NodeVisitor &nv)
{
  if (nv.getVisitorType() == nv.CULL_VISITOR) {
    _ubo->at(0) = *nv.asCullVisitor()->getModelViewMatrix();
    _ubo->at(1) = *nv.asCullVisitor()->getProjectionMatrix();
    _ubo->dirty();

    _nbo->dirty();
    auto ss = _cmpNode->getOrCreateStateSet();
    ss->getOrCreateUniform("wind_magnitude", osg::Uniform::FLOAT)->set(m_wind_mag);
    ss->getOrCreateUniform("wind_wave_length", osg::Uniform::FLOAT)->set(m_wind_length);
    ss->getOrCreateUniform("wind_wave_period", osg::Uniform::FLOAT)->set(m_wind_period);

    static float _prevTime = 0;
    float time = nv.getFrameStamp()->getReferenceTime();
    ss->getOrCreateUniform("current_time", osg::Uniform::FLOAT)->set(float(time));
    ss->getOrCreateUniform("delta_time", osg::Uniform::FLOAT)->set(time - _prevTime);
    _prevTime = time;
  } else if (nv.getVisitorType() == nv.UPDATE_VISITOR) {
    if (ImGui::GetCurrentContext()) {
      ImGui::Begin("xxx");
      ImGui::SliderFloat("wind_mag", &m_wind_mag, 0.1f, 10.0f);
      ImGui::SliderFloat("wind_length", &m_wind_length, 0.1f, 10.0f);
      ImGui::SliderFloat("wind_period", &m_wind_period, 0.1f, 10.0f);
      ImGui::End();
    }
  }

  Group::traverse(nv);
}
