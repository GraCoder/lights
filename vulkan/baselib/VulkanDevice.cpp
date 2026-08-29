#include <array>
#include <unordered_set>
#include <iostream>
#include <stdexcept>

#include "VulkanDevice.h"
#include "VulkanTools.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanInitializers.hpp"

/**
 * Default constructor
 *
 * @param physicalDevice Physical device that is to be used
 */
VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice)
{
  assert(physicalDevice);
  this->_physicalDevice = physicalDevice;

  // Store Properties features, limits and properties of the physical device for later use
  // Device properties also contain limits and sparse properties
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);
  // Features should be checked by the examples before using them
  vkGetPhysicalDeviceFeatures(physicalDevice, &features);
  // Memory properties are used regularly for creating all kinds of buffers
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
  // Queue family properties, used for setting up requested queues upon device creation
  uint32_t queueFamilyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
  assert(queueFamilyCount > 0);
  _queueFamilyProperties.resize(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, _queueFamilyProperties.data());

  // Get list of supported extensions
  uint32_t extCount = 0;
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
  if (extCount > 0) {
    std::vector<VkExtensionProperties> extensions(extCount);
    if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS) {
      for (auto ext : extensions) {
        supportedExtensions.push_back(ext.extensionName);
      }
    }
  }
}

/**
 * Default destructor
 *
 * @note Frees the logical device
 */
VulkanDevice::~VulkanDevice()
{
  if (_pipeCache) {
    vkDestroyPipelineCache(_logicalDevice, _pipeCache, nullptr);
    _pipeCache = VK_NULL_HANDLE;
  }

  if (_commandPool) {
    vkDestroyCommandPool(_logicalDevice, _commandPool, nullptr);
    _commandPool = VK_NULL_HANDLE;
  }

  if(_descriptorPool) {
    vkDestroyDescriptorPool(_logicalDevice, _descriptorPool, nullptr);
    _descriptorPool = VK_NULL_HANDLE;
  }

  if (_logicalDevice) {
    vkDestroyDevice(_logicalDevice, nullptr);
    _logicalDevice = VK_NULL_HANDLE;
  }
}

/**
 * Create the logical device based on the assigned physical device, also gets default queue family indices
 *
 * @param enabledFeatures Can be used to enable certain features upon device creation
 * @param pNextChain Optional chain of pointer to extension structures
 * @param useSwapChain Set to false for headless rendering to omit the swapchain device extensions
 * @param requestedQueueTypes Bit flags specifying the queue types to be requested from the device
 *
 * @return VkResult of the device creation call
 */
