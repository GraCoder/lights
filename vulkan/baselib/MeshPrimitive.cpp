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

void MeshPrimitive::setTransform(const tg::mat4& m)
{
  _m = m;
}

void MeshPrimitive::setVertex(uint8_t* data, int n)
{
  int sz = n / sizeof(tg::vec3);
  _vertexs.resize(sz);
  memcpy(_vertexs.data(), data, n);
}

void MeshPrimitive::setNormal(uint8_t* data, int n)
{
  int sz = n / sizeof(tg::vec3);
  _normals.resize(sz);
  memcpy(_normals.data(), data, n);
}

void MeshPrimitive::setUvs(uint8_t* data, int n)
{
  int sz = n / sizeof(tg::vec2);
  _uvs.resize(sz);
  memcpy(_uvs.data(), data, n);
}

void MeshPrimitive::setIndex(uint8_t* data, int n)
{
  int sz = n / 2;
  _indexs.resize(sz);
  memcpy(_indexs.data(), data, n);
}

uint32_t MeshPrimitive::indexCount()
{
  return _indexs.size();
}

void MeshPrimitive::setMaterial(const Material& m)
{
  _material = m;
}

void MeshPrimitive::realize(const std::shared_ptr<VulkanDevice>& dev)
{
  auto fun = [dev](uint8_t* data, int n) -> std::shared_ptr<VulkanBuffer> {
    auto oriBuf = dev->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, n, data);
    auto dstBuf = dev->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, n, 0);
    dev->copyBuffer(oriBuf.get(), dstBuf.get(), dev->transferQueue());
    return dstBuf;
  };
  _vertexBuf = fun((uint8_t *)_vertexs.data(), _vertexs.size() * sizeof(vec3));
  _normalBuf = fun((uint8_t *)_normals.data(), _normals.size() * sizeof(vec3));
  _uvBuf = fun((uint8_t *)_uvs.data(), _uvs.size() * sizeof(vec2));

  auto indexSz = _indexs.size() * sizeof(uint16_t);
  auto indexOriBuf = dev->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, indexSz, (uint8_t *)_indexs.data());
  auto indexBuf = dev->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexSz, 0);
  dev->copyBuffer(indexOriBuf.get(), indexBuf.get(), dev->transferQueue());
  _indexBuf = indexBuf;
}
