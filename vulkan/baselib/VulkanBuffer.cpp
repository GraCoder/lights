#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"

VulkanBuffer::VulkanBuffer(const std::shared_ptr<VulkanDevice>& dev) : _device(dev)
{
}

VulkanBuffer::~VulkanBuffer()
{
  destroy();
}

uint8_t* VulkanBuffer::map()
{
  uint8_t* entry = 0;
  VK_CHECK_RESULT(vkMapMemory(*_device, _memory, 0, _size, 0, (void **)&entry));
  return entry;
}

void VulkanBuffer::unmap()
{
  vkUnmapMemory(*_device, _memory);
}

/**
 * Flush a _memory range of the _buffer to make it visible to the _device
 *
 * @note Only required for non-coherent _memory
 *
 * @param size (Optional) Size of the _memory range to flush. Pass VK_WHOLE_SIZE to flush the complete _buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the flush call
 */
VkResult VulkanBuffer::flush(VkDeviceSize size, VkDeviceSize offset)
{
  if (size == VK_WHOLE_SIZE)
    size = _size;
  VkMappedMemoryRange mappedRange = {};
  mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  mappedRange.memory = _memory;
  mappedRange.offset = offset;
  mappedRange.size = size;
  return vkFlushMappedMemoryRanges(*_device, 1, &mappedRange);
}

/**
 * Invalidate a _memory range of the _buffer to make it visible to the host
 *
 * @note Only required for non-coherent _memory
 *
 * @param size (Optional) Size of the _memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete _buffer range.
 * @param offset (Optional) Byte offset from beginning
 *
 * @return VkResult of the invalidate call
 */
VkResult VulkanBuffer::invalidate(VkDeviceSize size, VkDeviceSize offset)
{
  VkMappedMemoryRange mappedRange = {};
  mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  mappedRange.memory = _memory;
  mappedRange.offset = offset;
  mappedRange.size = size;
  return vkInvalidateMappedMemoryRanges(*_device, 1, &mappedRange);
}

/**
 * Release all Vulkan resources held by this _buffer
 */
void VulkanBuffer::destroy()
{
  if (_buffer) {
    vkDestroyBuffer(*_device, _buffer, nullptr);
    _buffer = VK_NULL_HANDLE;
  }
  if (_memory) {
    vkFreeMemory(*_device, _memory, nullptr);
    _memory = VK_NULL_HANDLE;
  }
}