VkResult VulkanDevice::realize(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> enabledExtensions, void *pNextChain,
                                           bool useSwapChain, VkQueueFlags requestedQueueTypes)
{
  // Desired queues need to be requested upon logical device creation
  // Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
  // requests different queue types

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

  // Get queue family indices for the requested queue family types
  // Note that the indices may overlap depending on the implementation

  const float defaultQueuePriority(0.0f);

  // Graphics queue
  if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT) {
    _queueFamily.graphics = queueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = _queueFamily.graphics;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &defaultQueuePriority;
    queueCreateInfos.push_back(queueInfo);
  } else {
    _queueFamily.graphics = 0;
  }

  // Dedicated compute queue
  if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT) {
    _queueFamily.compute = queueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
    if (_queueFamily.compute != _queueFamily.graphics) {
      // If compute family index differs, we need an additional queue create info for the compute queue
      VkDeviceQueueCreateInfo queueInfo{};
      queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueInfo.queueFamilyIndex = _queueFamily.compute;
      queueInfo.queueCount = 1;
      queueInfo.pQueuePriorities = &defaultQueuePriority;
      queueCreateInfos.push_back(queueInfo);
    }
  } else {
    // Else we use the same queue
    _queueFamily.compute = _queueFamily.graphics;
  }

  // Dedicated transfer queue
  if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT) {
    _queueFamily.transfer = queueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
    if ((_queueFamily.transfer != _queueFamily.graphics) && (_queueFamily.transfer != _queueFamily.compute)) {
      // If transfer family index differs, we need an additional queue create info for the transfer queue
      VkDeviceQueueCreateInfo queueInfo{};
      queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueInfo.queueFamilyIndex = _queueFamily.transfer;
      queueInfo.queueCount = 1;
      queueInfo.pQueuePriorities = &defaultQueuePriority;
      queueCreateInfos.push_back(queueInfo);
    }
  } else {
    // Else we use the same queue
    _queueFamily.transfer = _queueFamily.graphics;
  }

  // Create the logical device representation
  std::vector<const char *> deviceExtensions(enabledExtensions);
  if (useSwapChain) {
    // If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }
  deviceExtensions.push_back(VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME);

  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
  deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
  deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

  // If a pNext(Chain) has been passed, we need to add it to the device creation info
  VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
  if (pNextChain) {
    physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physicalDeviceFeatures2.features = enabledFeatures;
    physicalDeviceFeatures2.pNext = pNextChain;
    deviceCreateInfo.pEnabledFeatures = nullptr;
    deviceCreateInfo.pNext = &physicalDeviceFeatures2;
  }

#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK)) && defined(VK_KHR_portability_subset)
  // SRS - When running on iOS/macOS with MoltenVK and VK_KHR_portability_subset is defined and supported by the device, enable the extension
  if (extensionSupported(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
    deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
  }
#endif

  deviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

  if (deviceExtensions.size() > 0) {
    for (const char *enabledExtension : deviceExtensions) {
      if (!extensionSupported(enabledExtension)) {
        std::cerr << "Enabled device extension \"" << enabledExtension << "\" is not present at device level\n";
      }
    }

    deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
  }

  this->enabledFeatures = enabledFeatures;

  VkResult result = vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_logicalDevice);
  if (result != VK_SUCCESS) {
    return result;
  }

  // Create a default command pool for graphics command buffers
  _commandPool = createCommandPool(_queueFamily.graphics);

  vkGetPhysicalDeviceProperties(_physicalDevice, &properties);
  vkGetPhysicalDeviceFeatures(_physicalDevice, &features);
  vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memoryProperties);


  vkCmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)vkGetDeviceProcAddr(_logicalDevice, "vkCmdPushDescriptorSetKHR");

  return result;
}

VkRenderPass VulkanDevice::createRenderPass(VkFormat color, VkFormat depth)
{
  std::array<VkAttachmentDescription, 2> attachments = {};
  attachments[0].format = color;
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  bool hasdepth = false;
  if (depth != VK_FORMAT_UNDEFINED) {
    hasdepth = true;
    attachments[1].format = depth;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }

  VkAttachmentReference colorReference = {};
  colorReference.attachment = 0;
  colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthReference = {};
  depthReference.attachment = 1;
  depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpassDescription = {};
  subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpassDescription.colorAttachmentCount = 1;
  subpassDescription.pColorAttachments = &colorReference;
  if (hasdepth)
    subpassDescription.pDepthStencilAttachment = &depthReference;
  subpassDescription.inputAttachmentCount = 0;
  subpassDescription.pInputAttachments = nullptr;
  subpassDescription.preserveAttachmentCount = 0;
  subpassDescription.pPreserveAttachments = nullptr;
  subpassDescription.pResolveAttachments = nullptr;

  std::array<VkSubpassDependency, 2> dependencies;

  dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[0].dstSubpass = 0;
  dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  dependencies[0].dependencyFlags = 0;

  dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
  dependencies[1].dstSubpass = 0;
  dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependencies[1].srcAccessMask = 0;
  dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
  dependencies[1].dependencyFlags = 0;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  if (hasdepth)
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
  else
    renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = attachments.data();
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpassDescription;
  renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
  renderPassInfo.pDependencies = dependencies.data();

  VkRenderPass renderPass = VK_NULL_HANDLE;
  VK_CHECK_RESULT(vkCreateRenderPass(_logicalDevice, &renderPassInfo, nullptr, &renderPass));

  return renderPass;
}

void VulkanDevice::destroyRenderPass(VkRenderPass rdpass)
{
  vkDestroyRenderPass(_logicalDevice, rdpass, nullptr);
}

