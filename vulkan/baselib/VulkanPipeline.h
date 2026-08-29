#pragma once

#include <vulkan/vulkan_core.h>
#include "VulkanDevice.h"

class VulkanPass;

class VulkanPipeline{
public:
  VulkanPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~VulkanPipeline();

  operator VkPipeline() { return _pipeline; }

  virtual void realize(VulkanPass *renderPass, int subpass = 0) = 0;

  VkDescriptorSetLayout matrixLayout();
  void setMatrixLayout(VkDescriptorSetLayout layout) { _matrixLayout = layout; }

  bool valid() { return _pipeline != VK_NULL_HANDLE; }

  VkPipelineLayout pipeLayout();

protected:

  virtual VkPipelineLayout  createPipeLayout() = 0;

  std::shared_ptr<VulkanDevice> _device;

  VkDescriptorSetLayout _matrixLayout = VK_NULL_HANDLE;

  VkPipelineLayout  _pipeLayout = VK_NULL_HANDLE;
  VkPipeline        _pipeline = VK_NULL_HANDLE;
};