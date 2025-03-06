#include "MeshInstance.h"

#include "VulkanTools.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "MeshPrimitive.h"
#include "VulkanTexture.h"
#include "VulkanInitializers.hpp"
#include "TexturePipeline.h"
#include "DepthPersPipeline.h"

#include "tvec.h"
#include "config.h"

#define SHADER_DIR ROOT_DIR##"/vulkan/baselib/shaders"


MeshInstance::MeshInstance()
{
  _transform.identity();
}

MeshInstance::~MeshInstance()
{
  if(_pbr_set) {
    vkFreeDescriptorSets(*_device, _device->get_or_create_descriptor_pool(), 1, &_pbr_set);
    _pbr_set = VK_NULL_HANDLE;
  }
}

void MeshInstance::set_transform(const tg::mat4 &transform)
{
  _transform = transform;
}

void MeshInstance::add_primitive(std::shared_ptr<MeshPrimitive>& pri) {
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
    auto &tex = pri->material().albedo_tex;
    if (tex)
      tex->realize(dev);
  }

  vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(*dev, "vkCmdPushDescriptorSetKHR");
}

void MeshInstance::realize(const std::shared_ptr<VulkanDevice> &dev, const std::shared_ptr<TexturePipeline> &pipeline)
{
  realize(dev);

  std::vector<PBRBase> pbrdata(_pris.size());
  for (int i = 0; i < _pris.size(); i++) {
    auto &pbr = pbrdata[i];
    auto &m = _pris[i]->material();
    memcpy(&pbr, &m.pbrdata, sizeof(PBRBase));
  }
  uint32_t sz = pbrdata.size() * sizeof(PBRBase);
  auto ori_buf = dev->create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, sz, pbrdata.data());
  auto dst_buf = dev->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sz, 0);
  dev->copy_buffer(ori_buf.get(), dst_buf.get(), dev->transfer_queue());
  _pbr_buf = dst_buf;

  auto layout = pipeline->pbr_layout();
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = _device->get_or_create_descriptor_pool();
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;

  VK_CHECK_RESULT(vkAllocateDescriptorSets(*_device, &allocInfo, &_pbr_set));

  VkDescriptorBufferInfo descriptor = {};
  descriptor.buffer = *_pbr_buf;
  descriptor.offset = 0;
  descriptor.range = sizeof(PBRBase);

  VkWriteDescriptorSet writeDescriptorSet = {};
  writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSet.dstSet = _pbr_set;
  writeDescriptorSet.descriptorCount = 1;
  writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  writeDescriptorSet.pBufferInfo = &descriptor;
  writeDescriptorSet.dstBinding = 0;
  vkUpdateDescriptorSets(*_device, 1, &writeDescriptorSet, 0, nullptr);
}

void MeshInstance::build_command_buffer(VkCommandBuffer cmd_buf, const std::shared_ptr<VulkanPipeline> &pipeline)
{
  if (!pipeline || !pipeline->valid())
    return;

  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

  for (int i = 0; i < _pris.size(); i++) {
    auto &pri = _pris[i];
    auto m = _transform * pri->transform();
    vkCmdPushConstants(cmd_buf, pipeline->pipe_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(m), &m);

    std::vector<VkBuffer> bufs(2);
    bufs[0] = *pri->_vertex_buf;
    bufs[1] = *pri->_normal_buf;
    std::vector<VkDeviceSize> offset(bufs.size(), 0);
    vkCmdBindVertexBuffers(cmd_buf, 0, bufs.size(), bufs.data(), offset.data());
    vkCmdBindIndexBuffer(cmd_buf, *pri->_index_buf, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd_buf, pri->index_count(), 1, 0, 0, 0);
  }
}

void MeshInstance::build_command_buffer(VkCommandBuffer cmd_buf, const std::shared_ptr<TexturePipeline> &pipeline)
{
  if (!pipeline || !pipeline->valid())
    return;


  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

  for (int i = 0; i < _pris.size(); i++) {
    auto &pri = _pris[i];
    auto m = _transform * pri->transform();
    vkCmdPushConstants(cmd_buf, pipeline->pipe_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(m), &m);

    uint32_t uoffset = i * sizeof(PBRBase);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipe_layout(), 2, 1, &_pbr_set, 1, &uoffset);

    VkWriteDescriptorSet texture_set = {};
    texture_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    texture_set.dstSet = 0;
    texture_set.dstBinding = 0;
    texture_set.descriptorCount = 1;
    texture_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    auto descriptor = pri->material().albedo_tex->descriptor();
    texture_set.pImageInfo = &descriptor;
    vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipe_layout(), 3, 1, &texture_set);

    std::vector<VkBuffer> bufs(3);
    bufs[0] = *pri->_vertex_buf;
    bufs[1] = *pri->_normal_buf;
    bufs[2] = *pri->_uv_buf;
    std::vector<VkDeviceSize> offset(bufs.size(), 0);
    vkCmdBindVertexBuffers(cmd_buf, 0, bufs.size(), bufs.data(), offset.data());
    vkCmdBindIndexBuffer(cmd_buf, *pri->_index_buf, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd_buf, pri->index_count(), 1, 0, 0, 0);
  }
}

void MeshInstance::build_command_buffer(VkCommandBuffer cmd_buf, const std::shared_ptr<DepthPersPipeline> &pipeline)
{
  if (!pipeline || !pipeline->valid())
    return;

  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline);

  for (int i = 0; i < _pris.size(); i++) {
    auto &pri = _pris[i];
    auto m = _transform * pri->transform();
    vkCmdPushConstants(cmd_buf, pipeline->pipe_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(m), &m);

    VkWriteDescriptorSet texture_set = {};
    texture_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    texture_set.dstSet = 0;
    texture_set.dstBinding = 0;
    texture_set.descriptorCount = 1;
    texture_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    auto descriptor = pri->material().albedo_tex->descriptor();
    texture_set.pImageInfo = &descriptor;
    vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipe_layout(), 1, 1, &texture_set);

    std::vector<VkBuffer> bufs(3);
    bufs[0] = *pri->_vertex_buf;
    bufs[1] = *pri->_normal_buf;
    bufs[2] = *pri->_uv_buf;
    std::vector<VkDeviceSize> offset(bufs.size(), 0);
    vkCmdBindVertexBuffers(cmd_buf, 0, bufs.size(), bufs.data(), offset.data());
    vkCmdBindIndexBuffer(cmd_buf, *pri->_index_buf, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd_buf, pri->index_count(), 1, 0, 0, 0);
  }
}

