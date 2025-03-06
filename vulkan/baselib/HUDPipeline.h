#pragma once

#include "VulkanPipeline.h"

class HUDPipeline : public VulkanPipeline {
  typedef VulkanPipeline Base;

public:
  HUDPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~HUDPipeline();

  void realize(VulkanPass *render_pass, int subpass = 0);

  VkDescriptorSetLayout texture_layout();

private:

  VkPipelineLayout  create_pipe_layout();

  VkDescriptorSetLayout _hud_tex_layout = VK_NULL_HANDLE;
};