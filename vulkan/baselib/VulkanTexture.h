#pragma once

#include <vulkan/vulkan_core.h>
#include <memory>
#include <string>
#include <vector>

#include "tvec.h"

class VulkanDevice;
class VulkanImage;

class VulkanTexture {
public:
  VulkanTexture();

  ~VulkanTexture();

  VkImageView imageView();

  VkImageLayout imageLayout();

  VkSampler sampler() { return _sampler; }

  VkDescriptorImageInfo descriptor();

  void loadImage(const std::string &file);

  void setImage(int w, int h, int channel, int depth, uint8_t*data, int n);

  void setImage(int w, int h, const tg::Tvec4<uint8_t> &clr);

  void realize(const std::shared_ptr<VulkanDevice> &dev);

  void realize(const std::shared_ptr<VulkanImage> &img);

private:
  std::shared_ptr<VulkanDevice> _device;

  int _w, _h, _channel, _channelDepth;

  std::vector<uint8_t> _data;

  VkImageLayout _imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

  VkImage _image = VK_NULL_HANDLE;
  VkImageView _imageView = VK_NULL_HANDLE;
  VkDeviceMemory _imageMem = VK_NULL_HANDLE;

  VkSampler _sampler = VK_NULL_HANDLE;

  std::shared_ptr<VulkanImage> _vimage;
};