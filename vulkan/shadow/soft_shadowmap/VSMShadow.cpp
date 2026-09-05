#include "VSMShadow.h"

#include "MeshInstance.h"
#include "ShadowPipeline.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"
#include "VulkanInitializers.hpp"
#include "VulkanPass.h"
#include "VulkanPipeline.h"
#include "VulkanTexture.h"
#include "VulkanTools.h"
#include "config.h"
#include "tmath.h"

#include <cassert>
#include <cstring>
#include <stdexcept>

#define SHADER_DIR ROOT_DIR "/vulkan/shadow/soft_shadowmap"

namespace {
class VSMPass final : public VulkanPass
{
public:
  VSMPass(const std::shared_ptr<VulkanDevice> &device, VkFormat momentsFormat)
    : VulkanPass(device)
    , _momentsFormat(momentsFormat)
  {
  }

protected:
  void initialize() override
  {
    VkAttachmentDescription attachments[2] = {};
    attachments[0].format = _momentsFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    attachments[1].format = VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    subpass.pDepthStencilAttachment = &depthReference;

    VkSubpassDependency dependencies[2] = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 2;
    createInfo.pAttachments = attachments;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 2;
    createInfo.pDependencies = dependencies;
    VK_CHECK_RESULT(vkCreateRenderPass(*_device, &createInfo, nullptr, &_renderPass));
  }

private:
  VkFormat _momentsFormat;
};

class VSMMomentsPipeline final : public VulkanPipeline
{
public:
  VSMMomentsPipeline(const std::shared_ptr<VulkanDevice> &device, uint32_t size)
    : VulkanPipeline(device)
    , _size(size)
  {
  }

  void realize(VulkanPass *renderPass, int subpass = 0) override
  {
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState blendAttachment = {};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

    VkViewport viewport = {};
    viewport.y = static_cast<float>(_size);
    viewport.width = static_cast<float>(_size);
    viewport.height = -static_cast<float>(_size);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.extent = {_size, _size};

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.stride = sizeof(tg::vec3);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute = {};
    attribute.location = 0;
    attribute.binding = 0;
    attribute.format = VK_FORMAT_R32G32B32_SFLOAT;

    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 1;
    vertexInput.pVertexAttributeDescriptions = &attribute;

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = _device->createShader(SHADER_DIR "/vsm_moments.vert.spv");
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = _device->createShader(SHADER_DIR "/vsm_moments.frag.spv");
    stages[1].pName = "main";

    VkGraphicsPipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.layout = pipeLayout();
    createInfo.renderPass = *renderPass;
    createInfo.subpass = subpass;
    createInfo.stageCount = 2;
    createInfo.pStages = stages;
    createInfo.pInputAssemblyState = &inputAssembly;
    createInfo.pRasterizationState = &rasterization;
    createInfo.pColorBlendState = &colorBlend;
    createInfo.pViewportState = &viewportState;
    createInfo.pDepthStencilState = &depthStencil;
    createInfo.pMultisampleState = &multisample;
    createInfo.pVertexInputState = &vertexInput;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(*_device, _device->getOrCreatePipecache(), 1, &createInfo, nullptr, &_pipeline));

    vkDestroyShaderModule(*_device, stages[0].module, nullptr);
    vkDestroyShaderModule(*_device, stages[1].module, nullptr);
  }

protected:
  VkPipelineLayout createPipeLayout() override
  {
    VkDescriptorSetLayout layout = matrixLayout();

    VkPushConstantRange pushConstant = {};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.size = sizeof(Transform);

    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.setLayoutCount = 1;
    createInfo.pSetLayouts = &layout;
    createInfo.pushConstantRangeCount = 1;
    createInfo.pPushConstantRanges = &pushConstant;

    VkPipelineLayout layoutHandle = VK_NULL_HANDLE;
    VK_CHECK_RESULT(vkCreatePipelineLayout(*_device, &createInfo, nullptr, &layoutHandle));
    return layoutHandle;
  }

private:
  uint32_t _size;
};
} // namespace

