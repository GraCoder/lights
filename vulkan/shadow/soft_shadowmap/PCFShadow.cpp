#include "PCFShadow.h"

#include "DepthPass.h"
#include "DepthPipeline.h"
#include "MeshInstance.h"
#include "ShadowPipeline.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"
#include "VulkanInitializers.hpp"
#include "VulkanTexture.h"
#include "VulkanTools.h"
#include "tmath.h"

#include <cassert>
#include <cstring>

PCFShadow::PCFShadow(const std::shared_ptr<VulkanDevice> &device)
  : _device(device)
{
  _lightingPipeline = std::make_shared<ShadowPipeline>(device);
  _casterPipeline = std::make_shared<DepthPipeline>(device, MapSize, MapSize, 0.5f, 2.0f);
  _image = device->createDepthImage(MapSize, MapSize, VK_FORMAT_D32_SFLOAT);
  _pass = std::make_shared<DepthPass>(device);
}

PCFShadow::~PCFShadow()
{
  destroyFrameBuffers();
  if (_descriptorPool) {
    VkDescriptorSet sets[] = {_depthMatrixSet, _shadowSet};
    vkFreeDescriptorSets(*_device, _descriptorPool, 2, sets);
  }
  if (_compareSampler)
    vkDestroySampler(*_device, _compareSampler, nullptr);
}

VkDescriptorSetLayout PCFShadow::matrixLayout() const
{
  return _lightingPipeline->matrixLayout();
}

VkDescriptorSetLayout PCFShadow::lightLayout() const
{
  return _lightingPipeline->lightLayout();
}

VkPipelineLayout PCFShadow::lightingPipelineLayout() const
{
  return _lightingPipeline->pipeLayout();
}

VulkanTexture *PCFShadow::debugTexture() const
{
  return _texture.get();
}

void PCFShadow::initializeUniforms()
{
  auto lightPosition = tg::vec3(10, 10, 0);
  _depthMatrixBuffer =
    _device->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(MVP));
  _depthMatrix.view = tg::lookat(lightPosition, tg::vec3(0, 0, 0), tg::vec3(0, 1, 0));
  _depthMatrix.prj = tg::ortho<float>(-5, 5, -5, 5, 0.1, 20);
  writeBuffer(*_depthMatrixBuffer, _depthMatrix);

  _shadowBuffer = _device->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        sizeof(ShadowMatrix));
  _shadowMatrix.light = tg::normalize(lightPosition);
  _shadowMatrix.light.w() = _filterMode;
  _shadowMatrix.view = _depthMatrix.view;
  _shadowMatrix.prj = _depthMatrix.prj;
  _shadowMatrix.mvp = _depthMatrix.prj * _depthMatrix.view;
  _shadowMatrix.options = tg::vec4(0.001f, 1.0f, MapWorldSize / MapSize, 0.0f);
  writeBuffer(*_shadowBuffer, _shadowMatrix);
}

void PCFShadow::realize(VulkanPass *renderPass, VkDescriptorPool descriptorPool)
{
  _descriptorPool = descriptorPool;
  _casterPipeline->realize(_pass.get());
  _lightingPipeline->realize(renderPass);

  _depthMatrixSet = allocateSet(descriptorPool, _casterPipeline->matrixLayout());
  writeUniformDescriptor(_depthMatrixSet, 0, *_depthMatrixBuffer, sizeof(MVP));

  _shadowSet = allocateSet(descriptorPool, _lightingPipeline->shadowLayout());
  writeUniformDescriptor(_shadowSet, 0, *_shadowBuffer, sizeof(ShadowMatrix));

  _texture = std::make_shared<VulkanTexture>();
  _texture->realize(_image);
  createCompareSampler();

  VkDescriptorImageInfo imageDescriptor = {};
  imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  imageDescriptor.imageView = _texture->imageView();
  imageDescriptor.sampler = _compareSampler;

  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = _shadowSet;
  write.dstBinding = 1;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &imageDescriptor;
  vkUpdateDescriptorSets(*_device, 1, &write, 0, nullptr);
}