std::tuple<VkImage, VkDeviceMemory>
VulkanDevice::createImage(int w, int h, VkFormat format)
{
  VkImageCreateInfo imageInfo = vks::initializers::imageCreateInfo();
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = format;
  imageInfo.extent.width = w;
  imageInfo.extent.height = h;

  VkImage img = VK_NULL_HANDLE;
  VK_CHECK_RESULT(vkCreateImage(_logicalDevice, &imageInfo, nullptr, &img));
  VkMemoryRequirements memReqs;
  vkGetImageMemoryRequirements(_logicalDevice, img, &memReqs);

  VkDeviceMemory mem;
  VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
  memAllocInfo.allocationSize = memReqs.size;
  memAllocInfo.memoryTypeIndex = *memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(_logicalDevice, &memAllocInfo, nullptr, &mem));
  VK_CHECK_RESULT(vkBindImageMemory(_logicalDevice, img, mem, 0));

  return std::make_tuple(img, mem);
}

VkImageView VulkanDevice::createImageView(VkImage img, VkFormat format)
{
  VkImageViewCreateInfo colorAttachmentView = {};
  colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  colorAttachmentView.pNext = NULL;
  colorAttachmentView.format = format;
  colorAttachmentView.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
  colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  colorAttachmentView.subresourceRange.baseMipLevel = 0;
  colorAttachmentView.subresourceRange.levelCount = 1;
  colorAttachmentView.subresourceRange.baseArrayLayer = 0;
  colorAttachmentView.subresourceRange.layerCount = 1;
  colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
  colorAttachmentView.flags = 0;
  colorAttachmentView.image = img;

  VkImageView view = VK_NULL_HANDLE;
  VK_CHECK_RESULT(vkCreateImageView(_logicalDevice, &colorAttachmentView, nullptr, &view));
  return view;
}

std::shared_ptr<VulkanImage> VulkanDevice::createColorImage(uint32_t width, uint32_t height, VkFormat format)
{
  auto [img, imgmem] = createImage(width, height, format);
  auto imgview = createImageView(img, format);

  auto dev = shared_from_this();
  auto vkimg = std::make_shared<VulkanImage>(dev);
  vkimg->_image = img;
  vkimg->_imageMem = imgmem;
  vkimg->_imageView = imgview;
  vkimg->_format = format;

  return vkimg;
}

