/*
 * Vulkan device class
 *
 * Encapsulates a physical Vulkan device and its logical representation
 *
 * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#include "vulkan/vulkan.h"
#include "VulkanDef.h"

#include <vector>
#include <string>
#include <assert.h>
#include <exception>
#include <optional>
#include <memory>

class VulkanBuffer;
class VulkanImage;

class VulkanDevice : public std::enable_shared_from_this<VulkanDevice>{
public:

  explicit VulkanDevice(VkPhysicalDevice _physical_device);
  ~VulkanDevice();
  operator VkDevice() const { return _logical_device; };
  VkPhysicalDevice physical_device() { return _physical_device; }
  VkCommandPool command_pool() { return _command_pool; }

  VkResult realize(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> enabledExtensions, void *pNextChain,
                               bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

  VkRenderPass create_render_pass(VkFormat color, VkFormat depth = VK_FORMAT_D24_UNORM_S8_UINT);
  void destroy_render_pass(VkRenderPass rdpass);
  
  std::tuple<VkImage, VkDeviceMemory> create_image(int w, int h, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);
  VkImageView create_image_view(VkImage img, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

  std::shared_ptr<VulkanImage> create_color_image(uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);
  std::shared_ptr<VulkanImage> create_depth_image(uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_D24_UNORM_S8_UINT);

  VkShaderModule create_shader(const std::string &file);

  VkShaderModule create_shader(const char *source, int n);

  VkPipelineCache get_or_create_pipecache();

  VkDescriptorPool get_or_create_descriptor_pool();

  uint32_t queue_family_index(VkQueueFlags queueFlags) const;

  std::optional<uint32_t> memory_type_index(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

  std::shared_ptr<VulkanBuffer> create_buffer(VkBufferUsageFlags usageFlags, 
    VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void *data = nullptr);
  void copy_buffer(VulkanBuffer *src, VulkanBuffer *dst, VkQueue queue, VkBufferCopy *copyRegion = nullptr);

  VkCommandPool create_command_pool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  VkCommandBuffer create_command_buffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = true);
  VkCommandBuffer create_command_buffer(VkCommandBufferLevel level, bool begin = true);
  void flush_command_buffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);
  void flush_command_buffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);

  std::vector<VkCommandBuffer> create_command_buffers(uint32_t n);
  void destroy_command_buffers(std::vector<VkCommandBuffer> &cmdbufs);

  std::vector<VkFence> create_fences(uint32_t n);

  VkQueue graphic_queue(uint32_t idx = 0);
  VkQueue transfer_queue(uint32_t idx = 0);

  bool extension_supported(std::string extension);
  VkFormat supported_depth_format(bool checkSamplingSupport);

public:

  const std::vector<VkQueueFamilyProperties> &queue_family_properties() { return _queue_family_properties; }

public:

  PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR = nullptr;

private:
  /** @brief Physical device representation */
  VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
  /** @brief Logical device representation (application's view of the device) */
  VkDevice _logical_device = VK_NULL_HANDLE;
  /** @brief Properties of the physical device including limits that the application can check against */
  VkPhysicalDeviceProperties properties;
  /** @brief Features of the physical device that an application can use to check if a feature is supported */
  VkPhysicalDeviceFeatures features;
  /** @brief Features that have been enabled for use on the physical device */
  VkPhysicalDeviceFeatures enabledFeatures;
  /** @brief Memory types and heaps of the physical device */
  VkPhysicalDeviceMemoryProperties memoryProperties;
  /** @brief Queue family properties of the physical device */
  std::vector<VkQueueFamilyProperties> _queue_family_properties;
  /** @brief List of extensions supported by the device */
  std::vector<std::string> supportedExtensions;
  /** @brief Default command pool for the graphics queue family index */
  VkCommandPool _command_pool = VK_NULL_HANDLE;
  /** @brief Contains queue family indices */
  struct {
    uint32_t graphics;
    uint32_t compute;
    uint32_t transfer;
  } _queue_family;

  VkPipelineCache _pipe_cache = VK_NULL_HANDLE;
  VkDescriptorPool _descriptor_pool = VK_NULL_HANDLE;
};
