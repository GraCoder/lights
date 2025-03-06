#pragma once

#include <vulkan/vulkan_core.h>
#include <memory>

class VulkanDevice;

class VulkanImage {
  friend class VulkanDevice;
public:
  VulkanImage(std::shared_ptr<VulkanDevice> &dev);
  ~VulkanImage();

  uint16_t width() { return _w; }
  uint16_t height() { return _h; }

  std::shared_ptr<VulkanDevice> device() { return _device; }

  const VkImage& image() const { return _image; }
  const VkImageView& image_view() const { return _image_view; }
  const VkDeviceMemory& image_mem() const { return _image_mem; }
  const VkFormat& format() const { return _format; }

  void setImage(int w, int h, VkFormat format, VkDeviceMemory mem, VkImage img = VK_NULL_HANDLE, VkImageView imgview = VK_NULL_HANDLE);

private:
  std::shared_ptr<VulkanDevice> _device;
  uint16_t _w = 0, _h = 0;

  VkImage _image = VK_NULL_HANDLE;
  VkImageView _image_view = VK_NULL_HANDLE;
  VkDeviceMemory _image_mem = VK_NULL_HANDLE;

  VkFormat _format = VK_FORMAT_UNDEFINED;
};