std::shared_ptr<VulkanImage> VulkanDevice::createDepthImage(uint32_t width, uint32_t height, VkFormat format)
{
  VkImage img = VK_NULL_HANDLE;
  VkDeviceMemory imgmem = VK_NULL_HANDLE;
  // Create an optimal image used as the depth stencil attachment
  VkImageCreateInfo image = {};
  image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image.imageType = VK_IMAGE_TYPE_2D;
  image.format = format;
  image.extent = {width, height, 1};
  image.mipLevels = 1;
  image.arrayLayers = 1;
  image.samples = VK_SAMPLE_COUNT_1_BIT;
  image.tiling = VK_IMAGE_TILING_OPTIMAL;
  image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  VK_CHECK_RESULT(vkCreateImage(_logicalDevice, &image, nullptr, &img));

  // Allocate memory for the image (device local) and bind it to our image
  VkMemoryAllocateInfo memAlloc = {};
  memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  VkMemoryRequirements memReqs;
  vkGetImageMemoryRequirements(_logicalDevice, img, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  auto memIndex = memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (!memIndex)
    throw std::runtime_error("No proper memory type!");
  memAlloc.memoryTypeIndex = *memIndex;
  VK_CHECK_RESULT(vkAllocateMemory(_logicalDevice, &memAlloc, nullptr, &imgmem));
  VK_CHECK_RESULT(vkBindImageMemory(_logicalDevice, img, imgmem, 0));

  // Create a view for the depth stencil image
  // Images aren't directly accessed in Vulkan, but rather through views described by a subresource range
  // This allows for multiple views of one image with differing ranges (e.g. for different layers)
  VkImageViewCreateInfo depthStencilView = {};
  depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
  depthStencilView.format = format;
  depthStencilView.subresourceRange = {};
  depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  // Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT)
  if (format >= VK_FORMAT_D16_UNORM_S8_UINT)
    depthStencilView.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

  depthStencilView.subresourceRange.baseMipLevel = 0;
  depthStencilView.subresourceRange.levelCount = 1;
  depthStencilView.subresourceRange.baseArrayLayer = 0;
  depthStencilView.subresourceRange.layerCount = 1;
  depthStencilView.image = img;

  VkImageView imgview = VK_NULL_HANDLE;
  VK_CHECK_RESULT(vkCreateImageView(_logicalDevice, &depthStencilView, nullptr, &imgview));

  auto dev = shared_from_this();
  auto vkimg = std::make_shared<VulkanImage>(dev);
  vkimg->setImage(width, height, format, imgmem, img, imgview);

  return vkimg;
}

VkShaderModule VulkanDevice::createShader(const std::string &file)
{
  auto shaderSource = vks::tools::readFile(file);
  assert(!shaderSource.empty());
  return createShader(shaderSource.c_str(), shaderSource.size());
}

VkShaderModule VulkanDevice::createShader(const char *shaderSource, int n)
{
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = n;
  createInfo.pCode = (uint32_t *)shaderSource;

  VkShaderModule shaderModule;
  VK_CHECK_RESULT(vkCreateShaderModule(_logicalDevice, &createInfo, nullptr, &shaderModule));
  return shaderModule;
}

VkPipelineCache VulkanDevice::getOrCreatePipecache()
{
  if (!_pipeCache) {
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VK_CHECK_RESULT(vkCreatePipelineCache(_logicalDevice, &pipelineCacheCreateInfo, nullptr, &_pipeCache));
  }
  return _pipeCache;
}

VkDescriptorPool VulkanDevice::getOrCreateDescriptorPool()
{
  if (!_descriptorPool) {
    VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
    uint32_t sz = sizeof(poolSizes) / sizeof(VkDescriptorPoolSize);
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * sz;
    poolInfo.poolSizeCount = sz;
    poolInfo.pPoolSizes = poolSizes;
    VK_CHECK_RESULT(vkCreateDescriptorPool(_logicalDevice, &poolInfo, nullptr, &_descriptorPool));
  }
  return _descriptorPool;
}

/**
 * Get the index of a queue family that supports the requested queue flags
 * SRS - support VkQueueFlags parameter for requesting multiple flags vs. VkQueueFlagBits for a single flag only
 *
 * @param queueFlags Queue flags to find a queue family index for
 *
 * @return Index of the queue family index that matches the flags
 *
 * @throw Throws an exception if no queue family index could be found that supports the requested flags
 */
uint32_t VulkanDevice::queueFamilyIndex(VkQueueFlags queueFlags) const
{
  // Dedicated queue for compute
  // Try to find a queue family index that supports compute but not graphics
  if ((queueFlags & VK_QUEUE_COMPUTE_BIT) == queueFlags) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(_queueFamilyProperties.size()); i++) {
      if ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
        return i;
      }
    }
  }

  // Dedicated queue for transfer
  // Try to find a queue family index that supports transfer but not graphics and compute
  if ((queueFlags & VK_QUEUE_TRANSFER_BIT) == queueFlags) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(_queueFamilyProperties.size()); i++) {
      if ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
          ((_queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)) {
        return i;
      }
    }
  }

  // For other queue types or if no separate compute queue is present, return the first one to support the requested flags
  for (uint32_t i = 0; i < static_cast<uint32_t>(_queueFamilyProperties.size()); i++) {
    if ((_queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags) {
      return i;
    }
  }

  throw std::runtime_error("Could not find a matching queue family index");
}

/**
 * Get the index of a memory type that has all the requested property bits set
 *
 * @param typeBits Bit mask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
 * @param properties Bit mask of properties for the memory type to request
 * @param (Optional) memTypeFound Pointer to a bool that is set to true if a matching memory type has been found
 *
 * @return Index of the requested memory type
 *
 * @throw Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties
 */

std::optional<uint32_t> VulkanDevice::memoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const
{
  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
    if ((typeBits & 1)) {
      if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
        return i;
      }
    }
    typeBits >>= 1;
  }
  return std::nullopt;
}

/**
 * Create a buffer on the device
 *
 * @param usageFlags Usage flag bit mask for the buffer (i.e. index, vertex, uniform buffer)
 * @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
 * @param size Size of the buffer in byes
 * @param buffer Pointer to the buffer handle acquired by the function
 * @param memory Pointer to the memory handle acquired by the function
 * @param data Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
 *
 * @return VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
 */
