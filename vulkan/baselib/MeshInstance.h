#pragma once

#include <vector>
#include <memory>
#include <vulkan/vulkan_core.h>

#include "tvec.h"
#include "RenderData.h"

class VulkanDevice;
class MeshPrimitive;
class VulkanBuffer;
class TexPBRPipeline;

class MeshInstance{
public:
  MeshInstance();
  ~MeshInstance();

  void setTransform(const tg::mat4 &);

  void addPrimitive(std::shared_ptr<MeshPrimitive> &pri);

  void realize(const std::shared_ptr<VulkanDevice> &dev);

  void realize(const std::shared_ptr<VulkanDevice> &dev, const std::shared_ptr<TexPBRPipeline> &pipeline);

  void buildCommandBuffer(VkCommandBuffer cmdBuf, VkPipelineLayout layout);

  void buildTextureCommandBuffer(VkCommandBuffer cmdBuf, VkPipelineLayout layout);

  void buildTextureCommandBuffer(VkCommandBuffer cmdBuf, VkPipelineLayout layout, uint32_t textureSet);

  void buildDepthTextureCommandBuffer(VkCommandBuffer cmdBuf, VkPipelineLayout layout);

private:

private:
  std::shared_ptr<VulkanDevice> _device;

  PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;

  tg::mat4 _transform;

  std::vector<std::shared_ptr<MeshPrimitive>> _pris;

  std::shared_ptr<VulkanBuffer> _pbrBuf;

  VkDescriptorSet _pbrSet = VK_NULL_HANDLE;
};
