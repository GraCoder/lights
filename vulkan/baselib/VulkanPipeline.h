#pragma once

#include <vulkan/vulkan_core.h>
#include "VulkanDevice.h"

class VulkanPass;

class VulkanPipeline{
public:
  VulkanPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~VulkanPipeline();

  operator VkPipeline() { return _pipeline; }

  virtual void realize(VulkanPass *render_pass, int subpass = 0) = 0;

  VkDescriptorSetLayout matrix_layout();
  void set_matrix_layout(VkDescriptorSetLayout layout) { _matrix_layout = layout; }

  bool valid() { return _pipeline != VK_NULL_HANDLE; }

  VkPipelineLayout pipe_layout();

protected:

  virtual VkPipelineLayout  create_pipe_layout() = 0;
  
  std::shared_ptr<VulkanDevice> _device;

  VkDescriptorSetLayout _matrix_layout = VK_NULL_HANDLE;

  VkPipelineLayout  _pipe_layout = VK_NULL_HANDLE;
  VkPipeline        _pipeline = VK_NULL_HANDLE;
};