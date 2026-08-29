#ifndef __VULKAN_SWAP_CHAIN_INC__
#define __VULkAN_SWAP_CHAIN_INC__

#include <memory>
#include <vector>
#include "vulkan/vulkan.h"

class VulkanInstance;
class VulkanDevice;
class VulkanImage;

class VulkanSwapChain{
public:
  VulkanSwapChain(const std::shared_ptr<VulkanDevice> &dev);
  ~VulkanSwapChain();

  VkSurfaceKHR surface() { return _surface; };

  void setSurface(VkSurfaceKHR surface);

  VkFormat colorFormat() { return _colorFormat; }

  void realize(uint32_t width, uint32_t height, bool vsync, bool fullscreen = false);

  uint32_t imageCount() { return _images.size(); }

  VkImageView imageView(int idx) { return _images[idx].view; }

  VkImageView depthImageView(int idx);

  std::vector<VkFramebuffer> createFrameBuffer(VkRenderPass vkPass);

  std::vector<VkFramebuffer> createFrameBuffer(VkRenderPass vkPass, const VkImageView &depth);

  std::vector<VkFramebuffer> createFrameBuffer(VkRenderPass vkPass, const std::vector<VkImageView> &color, const VkImageView &depth);

  std::tuple<VkResult, uint32_t> acquireImage(VkSemaphore presentSema);

  VkResult queuePresent(VkQueue queue, uint32_t index, VkSemaphore waitSema = VK_NULL_HANDLE);

private:
  std::shared_ptr<VulkanDevice>    _device;

  VkSurfaceKHR _surface = VK_NULL_HANDLE;
  uint32_t _queueIndex = UINT32_MAX;
  VkFormat _colorFormat;
  VkColorSpaceKHR _colorSpace;
  VkSwapchainKHR _swapChain = VK_NULL_HANDLE;

  uint32_t _width = 0, _height = 0;

  struct SwapChainImage{
    VkImage image;
    VkImageView view;
  };
  std::vector<SwapChainImage> _images;
  std::vector<std::shared_ptr<VulkanImage>> _depthImages;

  //PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
  //PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
  //PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
  //PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
  //PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
  //PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
  //PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
  //PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
  //PFN_vkQueuePresentKHR fpQueuePresentKHR;
};


#endif
