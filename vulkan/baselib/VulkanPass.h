#pragma once

#include "VulkanDevice.h"

class VulkanPass{
public:
  VulkanPass(const std::shared_ptr<VulkanDevice> &dev);
  ~VulkanPass();

  operator VkRenderPass();

protected:

  virtual void initialize();

protected:
  std::shared_ptr<VulkanDevice> _device;

  VkRenderPass _renderPass = VK_NULL_HANDLE;
};

