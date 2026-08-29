#pragma once

#include "VulkanPipeline.h"

class DepthPersPipeline : public VulkanPipeline{
  typedef VulkanPipeline Base;
public:
  DepthPersPipeline(const std::shared_ptr<VulkanDevice> &dev, int w = 1024, int h = 1024);
  ~DepthPersPipeline();

  void realize(VulkanPass *renderPass, int subpass = 0);

  VkPipelineLayout pipeLayout();

  VkDescriptorSetLayout textureLayout();

private:
  VkPipelineLayout  createPipeLayout();

  int _w = 0, _h = 0;

  VkDescriptorSetLayout _textureLayout = VK_NULL_HANDLE;
};