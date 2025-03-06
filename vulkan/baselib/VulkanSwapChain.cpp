#include "VulkanSwapChain.h"

#include "VulkanDevice.h"
#include "VulkanInstance.h"
#include "VulkanTools.h"

#include <array>

VulkanSwapChain::VulkanSwapChain(const std::shared_ptr<VulkanDevice> &dev)
  : _device(dev)
{
  //fpGetPhysicalDeviceSurfaceSupportKHR =
  //    reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
  //fpGetPhysicalDeviceSurfaceCapabilitiesKHR =
  //    reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
  //fpGetPhysicalDeviceSurfaceFormatsKHR =
  //    reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
  //fpGetPhysicalDeviceSurfacePresentModesKHR =
  //    reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));

  //fpCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(vkGetDeviceProcAddr(*_device, "vkCreateSwapchainKHR"));
  //fpDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(vkGetDeviceProcAddr(*_device, "vkDestroySwapchainKHR"));
  //fpGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(vkGetDeviceProcAddr(*_device, "vkGetSwapchainImagesKHR"));
  //fpAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetDeviceProcAddr(*_device, "vkAcquireNextImageKHR"));
  //fpQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(*_device, "vkQueuePresentKHR"));
}

VulkanSwapChain::~VulkanSwapChain() 
{
  if (_swapChain != VK_NULL_HANDLE) {
    for (uint32_t i = 0; i < _images.size(); i++) {
      vkDestroyImageView(*_device, _images[i].view, nullptr);
    }
  }

  if(_swapChain)
    vkDestroySwapchainKHR(*_device, _swapChain, nullptr);
  _swapChain = VK_NULL_HANDLE;

  if (_surface)
    vkDestroySurfaceKHR(VulkanInstance::instance(), _surface, nullptr);
  _surface = VK_NULL_HANDLE;
}

void VulkanSwapChain::set_surface(VkSurfaceKHR surface) 
{
  _surface = surface;

  auto &props = _device->queue_family_properties();
  std::vector<VkBool32> supportsPresent(props.size());
  for (int i = 0; i < props.size(); i++)
    vkGetPhysicalDeviceSurfaceSupportKHR(_device->physical_device(), i, surface, &supportsPresent[i]);

  uint32_t graphicIndex = UINT32_MAX;
  uint32_t presentIndex = UINT32_MAX;
  for(int i = 0; i < supportsPresent.size(); i++) {
    if(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      if (graphicIndex == UINT32_MAX)
        graphicIndex = i;
      if(supportsPresent[i]) {
        graphicIndex = i;
        presentIndex = i;
        break;
      }
    }
  }
  if(presentIndex == UINT32_MAX) {
    for(int i = 0; i < supportsPresent.size(); i++) {
      if (supportsPresent[i]) {
        presentIndex = i;
        break;
      }
    }
  }

  if (graphicIndex == UINT32_MAX || presentIndex == UINT32_MAX)
    throw std::runtime_error("Could not find a graphic or present queue!");

  if (graphicIndex != presentIndex)
    throw std::runtime_error("Separate graphic and present queue are not supported yet !");

  _queueIndex = graphicIndex;

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(_device->physical_device(), surface, &formatCount, 0);
  std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(_device->physical_device(), surface, &formatCount, surfaceFormats.data());

  if(formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
    _color_format = VK_FORMAT_B8G8R8A8_UNORM;
  }else{
    _color_format = VK_FORMAT_UNDEFINED;
    for(auto &surfaceFormat : surfaceFormats) {
      if(surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM) {
        _color_format = surfaceFormat.format;
        _color_space = surfaceFormat.colorSpace;
        break;
      }
    }
    if(_color_format == VK_FORMAT_UNDEFINED) {
      _color_format = surfaceFormats[0].format;
      _color_space = surfaceFormats[0].colorSpace;
    }
  }
}

