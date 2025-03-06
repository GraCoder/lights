#ifndef __VULKAN_INSTANCE_INC__
#define __VULKAN_INSTANCE_INC__

#include "vulkan/vulkan_core.h"

#include <string>
#include <memory>

class VulkanDevice;
class VulkanSwapChain;

class VulkanInstance {
public:
  static VulkanInstance &instance();

  ~VulkanInstance();

  operator VkInstance() { return _instance; } 

  void enable_debug();

  std::shared_ptr<VulkanDevice> create_device(const std::string &dev = "");

private:

  VulkanInstance();

  void initialize();


  VkInstance _instance = nullptr;
};

#endif