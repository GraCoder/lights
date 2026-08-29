#pragma once

#include "VulkanPipeline.h"

class TexturePipeline : public VulkanPipeline {
public:
  TexturePipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~TexturePipeline();

  VkDescriptorSetLayout textureLayout();
  void setTextureLayout(VkDescriptorSetLayout layout) { _textureLayout = layout; }

  void realize(VulkanPass *renderPass, int subpass = 0) override;

protected:
  VkPipelineLayout createPipeLayout() override;

  VkDescriptorSetLayout _textureLayout = VK_NULL_HANDLE;
};
