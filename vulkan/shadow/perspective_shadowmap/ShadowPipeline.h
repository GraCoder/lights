#pragma once

#include "TexturePipeline.h"

class ShadowPipeline : public TexturePipeline {
public:
  ShadowPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~ShadowPipeline();

  void realize(VulkanPass *render_pass, int subpass = 0);

  virtual VkPipelineLayout pipe_layout();

  VkDescriptorSetLayout shadow_matrix_layout();

  VkDescriptorSetLayout shadow_texture_layout();

private:

  VkDescriptorSetLayout _shadow_matrix_layout = VK_NULL_HANDLE;
  VkDescriptorSetLayout _shadow_texture_layout = VK_NULL_HANDLE;

};