void VulkanSwapChain::realize(uint32_t width, uint32_t height, bool vsync, bool fullscreen) 
{
  _width = width; _height = height;
  vkDeviceWaitIdle(*_device);

  VkSwapchainKHR oldchain = _swapChain;

  VkSurfaceCapabilitiesKHR surfaceCaps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_device->physical_device(), _surface, &surfaceCaps);

  uint32_t presetModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(_device->physical_device(), _surface, &presetModeCount, nullptr);
  std::vector<VkPresentModeKHR> presentModes(presetModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(_device->physical_device(), _surface, &presetModeCount, presentModes.data());

  VkExtent2D chainExtent = {};
  chainExtent.width = width;
  chainExtent.height = height;

  VkPresentModeKHR chainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

  if(!vsync) {
    for(int i = 0; i < presetModeCount; i++) {
      if(presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
        chainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        break;
      }
      if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        chainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
      }
    }
  }

  uint32_t desireImages = surfaceCaps.minImageCount + 1;
  if (surfaceCaps.maxImageCount > 0 && desireImages > surfaceCaps.maxImageCount)
    desireImages = surfaceCaps.maxImageCount;

  VkSurfaceTransformFlagsKHR preTransform;
  if (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
    // We prefer a non-rotated transform
    preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  } else {
    preTransform = surfaceCaps.currentTransform;
  }

  // Find a supported composite alpha format (not all devices support alpha opaque)
  VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  // Simply select the first composite alpha format available
  std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
  };
  for (auto& compositeAlphaFlag : compositeAlphaFlags) {
    if (surfaceCaps.supportedCompositeAlpha & compositeAlphaFlag) {
      compositeAlpha = compositeAlphaFlag;
      break;
    };
  }

  VkSwapchainCreateInfoKHR swapchainCI = {};
  swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainCI.surface = _surface;
  swapchainCI.minImageCount = desireImages;
  swapchainCI.imageFormat = _color_format;
  swapchainCI.imageColorSpace = _color_space;
  swapchainCI.imageExtent = {chainExtent.width, chainExtent.height};
  swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
  swapchainCI.imageArrayLayers = 1;
  swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainCI.queueFamilyIndexCount = 0;
  swapchainCI.presentMode = chainPresentMode;
  // Setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse 
  // and makes sure that we can still present already acquired images
  swapchainCI.oldSwapchain = oldchain;
  // Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
  swapchainCI.clipped = VK_TRUE;
  swapchainCI.compositeAlpha = compositeAlpha;

  // Enable transfer source on swap chain images if supported
  if (surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
    swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  // Enable transfer destination on swap chain images if supported
  if (surfaceCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
    swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }

  vkCreateSwapchainKHR(*_device, &swapchainCI, nullptr, &_swapChain);

  // If an existing swap chain is re-created, destroy the old swap chain
  // This also cleans up all the presentable images
  if (oldchain != VK_NULL_HANDLE) {
    for (uint32_t i = 0; i < _images.size(); i++) {
      vkDestroyImageView(*_device, _images[i].view, nullptr);
    }
    vkDestroySwapchainKHR(*_device, oldchain, nullptr);
  }

  uint32_t imageCount;
  VK_CHECK_RESULT(vkGetSwapchainImagesKHR(*_device, _swapChain, &imageCount, NULL));
  std::vector<VkImage> images(imageCount);
  VK_CHECK_RESULT(vkGetSwapchainImagesKHR(*_device, _swapChain, &imageCount, images.data()));

  // Get the swap chain buffers containing the image and imageview
  _images.resize(imageCount);
  for (uint32_t i = 0; i < imageCount; i++) {
    VkImageViewCreateInfo colorAttachmentView = {};
    colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorAttachmentView.pNext = NULL;
    colorAttachmentView.format = _color_format;
    colorAttachmentView.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorAttachmentView.subresourceRange.baseMipLevel = 0;
    colorAttachmentView.subresourceRange.levelCount = 1;
    colorAttachmentView.subresourceRange.baseArrayLayer = 0;
    colorAttachmentView.subresourceRange.layerCount = 1;
    colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorAttachmentView.flags = 0;

    _images[i].image = images[i];

    colorAttachmentView.image = _images[i].image;

    VK_CHECK_RESULT(vkCreateImageView(*_device, &colorAttachmentView, nullptr, &_images[i].view));
  }
}

std::vector<VkFramebuffer> VulkanSwapChain::create_frame_buffer(VkRenderPass vkPass, const VkImageView& depth)
{
  std::vector<VkImageView> imgviews;
  for (auto& img : _images)
    imgviews.push_back(img.view);
  return create_frame_buffer(vkPass, imgviews, depth);
}

std::vector<VkFramebuffer> VulkanSwapChain::create_frame_buffer(VkRenderPass vkPass, const std::vector<VkImageView>& color, const VkImageView& depth)
{
  assert(_images.size() == color.size());

  VkImageView attachments[2];

  // Depth/Stencil attachment is the same for all frame buffers
  attachments[1] = depth;

  VkFramebufferCreateInfo frameBufferCreateInfo = {};
  frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  frameBufferCreateInfo.pNext = NULL;
  frameBufferCreateInfo.renderPass = vkPass;
  if (depth == VK_NULL_HANDLE)
    frameBufferCreateInfo.attachmentCount = 1;
  else
    frameBufferCreateInfo.attachmentCount = 2;
  frameBufferCreateInfo.pAttachments = attachments;
  frameBufferCreateInfo.width = _width;
  frameBufferCreateInfo.height = _height;
  frameBufferCreateInfo.layers = 1;

  std::vector<VkFramebuffer> frameBuffers;
  frameBuffers.resize(color.size());
  for (uint32_t i = 0; i < frameBuffers.size(); i++) {
    attachments[0] = color[i];
    VK_CHECK_RESULT(vkCreateFramebuffer(*_device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
  }

  return frameBuffers;
}

std::tuple<VkResult, uint32_t> VulkanSwapChain::acquire_image(VkSemaphore present_sema)
{
  uint32_t index;
  auto result = vkAcquireNextImageKHR(*_device, _swapChain, UINT64_MAX, present_sema, nullptr, &index);
  return {result, index};
}

VkResult VulkanSwapChain::queue_present(VkQueue queue, uint32_t index, VkSemaphore wait_sema)
{
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = NULL;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &_swapChain;
  presentInfo.pImageIndices = &index;
  // Check if a wait semaphore has been specified to wait for before presenting the image
  if (wait_sema != VK_NULL_HANDLE) {
    presentInfo.pWaitSemaphores = &wait_sema;
    presentInfo.waitSemaphoreCount = 1;
  }
  return vkQueuePresentKHR(queue, &presentInfo);
}
