#pragma once

#include "VulkanPipeline.h"

class VulkanPass;

class PBRPipeline : public VulkanPipeline {
public:
  PBRPipeline(const std::shared_ptr<VulkanDevice> &dev);
  ~PBRPipeline();

  virtual void realize(VulkanPass *renderPass, int subpass = 0);

  VkDescriptorSetLayout lightLayout();
  void setLightLayout(VkDescriptorSetLayout layout) { _lightLayout = layout; }

  VkDescriptorSetLayout pbrLayout();
  void setPbrLayout(VkDescriptorSetLayout layout) { _pbrLayout = layout; }

protected:

  VkPipelineLayout createPipeLayout();

protected:

  VkDescriptorSetLayout _lightLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout _pbrLayout = VK_NULL_HANDLE;
};