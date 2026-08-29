#pragma once

#include "TexPBRPipeline.h"

class ShadowPipeline : public TexPBRPipeline {
public:
  ShadowPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~ShadowPipeline();

  void realize(VulkanPass *renderPass, int subpass = 0);

  virtual VkPipelineLayout pipeLayout();

  VkDescriptorSetLayout shadowMatrixLayout();

  VkDescriptorSetLayout shadowTextureLayout();

private:

  VkDescriptorSetLayout _shadowMatrixLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout _shadowTextureLayout = VK_NULL_HANDLE;

};
