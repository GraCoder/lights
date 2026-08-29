#pragma once

#include "VulkanPipeline.h"

class HUDPipeline : public VulkanPipeline {
  typedef VulkanPipeline Base;

public:
  HUDPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~HUDPipeline();

  void realize(VulkanPass *renderPass, int subpass = 0);

  VkDescriptorSetLayout textureLayout();

private:

  VkPipelineLayout  createPipeLayout();

  VkDescriptorSetLayout _hudTexLayout = VK_NULL_HANDLE;
};