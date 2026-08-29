#include "MeshInstance.h"

#include "VulkanTools.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "MeshPrimitive.h"
#include "VulkanTexture.h"
#include "VulkanInitializers.hpp"
#include "TexPBRPipeline.h"

#include "tvec.h"
#include "tmath.h"
#include "config.h"

#define SHADER_DIR ROOT_DIR##"/vulkan/baselib/shaders"


MeshInstance::MeshInstance()
{
  _transform.identity();
}

MeshInstance::~MeshInstance()
{
  if(_pbrSet) {
    vkFreeDescriptorSets(*_device, _device->getOrCreateDescriptorPool(), 1, &_pbrSet);
    _pbrSet = VK_NULL_HANDLE;
  }
}

void MeshInstance::setTransform(const tg::mat4 &transform)
{
  _transform = transform;
}

void MeshInstance::addPrimitive(std::shared_ptr<MeshPrimitive>& pri) {
  _pris.emplace_back(pri);

  auto &m = pri->material();
  //if (m.tex) {

  //  VkDescriptorSet material_set;
  //  VK_CHECK_RESULT(vkAllocateDescriptorSets(*_device, &allocInfo, &material_set));

  //  VkDescriptorImageInfo descriptor;
  //  descriptor.imageView = m.tex->image_view();
  //  descriptor.sampler = m.tex->sampler();
  //  descriptor.imageLayout = m.tex->image_layout();

  //  VkWriteDescriptorSet set = {};
  //  set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  //  set.dstSet = material_set;
  //  set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  //  set.dstBinding = 0;
  //  set.pImageInfo = &descriptor;
  //  set.descriptorCount = 1;

  //  vkUpdateDescriptorSets(*_device, 1, &set, 0, nullptr);

  //  pri->set_material_set(material_set);
  //}
}

void MeshInstance::realize(const std::shared_ptr<VulkanDevice> &dev)
{
  _device = dev;

  for (auto &pri : _pris) {
    pri->realize(dev);
    auto &tex = pri->material().albedoTex;
    if (tex)
      tex->realize(dev);
  }

  vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(*dev, "vkCmdPushDescriptorSetKHR");
}

void MeshInstance::realize(const std::shared_ptr<VulkanDevice> &dev, const std::shared_ptr<TexPBRPipeline> &pipeline)
{
  realize(dev);

  std::vector<PBRBase> pbrdata(_pris.size());
  for (int i = 0; i < _pris.size(); i++) {
    auto &pbr = pbrdata[i];
    auto &m = _pris[i]->material();
    memcpy(&pbr, &m.pbrdata, sizeof(PBRBase));
  }
  uint32_t sz = pbrdata.size() * sizeof(PBRBase);
  auto oriBuf = dev->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, sz, pbrdata.data());
  auto dstBuf = dev->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sz, 0);
  dev->copyBuffer(oriBuf.get(), dstBuf.get(), dev->transferQueue());
  _pbrBuf = dstBuf;

  auto layout = pipeline->pbrLayout();
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = _device->getOrCreateDescriptorPool();
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;

  VK_CHECK_RESULT(vkAllocateDescriptorSets(*_device, &allocInfo, &_pbrSet));

  VkDescriptorBufferInfo descriptor = {};
  descriptor.buffer = *_pbrBuf;
  descriptor.offset = 0;
  descriptor.range = sizeof(PBRBase);

  VkWriteDescriptorSet writeDescriptorSet = {};
  writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSet.dstSet = _pbrSet;
  writeDescriptorSet.descriptorCount = 1;
  writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  writeDescriptorSet.pBufferInfo = &descriptor;
  writeDescriptorSet.dstBinding = 0;
  vkUpdateDescriptorSets(*_device, 1, &writeDescriptorSet, 0, nullptr);
}

void MeshInstance::buildCommandBuffer(VkCommandBuffer cmdBuf, VkPipelineLayout layout)
{
  if (layout == VK_NULL_HANDLE)
    return;

  for (int i = 0; i < _pris.size(); i++) {
    auto &pri = _pris[i];
    auto m = _transform * pri->transform();
    vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(m), &m);

    std::vector<VkBuffer> bufs(2);
    bufs[0] = *pri->_vertexBuf;
    bufs[1] = *pri->_normalBuf;
    std::vector<VkDeviceSize> offset(bufs.size(), 0);
    vkCmdBindVertexBuffers(cmdBuf, 0, bufs.size(), bufs.data(), offset.data());
    vkCmdBindIndexBuffer(cmdBuf, *pri->_indexBuf, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmdBuf, pri->indexCount(), 1, 0, 0, 0);
  }
}

