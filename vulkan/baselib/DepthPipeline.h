#pragma once

#include "VulkanPipeline.h"

class DepthPipeline : public VulkanPipeline{
  typedef VulkanPipeline Base;
public:
  DepthPipeline(const std::shared_ptr<VulkanDevice> &dev, int w = 1024, int h = 1024);
  ~DepthPipeline();

  void realize(VulkanPass *renderPass, int subpass = 0);

  VkPipelineLayout pipeLayout();

private:

  VkPipelineLayout  createPipeLayout();

private:

  int _w = 0, _h = 0;
};