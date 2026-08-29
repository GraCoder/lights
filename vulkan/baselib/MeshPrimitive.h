#pragma once

#include <vulkan/vulkan_core.h>
#include <vector>
#include <memory>

#include "tvec.h"
#include "RenderData.h"

class VulkanBuffer;
class VulkanDevice;

class MeshPrimitive {
  friend class GLTFLoader;
  friend class MeshInstance;

public:
  MeshPrimitive();
  ~MeshPrimitive();

  void setTransform(const tg::mat4 &m);

  const tg::mat4 &transform() { return _m; }

  void setVertex(uint8_t *data, int n);

  void setNormal(uint8_t *data, int n);

  void setUvs(uint8_t *data, int n);

  void setIndex(uint8_t *data, int n);

  uint32_t indexCount();

  const Material &material() { return _material; }

  void setMaterial(const Material &m);

  void realize(const std::shared_ptr<VulkanDevice> &dev);

private:

private:
  tg::mat4 _m;

  VkIndexType _indexType;

  std::vector<tg::vec3> _vertexs;
  std::vector<tg::vec3> _normals;
  std::vector<tg::vec2> _uvs;

  std::vector<uint16_t> _indexs;

  std::shared_ptr<VulkanBuffer> _vertexBuf, _normalBuf, _uvBuf, _indexBuf;


  Material _material = {};
};