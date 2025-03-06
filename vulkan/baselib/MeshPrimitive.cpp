#include "MeshPrimitive.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"
#include "VulkanBuffer.h"

#include "config.h"
#include "RenderData.h"

#define SHADER_DIR ROOT_DIR##"vulkan/baselib"

using tg::vec2;
using tg::vec3;

MeshPrimitive::MeshPrimitive()
{
}

MeshPrimitive::~MeshPrimitive()
{
}

void MeshPrimitive::set_transform(const tg::mat4& m)
{
  _m = m;
}

void MeshPrimitive::set_vertex(uint8_t* data, int n)
{
  int sz = n / sizeof(tg::vec3);
  _vertexs.resize(sz);
  memcpy(_vertexs.data(), data, n);
}

void MeshPrimitive::set_normal(uint8_t* data, int n)
{
  int sz = n / sizeof(tg::vec3);
  _normals.resize(sz);
  memcpy(_normals.data(), data, n);
}

void MeshPrimitive::set_uvs(uint8_t* data, int n)
{
  int sz = n / sizeof(tg::vec2);
  _uvs.resize(sz);
  memcpy(_uvs.data(), data, n);
}

void MeshPrimitive::set_index(uint8_t* data, int n)
{
  int sz = n / 2;
  _indexs.resize(sz);
  memcpy(_indexs.data(), data, n);
}

uint32_t MeshPrimitive::index_count()
{
  return _indexs.size();
}

void MeshPrimitive::set_material(const Material& m)
{
  _material = m;
}

void MeshPrimitive::realize(const std::shared_ptr<VulkanDevice>& dev)
{
  auto fun = [dev](uint8_t* data, int n) -> std::shared_ptr<VulkanBuffer> {
    auto ori_buf = dev->create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, n, data);
    auto dst_buf = dev->create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, n, 0);
    dev->copy_buffer(ori_buf.get(), dst_buf.get(), dev->transfer_queue());
    return dst_buf;
  };
  _vertex_buf = fun((uint8_t *)_vertexs.data(), _vertexs.size() * sizeof(vec3));
  _normal_buf = fun((uint8_t *)_normals.data(), _normals.size() * sizeof(vec3));
  _uv_buf = fun((uint8_t *)_uvs.data(), _uvs.size() * sizeof(vec2));

  auto index_sz = _indexs.size() * sizeof(uint16_t);
  auto index_ori_buf = dev->create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, index_sz, (uint8_t *)_indexs.data());
  auto index_buf = dev->create_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_sz, 0);
  dev->copy_buffer(index_ori_buf.get(), index_buf.get(), dev->transfer_queue());
  _index_buf = index_buf;
}
