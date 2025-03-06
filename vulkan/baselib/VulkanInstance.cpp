#include "VulkanInstance.h"

#include <Windows.h>

#include <vector>
#include <string>
#include <iostream>
#include <cstring>

#include <vulkan/vulkan.h> 
#include <vulkan/vulkan_win32.h>

#include "VulkanDebug.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"


VulkanInstance &VulkanInstance::instance()
{
  static VulkanInstance instance;
  return instance;
}

VulkanInstance::~VulkanInstance()
{
  if(_instance) {
    vks::debug::freeDebugCallback(_instance);
    vkDestroyInstance(_instance, nullptr);
  }
}

void VulkanInstance::enable_debug()
{
  if (!_instance)
    return;

  vks::debug::setupDebugging(_instance);
}

std::shared_ptr<VulkanDevice> 
VulkanInstance::create_device(const std::string &devStr) 
{
  uint32_t gpuCount = 0;
  if(vkEnumeratePhysicalDevices(_instance, &gpuCount, nullptr) != VK_SUCCESS) {
    throw std::runtime_error("Could not find any gpu.");
  }
  if (gpuCount == 0) {
    throw std::runtime_error("No device with Vulkan support found.");
  }

  std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
  if(vkEnumeratePhysicalDevices(_instance, &gpuCount, physicalDevices.data()) != VK_SUCCESS) {
    throw std::runtime_error("Could not enumerate physical devices.");
  }

  uint32_t selectedDevice = 0;
  VkPhysicalDeviceProperties deviceProperties;
  if (!devStr.empty()) {
    for (int i = 0; i < gpuCount; i++) {
      vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
      if (strstr(deviceProperties.deviceName, devStr.c_str()))
      {
        selectedDevice = i;
        break;
      }
    }
  }

  auto &phyDev = physicalDevices[selectedDevice];

  VkPhysicalDeviceFeatures features = {};
  std::vector<const char *> extension;
  auto dev = std::make_shared<VulkanDevice>(phyDev);
  dev->realize(features, extension, nullptr);
  return dev;
}

VulkanInstance::VulkanInstance() 
{
  initialize();
}

void VulkanInstance::initialize() 
{
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "demo";
  appInfo.pEngineName = "demo";
  appInfo.apiVersion = VK_API_VERSION_1_3;

  std::vector<const char*> instanceExtensions = {VK_KHR_SURFACE_EXTENSION_NAME};
  instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

  uint32_t extCount = 0;
  std::vector<std::string> supportedExt;
  vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
  if (extCount > 0) {
    std::vector<VkExtensionProperties> extensions(extCount);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS) {
      for (auto ext : extensions)
        supportedExt.emplace_back(ext.extensionName);
    }
  }
  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pNext = NULL;
  instanceCreateInfo.pApplicationInfo = &appInfo;

  instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  //instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

  if (instanceExtensions.size() > 0) {
    instanceCreateInfo.enabledExtensionCount = instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
  }

  const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
  uint32_t instanceLayerCount;
  vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
  std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
  vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());

  bool validationLayerPresent = false;
  for (VkLayerProperties layer : instanceLayerProperties) {
    if (strcmp(layer.layerName, validationLayerName) == 0) {
      validationLayerPresent = true;
      break;
    }
  }
  if (validationLayerPresent) {
    instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
    instanceCreateInfo.enabledLayerCount = 1;
  } else {
    std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
  }

  auto result = vkCreateInstance(&instanceCreateInfo, nullptr, &_instance);

  if (std::find(supportedExt.begin(), supportedExt.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedExt.end())
    vks::debugutils::setup(_instance); 
}
