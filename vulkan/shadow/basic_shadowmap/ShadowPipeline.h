#pragma once

#include "TexturePipeline.h"

class ShadowPipeline : public TexturePipeline {
public:
  ShadowPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~ShadowPipeline();

  void realize(VulkanPass *render_pass, int subpass = 0);

  virtual VkPipelineLayout pipe_layout();

  VkDescriptorSetLayout shadow_layout();

private:

  VkDescriptorSetLayout _shadow_layout = VK_NULL_HANDLE;

};