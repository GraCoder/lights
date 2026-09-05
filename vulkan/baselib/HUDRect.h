#pragma once

#include "VulkanDevice.h"

class HUDPipeline;
class VulkanTexture;

class HUDRect {
public:
  HUDRect(const std::shared_ptr<VulkanDevice> &dev);
  ~HUDRect();

  void setGeometry(float x, float y, float w, float h);

  void setTexture(HUDPipeline *pipeline, VulkanTexture *tex, VkDescriptorPool pool,
                  VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  void fillCommand(VkCommandBuffer cmdbuf, HUDPipeline *pipeline);

private:
  std::shared_ptr<VulkanDevice> _device;

  std::shared_ptr<VulkanBuffer> _buffer;

  VkDescriptorSet _set = VK_NULL_HANDLE;
  VkDescriptorPool _pool = VK_NULL_HANDLE;
};
