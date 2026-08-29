#pragma once

#include "PBRPipeline.h"

class TexPBRPipeline : public PBRPipeline {
public:
  TexPBRPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~TexPBRPipeline();

  VkDescriptorSetLayout textureLayout();
  void setTextureLayout(VkDescriptorSetLayout layout) { _textureLayout = layout; }

  void realize(VulkanPass *renderPass, int subpass = 0) override;

protected:
  VkPipelineLayout createPipeLayout() override;

  VkDescriptorSetLayout _textureLayout = VK_NULL_HANDLE;
};
