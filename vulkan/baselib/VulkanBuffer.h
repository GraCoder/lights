#pragma once

#include <vulkan/vulkan.h>
#include <memory>

class VulkanBuffer : public std::enable_shared_from_this<VulkanBuffer>{
  friend class VulkanDevice;
public:
  VulkanBuffer(const std::shared_ptr<VulkanDevice> &dev);
  ~VulkanBuffer();

  operator VkBuffer() const { return _buffer; }
  operator VkBuffer*() { return &_buffer; }
  VkDeviceMemory memory() { return _memory; }
  VkDeviceSize size() { return _size; }
  VkDeviceSize memsize() { return _memsize; }

  uint8_t* map();
  void unmap();

  VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
  VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
  void destroy();

private:

  std::shared_ptr<VulkanDevice> _device = nullptr;

  VkBuffer _buffer = VK_NULL_HANDLE;
  VkDeviceMemory _memory = VK_NULL_HANDLE;
  VkDeviceSize _size = 0, _memsize = 0;
  VkDeviceSize _alignment = 0;
  VkBufferUsageFlags _usageFlags;
  VkMemoryPropertyFlags _memoryPropertyFlags;
};