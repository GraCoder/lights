#pragma once

#include "VulkanPipeline.h"

class VulkanPass;

class PBRPipeline : public VulkanPipeline {
public:
  PBRPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~PBRPipeline();

  virtual void realize(VulkanPass *render_pass, int subpass = 0);

  VkDescriptorSetLayout light_layout();
  void set_light_layout(VkDescriptorSetLayout layout) { _light_layout = layout; }

  VkDescriptorSetLayout pbr_layout();
  void set_pbr_layout(VkDescriptorSetLayout layout) { _pbr_layout = layout; }

protected:

  VkPipelineLayout create_pipe_layout();
  
protected:

  VkDescriptorSetLayout _light_layout = VK_NULL_HANDLE;
  VkDescriptorSetLayout _pbr_layout = VK_NULL_HANDLE;
};