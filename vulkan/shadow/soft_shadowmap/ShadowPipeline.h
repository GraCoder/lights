#pragma once

#include "TexturePipeline.h"

class ShadowPipeline : public TexturePipeline {
public:
  ShadowPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~ShadowPipeline();

  void realize(VulkanPass *renderPass, int subpass = 0);

  virtual VkPipelineLayout pipeLayout();

  VkDescriptorSetLayout lightLayout();
  VkDescriptorSetLayout shadowLayout();

private:

  VkDescriptorSetLayout _lightLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout _shadowLayout = VK_NULL_HANDLE;

};