VSMShadow::VSMShadow(const std::shared_ptr<VulkanDevice> &device)
  : _device(device)
{
  constexpr VkFormat candidates[] = {VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R16G16_SFLOAT};
  for (VkFormat format : candidates) {
    VkFormatProperties properties = {};
    vkGetPhysicalDeviceFormatProperties(device->physicalDevice(), format, &properties);
    constexpr VkFormatFeatureFlags required =
      VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    if ((properties.optimalTilingFeatures & required) == required) {
      _momentsFormat = format;
      break;
    }
  }
  if (_momentsFormat == VK_FORMAT_UNDEFINED)
    throw std::runtime_error("No filterable VSM moments format is supported");

  _lightingPipeline = std::make_shared<ShadowPipeline>(device, "vsm_shadow.frag.spv");
  _casterPipeline = std::make_shared<VSMMomentsPipeline>(device, MapSize);
  _pass = std::make_shared<VSMPass>(device, _momentsFormat);
  _momentsImage = device->createColorImage(MapSize, MapSize, _momentsFormat);
  _depthImage = device->createDepthImage(MapSize, MapSize, VK_FORMAT_D32_SFLOAT);
}

VSMShadow::~VSMShadow()
{
  destroyFrameBuffers();
  if (_descriptorPool) {
    VkDescriptorSet sets[] = {_depthMatrixSet, _shadowSet};
    vkFreeDescriptorSets(*_device, _descriptorPool, 2, sets);
  }
  if (_sampler)
    vkDestroySampler(*_device, _sampler, nullptr);
}

VkDescriptorSetLayout VSMShadow::matrixLayout() const
{
  return _lightingPipeline->matrixLayout();
}
VkDescriptorSetLayout VSMShadow::lightLayout() const
{
  return _lightingPipeline->lightLayout();
}
VkPipelineLayout VSMShadow::lightingPipelineLayout() const
{
  return _lightingPipeline->pipeLayout();
}
VulkanTexture *VSMShadow::debugTexture() const
{
  return _texture.get();
}

void VSMShadow::initializeUniforms()
{
  auto lightPosition = tg::vec3(10, 10, 0);
  _depthMatrixBuffer =
    _device->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(MVP));
  _depthMatrix.view = tg::lookat(lightPosition, tg::vec3(0, 0, 0), tg::vec3(0, 1, 0));
  _depthMatrix.prj = tg::ortho<float>(-5, 5, -5, 5, 0.1, 20);
  writeBuffer(*_depthMatrixBuffer, _depthMatrix);

  _shadowBuffer =
    _device->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(ShadowMatrix));
  _shadowMatrix.light = tg::normalize(lightPosition);
  _shadowMatrix.light.w() = _filterMode;
  _shadowMatrix.view = _depthMatrix.view;
  _shadowMatrix.prj = _depthMatrix.prj;
  _shadowMatrix.mvp = _depthMatrix.prj * _depthMatrix.view;
  _shadowMatrix.options = tg::vec4(0.00002f, 0.2f, 0.0f, 0.0f);
  writeBuffer(*_shadowBuffer, _shadowMatrix);
}

void VSMShadow::realize(VulkanPass *renderPass, VkDescriptorPool descriptorPool)
{
  _descriptorPool = descriptorPool;
  _casterPipeline->realize(_pass.get());
  _lightingPipeline->realize(renderPass);

  _depthMatrixSet = allocateSet(descriptorPool, _casterPipeline->matrixLayout());
  writeUniformDescriptor(_depthMatrixSet, 0, *_depthMatrixBuffer, sizeof(MVP));
  _shadowSet = allocateSet(descriptorPool, _lightingPipeline->shadowLayout());
  writeUniformDescriptor(_shadowSet, 0, *_shadowBuffer, sizeof(ShadowMatrix));

  _texture = std::make_shared<VulkanTexture>();
  _texture->realize(_momentsImage);
  createSampler();

  VkDescriptorImageInfo imageInfo = {};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = _momentsImage->imageView();
  imageInfo.sampler = _sampler;

  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = _shadowSet;
  write.dstBinding = 1;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &imageInfo;
  vkUpdateDescriptorSets(*_device, 1, &write, 0, nullptr);
}