std::shared_ptr<VulkanBuffer> VulkanDevice::createBuffer(VkBufferUsageFlags usageFlags,
  VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, void *data)
{
  auto buffer = std::make_shared<VulkanBuffer>(shared_from_this());

  VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo(usageFlags, size);
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VK_CHECK_RESULT(vkCreateBuffer(_logicalDevice, &bufferCreateInfo, nullptr, &buffer->_buffer));
  buffer->_size = size;

  VkMemoryRequirements memReqs;
  VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
  vkGetBufferMemoryRequirements(_logicalDevice, *buffer, &memReqs);
  memAlloc.allocationSize = memReqs.size;

  auto index = memoryTypeIndex(memReqs.memoryTypeBits, memoryPropertyFlags);
  if (!index) return nullptr;

  memAlloc.memoryTypeIndex = *index;
  VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
  if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
    allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
    memAlloc.pNext = &allocFlagsInfo;
  }

  VkDeviceMemory memory;
  VK_CHECK_RESULT(vkAllocateMemory(_logicalDevice, &memAlloc, nullptr, &memory));
  buffer->_memory = memory;

  if (data != nullptr) {
    void *mapped = nullptr;
    VkDeviceSize sz = memReqs.size;
    VK_CHECK_RESULT(vkMapMemory(_logicalDevice, memory, 0, sz, 0, &mapped));
    memcpy(mapped, data, size);
    // If host coherency hasn't been requested, do a manual flush to make writes visible
    if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
      VkMappedMemoryRange mappedRange = vks::initializers::mappedMemoryRange();
      mappedRange.memory = memory;
      mappedRange.offset = 0;
      mappedRange.size = sz;
      vkFlushMappedMemoryRanges(_logicalDevice, 1, &mappedRange);
    }
    vkUnmapMemory(_logicalDevice, memory);
  }

  VK_CHECK_RESULT(vkBindBufferMemory(_logicalDevice, buffer->_buffer, memory, 0));
  buffer->_memsize = memReqs.size;

  return buffer;
}

/**
 * Copy buffer data from src to dst using VkCmdCopyBuffer
 *
 * @param src Pointer to the source buffer to copy from
 * @param dst Pointer to the destination buffer to copy to
 * @param queue Pointer
 * @param copyRegion (Optional) Pointer to a copy region, if NULL, the whole buffer is copied
 *
 * @note Source and destination pointers must have the appropriate transfer usage flags set (TRANSFER_SRC / TRANSFER_DST)
 */
void VulkanDevice::copyBuffer(VulkanBuffer *src, VulkanBuffer *dst, VkQueue queue, VkBufferCopy *copyRegion)
{
  assert(dst->_size <= src->_size);
  assert(src->_buffer && dst->_buffer);
  VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
  VkBufferCopy bufferCopy{};
  if (copyRegion == nullptr) {
    bufferCopy.size = src->_size;
  } else {
    bufferCopy = *copyRegion;
  }

  vkCmdCopyBuffer(copyCmd, src->_buffer, dst->_buffer, 1, &bufferCopy);

  flushCommandBuffer(copyCmd, queue);
}

/**
 * Create a command pool for allocation command buffers from
 *
 * @param queueFamilyIndex Family index of the queue to create the command pool for
 * @param createFlags (Optional) Command pool creation flags (Defaults to VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
 *
 * @note Command buffers allocated from the created pool can only be submitted to a queue with the same family index
 *
 * @return A handle to the created command buffer
 */
VkCommandPool VulkanDevice::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags)
{
  VkCommandPoolCreateInfo cmdPoolInfo = {};
  cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
  cmdPoolInfo.flags = createFlags;
  VkCommandPool cmdPool;
  vkCreateCommandPool(_logicalDevice, &cmdPoolInfo, nullptr, &cmdPool);
  return cmdPool;
}

/**
 * Allocate a command buffer from the command pool
 *
 * @param level Level of the new command buffer (primary or secondary)
 * @param pool Command pool from which the command buffer will be allocated
 * @param (Optional) begin If true, recording on the new command buffer will be started (vkBeginCommandBuffer) (Defaults to false)
 *
 * @return A handle to the allocated command buffer
 */
VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin)
{
  VkCommandBuffer cmdBuffer;
  VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(pool, level, 1);
  VK_CHECK_RESULT(vkAllocateCommandBuffers(_logicalDevice, &cmdBufAllocateInfo, &cmdBuffer));
  // If requested, also start recording for the new command buffer
  if (begin) {
    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
  }
  return cmdBuffer;
}

VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
  return createCommandBuffer(level, _commandPool, begin);
}

/**
 * Finish command buffer recording and submit it to a queue
 *
 * @param commandBuffer Command buffer to flush
 * @param queue Queue to submit the command buffer to
 * @param pool Command pool on which the command buffer has been created
 * @param free (Optional) Free the command buffer once it has been submitted (Defaults to true)
 *
 * @note The queue that the command buffer is submitted to must be from the same family index as the pool it was allocated from
 * @note Uses a fence to ensure command buffer has finished executing
 */
void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free)
{
  if (commandBuffer == VK_NULL_HANDLE) {
    return;
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

  VkSubmitInfo submitInfo = vks::initializers::submitInfo();
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  // Create fence to ensure that the command buffer has finished executing
  VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
  VkFence fence;
  VK_CHECK_RESULT(vkCreateFence(_logicalDevice, &fenceInfo, nullptr, &fence));
  // Submit to the queue
  VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
  // Wait for the fence to signal that command buffer has finished executing
  VK_CHECK_RESULT(vkWaitForFences(_logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
  vkDestroyFence(_logicalDevice, fence, nullptr);
  if (free) {
    vkFreeCommandBuffers(_logicalDevice, pool, 1, &commandBuffer);
  }
}

void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
{
  return flushCommandBuffer(commandBuffer, queue, _commandPool, free);
}

std::vector<VkCommandBuffer> VulkanDevice::createCommandBuffers(uint32_t n)
{
  std::vector<VkCommandBuffer> cmdBufs(n);
  VkCommandBufferAllocateInfo cmdBufAllocateInfo =
      vks::initializers::commandBufferAllocateInfo(_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, n);

  VK_CHECK_RESULT(vkAllocateCommandBuffers(_logicalDevice, &cmdBufAllocateInfo, cmdBufs.data()));
  return cmdBufs;
}

void VulkanDevice::destroyCommandBuffers(std::vector<VkCommandBuffer> &cmdbufs)
{
  if (cmdbufs.empty())
    return;

  vkFreeCommandBuffers(_logicalDevice, _commandPool, static_cast<uint32_t>(cmdbufs.size()), cmdbufs.data());
  cmdbufs.clear();
}

std::vector<VkFence> VulkanDevice::createFences(uint32_t n)
{
  std::vector<VkFence> fences;
  VkFenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
  fences.resize(n);
  for (auto &fence : fences) {
    VK_CHECK_RESULT(vkCreateFence(_logicalDevice, &fenceCreateInfo, nullptr, &fence));
  }
  return fences;
}

VkQueue VulkanDevice::graphicQueue(uint32_t idx)
{
  VkQueue queue = VK_NULL_HANDLE;
  vkGetDeviceQueue(_logicalDevice, _queueFamily.graphics, idx, &queue);
  return queue;
}

VkQueue VulkanDevice::transferQueue(uint32_t idx)
{
  VkQueue queue = VK_NULL_HANDLE;
  vkGetDeviceQueue(_logicalDevice, _queueFamily.transfer, idx, &queue);
  return queue;
}

/**
 * Check if an extension is supported by the (physical device)
 *
 * @param extension Name of the extension to check
 *
 * @return True if the extension is supported (present in the list read at device creation time)
 */
bool VulkanDevice::extensionSupported(std::string extension)
{
  return (std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end());
}

/**
 * Select the best-fit depth format for this device from a list of possible depth (and stencil) formats
 *
 * @param checkSamplingSupport Check if the format can be sampled from (e.g. for shader reads)
 *
 * @return The depth format that best fits for the current device
 *
 * @throw Throws an exception if no depth format fits the requirements
 */
VkFormat VulkanDevice::supportedDepthFormat(bool checkSamplingSupport)
{
  // All depth formats may be optional, so we need to find a suitable depth format to use
  std::vector<VkFormat> depthFormats = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT,
                                        VK_FORMAT_D16_UNORM};
  for (auto &format : depthFormats) {
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(_physicalDevice, format, &formatProperties);
    // Format must support depth stencil attachment for optimal tiling
    if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      if (checkSamplingSupport) {
        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
          continue;
        }
      }
      return format;
    }
  }
  throw std::runtime_error("Could not find a matching depth format");
}