void PCFShadow::createFrameBuffers(uint32_t count)
{
  destroyFrameBuffers();

  VkImageView view = _image->imageView();
  VkFramebufferCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  createInfo.renderPass = *_pass;
  createInfo.attachmentCount = 1;
  createInfo.pAttachments = &view;
  createInfo.width = _image->width();
  createInfo.height = _image->height();
  createInfo.layers = 1;

  _frameBuffers.resize(count);
  for (auto &frameBuffer : _frameBuffers)
    VK_CHECK_RESULT(vkCreateFramebuffer(*_device, &createInfo, nullptr, &frameBuffer));
}

void PCFShadow::destroyFrameBuffers()
{
  for (auto frameBuffer : _frameBuffers)
    vkDestroyFramebuffer(*_device, frameBuffer, nullptr);
  _frameBuffers.clear();
}

void PCFShadow::recordShadowPass(VkCommandBuffer cmdBuf, uint32_t frameIndex, MeshInstance &model)
{
  assert(frameIndex < _frameBuffers.size());

  VkClearValue clearValue = {};
  clearValue.depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  beginInfo.renderPass = *_pass;
  beginInfo.framebuffer = _frameBuffers[frameIndex];
  beginInfo.renderArea.extent.width = _image->width();
  beginInfo.renderArea.extent.height = _image->height();
  beginInfo.clearValueCount = 1;
  beginInfo.pClearValues = &clearValue;
  vkCmdBeginRenderPass(cmdBuf, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

  tg::mat4 transform;
  transform.identity();
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_casterPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _casterPipeline->pipeLayout(), 0, 1, &_depthMatrixSet, 0, nullptr);
  vkCmdPushConstants(cmdBuf, _casterPipeline->pipeLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Transform), &transform);
  model.buildCommandBuffer(cmdBuf, _casterPipeline->pipeLayout());

  vkCmdEndRenderPass(cmdBuf);
}

void PCFShadow::bindLighting(VkCommandBuffer cmdBuf, VkDescriptorSet matrixSet, VkDescriptorSet lightSet)
{
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_lightingPipeline);
  VkDescriptorSet commonSets[2] = {matrixSet, lightSet};
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _lightingPipeline->pipeLayout(), 0, 2, commonSets, 0, nullptr);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _lightingPipeline->pipeLayout(), 3, 1, &_shadowSet, 0, nullptr);
}

void PCFShadow::toggleFilterMode()
{
  _filterMode = _filterMode >= 0.5f ? 0.0f : 1.0f;
  _shadowMatrix.light.w() = _filterMode;
  writeBuffer(*_shadowBuffer, _shadowMatrix);
}

bool PCFShadow::valid() const
{
  return _casterPipeline->valid() && _lightingPipeline->valid();
}

template <typename T>
void PCFShadow::writeBuffer(VulkanBuffer &buffer, const T &value)
{
  void *data = nullptr;
  VK_CHECK_RESULT(vkMapMemory(*_device, buffer.memory(), 0, sizeof(T), 0, &data));
  memcpy(data, &value, sizeof(T));
  vkUnmapMemory(*_device, buffer.memory());
}

VkDescriptorSet PCFShadow::allocateSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout layout)
{
  VkDescriptorSet set = VK_NULL_HANDLE;
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;
  VK_CHECK_RESULT(vkAllocateDescriptorSets(*_device, &allocInfo, &set));
  return set;
}

void PCFShadow::writeUniformDescriptor(VkDescriptorSet set, uint32_t binding, VulkanBuffer &buffer, VkDeviceSize size)
{
  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = buffer;
  bufferInfo.offset = 0;
  bufferInfo.range = size;

  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = set;
  write.dstBinding = binding;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  write.pBufferInfo = &bufferInfo;
  vkUpdateDescriptorSets(*_device, 1, &write, 0, nullptr);
}

void PCFShadow::createCompareSampler()
{
  VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  samplerInfo.compareEnable = VK_TRUE;
  samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;
  VK_CHECK_RESULT(vkCreateSampler(*_device, &samplerInfo, nullptr, &_compareSampler));
}
