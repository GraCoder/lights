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

  explicit VulkanDevice(VkPhysicalDevice physicalDevice);
  ~VulkanDevice();
  operator VkDevice() const { return _logicalDevice; };
  VkPhysicalDevice physicalDevice() { return _physicalDevice; }
  VkCommandPool commandPool() { return _commandPool; }

  VkResult realize(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> enabledExtensions, void *pNextChain,
                               bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

  VkRenderPass createRenderPass(VkFormat color, VkFormat depth = VK_FORMAT_D24_UNORM_S8_UINT);
  void destroyRenderPass(VkRenderPass rdpass);

  std::tuple<VkImage, VkDeviceMemory> createImage(int w, int h, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);
  VkImageView createImageView(VkImage img, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

  std::shared_ptr<VulkanImage> createColorImage(uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                                                VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
  std::shared_ptr<VulkanImage> createDepthImage(uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_D24_UNORM_S8_UINT);

  VkShaderModule createShader(const std::string &file);

  VkShaderModule createShader(const char *source, int n);

  VkPipelineCache getOrCreatePipecache();

  VkDescriptorPool getOrCreateDescriptorPool();

  uint32_t queueFamilyIndex(VkQueueFlags queueFlags) const;

  std::optional<uint32_t> memoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

  std::shared_ptr<VulkanBuffer> createBuffer(VkBufferUsageFlags usageFlags,
    VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void *data = nullptr);
  void copyBuffer(VulkanBuffer *src, VulkanBuffer *dst, VkQueue queue, VkBufferCopy *copyRegion = nullptr);

  VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = true);
  VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = true);
  void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);
  void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);

  std::vector<VkCommandBuffer> createCommandBuffers(uint32_t n);
  void destroyCommandBuffers(std::vector<VkCommandBuffer> &cmdbufs);

  std::vector<VkFence> createFences(uint32_t n);

  VkQueue graphicQueue(uint32_t idx = 0);
  VkQueue transferQueue(uint32_t idx = 0);

  bool extensionSupported(std::string extension);
  VkFormat supportedDepthFormat(bool checkSamplingSupport);

public:

  const std::vector<VkQueueFamilyProperties> &queueFamilyProperties() { return _queueFamilyProperties; }

public:

  PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR = nullptr;

private:
  /** @brief Physical device representation */
  VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
  /** @brief Logical device representation (application's view of the device) */
  VkDevice _logicalDevice = VK_NULL_HANDLE;
  /** @brief Properties of the physical device including limits that the application can check against */
  VkPhysicalDeviceProperties properties;
  /** @brief Features of the physical device that an application can use to check if a feature is supported */
  VkPhysicalDeviceFeatures features;
  /** @brief Features that have been enabled for use on the physical device */
  VkPhysicalDeviceFeatures enabledFeatures;
  /** @brief Memory types and heaps of the physical device */
  VkPhysicalDeviceMemoryProperties memoryProperties;
  /** @brief Queue family properties of the physical device */
  std::vector<VkQueueFamilyProperties> _queueFamilyProperties;
  /** @brief List of extensions supported by the device */
  std::vector<std::string> supportedExtensions;
  /** @brief Default command pool for the graphics queue family index */
  VkCommandPool _commandPool = VK_NULL_HANDLE;
  /** @brief Contains queue family indices */
  struct {
    uint32_t graphics;
    uint32_t compute;
    uint32_t transfer;
  } _queueFamily;

  VkPipelineCache _pipeCache = VK_NULL_HANDLE;
  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
};