void MeshInstance::buildTextureCommandBuffer(VkCommandBuffer cmdBuf, VkPipelineLayout layout)
{
  if (layout == VK_NULL_HANDLE)
    return;

  for (int i = 0; i < _pris.size(); i++) {
    auto &pri = _pris[i];
    auto m = _transform * pri->transform();
    vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(m), &m);

    uint32_t uoffset = i * sizeof(PBRBase);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 2, 1, &_pbrSet, 1, &uoffset);

    VkWriteDescriptorSet textureSet = {};
    textureSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    textureSet.dstSet = 0;
    textureSet.dstBinding = 0;
    textureSet.descriptorCount = 1;
    textureSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    auto descriptor = pri->material().albedoTex->descriptor();
    textureSet.pImageInfo = &descriptor;
    vkCmdPushDescriptorSetKHR(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 3, 1, &textureSet);

    std::vector<VkBuffer> bufs(3);
    bufs[0] = *pri->_vertexBuf;
    bufs[1] = *pri->_normalBuf;
    bufs[2] = *pri->_uvBuf;
    std::vector<VkDeviceSize> offset(bufs.size(), 0);
    vkCmdBindVertexBuffers(cmdBuf, 0, bufs.size(), bufs.data(), offset.data());
    vkCmdBindIndexBuffer(cmdBuf, *pri->_indexBuf, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmdBuf, pri->indexCount(), 1, 0, 0, 0);
  }
}

void MeshInstance::buildTextureCommandBuffer(VkCommandBuffer cmdBuf, VkPipelineLayout layout, uint32_t textureSetIndex)
{
  if (layout == VK_NULL_HANDLE)
    return;

  for (auto &pri : _pris) {
    auto m = _transform * pri->transform();
    vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(m), &m);

    VkWriteDescriptorSet textureSet = {};
    textureSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    textureSet.dstBinding = 0;
    textureSet.descriptorCount = 1;
    textureSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    auto descriptor = pri->material().albedoTex->descriptor();
    textureSet.pImageInfo = &descriptor;
    vkCmdPushDescriptorSetKHR(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, textureSetIndex, 1, &textureSet);

    VkBuffer bufs[] = {*pri->_vertexBuf, *pri->_normalBuf, *pri->_uvBuf};
    VkDeviceSize offsets[] = {0, 0, 0};
    vkCmdBindVertexBuffers(cmdBuf, 0, 3, bufs, offsets);
    vkCmdBindIndexBuffer(cmdBuf, *pri->_indexBuf, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmdBuf, pri->indexCount(), 1, 0, 0, 0);
  }
}

void MeshInstance::buildDepthTextureCommandBuffer(VkCommandBuffer cmdBuf, VkPipelineLayout layout)
{
  if (layout == VK_NULL_HANDLE)
    return;

  for (int i = 0; i < _pris.size(); i++) {
    auto &pri = _pris[i];
    auto m = _transform * pri->transform();
    vkCmdPushConstants(cmdBuf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(m), &m);

    VkWriteDescriptorSet textureSet = {};
    textureSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    textureSet.dstSet = 0;
    textureSet.dstBinding = 0;
    textureSet.descriptorCount = 1;
    textureSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    auto descriptor = pri->material().albedoTex->descriptor();
    textureSet.pImageInfo = &descriptor;
    vkCmdPushDescriptorSetKHR(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &textureSet);

    std::vector<VkBuffer> bufs(3);
    bufs[0] = *pri->_vertexBuf;
    bufs[1] = *pri->_normalBuf;
    bufs[2] = *pri->_uvBuf;
    std::vector<VkDeviceSize> offset(bufs.size(), 0);
    vkCmdBindVertexBuffers(cmdBuf, 0, bufs.size(), bufs.data(), offset.data());
    vkCmdBindIndexBuffer(cmdBuf, *pri->_indexBuf, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmdBuf, pri->indexCount(), 1, 0, 0, 0);
  }
}

