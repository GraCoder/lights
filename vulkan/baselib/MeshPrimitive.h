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

  void set_transform(const tg::mat4 &m);

  const tg::mat4 &transform() { return _m; }

  void set_vertex(uint8_t *data, int n);

  void set_normal(uint8_t *data, int n);

  void set_uvs(uint8_t *data, int n);

  void set_index(uint8_t *data, int n);

  uint32_t index_count();

  const Material &material() { return _material; }

  void set_material(const Material &m);

  void realize(const std::shared_ptr<VulkanDevice> &dev);

private:

private:
  tg::mat4 _m;

  VkIndexType _index_type;

  std::vector<tg::vec3> _vertexs;
  std::vector<tg::vec3> _normals;
  std::vector<tg::vec2> _uvs;

  std::vector<uint16_t> _indexs;

  std::shared_ptr<VulkanBuffer> _vertex_buf, _normal_buf, _uv_buf, _index_buf;


  Material _material = {};
};