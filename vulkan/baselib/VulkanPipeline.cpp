#include "VulkanPipeline.h"
#include "VulkanPass.h"
#include "VulkanTools.h"

#include "config.h"
#include "tvec.h"
#include "RenderData.h"

using tg::vec3;
using tg::vec4;

#define SHADER_DIR ROOT_DIR##"/vulkan/baselib/shaders"

VulkanPipeline::VulkanPipeline(const std::shared_ptr<VulkanDevice>& dev) : _device(dev)
{
}

VulkanPipeline::~VulkanPipeline()
{
  if(_matrixLayout) {
    vkDestroyDescriptorSetLayout(*_device, _matrixLayout, nullptr);
    _matrixLayout = VK_NULL_HANDLE;
  }

  if(_pipeline) {
    vkDestroyPipeline(*_device, _pipeline, nullptr);
    _pipeline = VK_NULL_HANDLE;
  }

  if(_pipeLayout) {
    vkDestroyPipelineLayout(*_device, _pipeLayout, nullptr);
    _pipeLayout = VK_NULL_HANDLE;
  }
}

VkDescriptorSetLayout VulkanPipeline::matrixLayout()
{
  if (!_matrixLayout) {
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBinding.pImmutableSamplers = nullptr;
    layoutBinding.binding = 0;

    VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.pNext = nullptr;
    descriptorLayout.bindingCount = 1;
    descriptorLayout.pBindings = &layoutBinding;

    VkDescriptorSetLayout deslay = VK_NULL_HANDLE;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*_device, &descriptorLayout, nullptr, &deslay));
    _matrixLayout = deslay;
  }
  return _matrixLayout;
}

VkPipelineLayout VulkanPipeline::pipeLayout()
{
  if (!_pipeLayout) {
    _pipeLayout = createPipeLayout();
  }
  return _pipeLayout;
}