void VSMShadow::createFrameBuffers(uint32_t count)
{
  destroyFrameBuffers();
  VkImageView attachments[] = {_momentsImage->imageView(), _depthImage->imageView()};

  VkFramebufferCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  createInfo.renderPass = *_pass;
  createInfo.attachmentCount = 2;
  createInfo.pAttachments = attachments;
  createInfo.width = MapSize;
  createInfo.height = MapSize;
  createInfo.layers = 1;

  _frameBuffers.resize(count);
  for (auto &frameBuffer : _frameBuffers)
    VK_CHECK_RESULT(vkCreateFramebuffer(*_device, &createInfo, nullptr, &frameBuffer));
}

void VSMShadow::destroyFrameBuffers()
{
  for (auto frameBuffer : _frameBuffers)
    vkDestroyFramebuffer(*_device, frameBuffer, nullptr);
  _frameBuffers.clear();
}

void VSMShadow::recordShadowPass(VkCommandBuffer cmdBuf, uint32_t frameIndex, MeshInstance &model)
{
  assert(frameIndex < _frameBuffers.size());
  VkClearValue clearValues[2] = {};
  clearValues[0].color = {{1.0f, 1.0f, 0.0f, 0.0f}};
  clearValues[1].depthStencil = {1.0f, 0};

  VkRenderPassBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  beginInfo.renderPass = *_pass;
  beginInfo.framebuffer = _frameBuffers[frameIndex];
  beginInfo.renderArea.extent = {MapSize, MapSize};
  beginInfo.clearValueCount = 2;
  beginInfo.pClearValues = clearValues;
  vkCmdBeginRenderPass(cmdBuf, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

  tg::mat4 transform;
  transform.identity();
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_casterPipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _casterPipeline->pipeLayout(), 0, 1, &_depthMatrixSet, 0, nullptr);
  vkCmdPushConstants(cmdBuf, _casterPipeline->pipeLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Transform), &transform);
  model.buildCommandBuffer(cmdBuf, _casterPipeline->pipeLayout());
  vkCmdEndRenderPass(cmdBuf);
}

void VSMShadow::bindLighting(VkCommandBuffer cmdBuf, VkDescriptorSet matrixSet, VkDescriptorSet lightSet)
{
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_lightingPipeline);
  VkDescriptorSet commonSets[2] = {matrixSet, lightSet};
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _lightingPipeline->pipeLayout(), 0, 2, commonSets, 0, nullptr);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _lightingPipeline->pipeLayout(), 3, 1, &_shadowSet, 0, nullptr);
}

void VSMShadow::toggleFilterMode()
{
  _filterMode = _filterMode >= 0.5f ? 0.0f : 1.0f;
  _shadowMatrix.light.w() = _filterMode;
  writeBuffer(*_shadowBuffer, _shadowMatrix);
}

bool VSMShadow::valid() const
{
  return _casterPipeline->valid() && _lightingPipeline->valid();
}

template <typename T>
void VSMShadow::writeBuffer(VulkanBuffer &buffer, const T &value)
{
  void *data = nullptr;
  VK_CHECK_RESULT(vkMapMemory(*_device, buffer.memory(), 0, sizeof(T), 0, &data));
  memcpy(data, &value, sizeof(T));
  vkUnmapMemory(*_device, buffer.memory());
}

VkDescriptorSet VSMShadow::allocateSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout layout)
{
  VkDescriptorSet set = VK_NULL_HANDLE;
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;
  VK_CHECK_RESULT(vkAllocateDescriptorSets(*_device, &allocInfo, &set));
  return set;
}

void VSMShadow::writeUniformDescriptor(VkDescriptorSet set, uint32_t binding, VulkanBuffer &buffer, VkDeviceSize size)
{
  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = buffer;
  bufferInfo.range = size;

  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = set;
  write.dstBinding = binding;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  write.pBufferInfo = &bufferInfo;
  vkUpdateDescriptorSets(*_device, 1, &write, 0, nullptr);
}

void VSMShadow::createSampler()
{
  VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;
  VK_CHECK_RESULT(vkCreateSampler(*_device, &samplerInfo, nullptr, &_sampler));
}
