#pragma once

#include "PBRPipeline.h"

class TexturePipeline : public PBRPipeline{
public:
  TexturePipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~TexturePipeline();

  VkDescriptorSetLayout texture_layout();
  void set_texture_layout(VkDescriptorSetLayout layout) { _texture_layout = layout; }

  void realize(VulkanPass *render_pass, int subpass = 0);

  VkDescriptorSetLayout pbr_layout();

  virtual VkPipelineLayout pipe_layout();

protected:

  VkDescriptorSetLayout _texture_layout = VK_NULL_HANDLE;
};