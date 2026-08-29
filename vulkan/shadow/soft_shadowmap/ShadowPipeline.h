#pragma once

#include "TexturePipeline.h"

#include <string>

class ShadowPipeline : public TexturePipeline
{
public:
  ShadowPipeline(const std::shared_ptr<VulkanDevice> &dev, std::string fragmentShader = "pcf_shadow.frag.spv");
  ~ShadowPipeline();

  void realize(VulkanPass *renderPass, int subpass = 0);

  virtual VkPipelineLayout pipeLayout();

  VkDescriptorSetLayout lightLayout();
  VkDescriptorSetLayout shadowLayout();

private:

  std::string _fragmentShader;

  VkDescriptorSetLayout _lightLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout _shadowLayout = VK_NULL_HANDLE;
};
