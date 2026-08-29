#pragma once

#include "TexPBRPipeline.h"

class ShadowPipeline : public TexPBRPipeline {
public:
  ShadowPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~ShadowPipeline();

  void realize(VulkanPass *renderPass, int subpass = 0);

  virtual VkPipelineLayout pipeLayout();

  VkDescriptorSetLayout shadowLayout();

private:

  VkDescriptorSetLayout _shadowLayout = VK_NULL_HANDLE;

};
