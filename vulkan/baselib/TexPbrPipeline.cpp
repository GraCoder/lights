#include "TexPBRPipeline.h"
#include "VulkanPass.h"
#include "VulkanTools.h"

#include "tvec.h"
#include "config.h"
#include "RenderData.h"

using tg::vec2;
using tg::vec3;

#define SHADER_DIR ROOT_DIR##"/vulkan/baselib/shaders"

TexPBRPipeline::TexPBRPipeline(const std::shared_ptr<VulkanDevice> &dev) : PBRPipeline(dev)
{
}

TexPBRPipeline::~TexPBRPipeline()
{
  if (_textureLayout) {
    vkDestroyDescriptorSetLayout(*_device, _textureLayout, nullptr);
    _textureLayout = VK_NULL_HANDLE;
  }
}

VkDescriptorSetLayout TexPBRPipeline::textureLayout()
{
  if (!_textureLayout) {
    VkDescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBinding.binding = 0;
    layoutBinding.descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    layoutInfo.pBindings = &layoutBinding;
    layoutInfo.bindingCount = 1;

    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*_device, &layoutInfo, nullptr, &_textureLayout));
  }
  return _textureLayout;
}

void TexPBRPipeline::realize(VulkanPass *renderPass, int subpass)
{
  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.layout = pipeLayout();
  pipelineCreateInfo.renderPass = *renderPass;

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
  inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineRasterizationStateCreateInfo rasterizationState = {};
  rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizationState.lineWidth = 1.0f;

  VkPipelineColorBlendAttachmentState blendAttachmentState = {};
  blendAttachmentState.colorWriteMask = 0xf;
  VkPipelineColorBlendStateCreateInfo colorBlendState = {};
  colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlendState.attachmentCount = 1;
  colorBlendState.pAttachments = &blendAttachmentState;

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.pDynamicStates = dynamicStates;
  dynamicState.dynamicStateCount = 2;

  VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
  depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilState.depthTestEnable = VK_TRUE;
  depthStencilState.depthWriteEnable = VK_TRUE;
  depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

  VkPipelineMultisampleStateCreateInfo multisampleState = {};
  multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkVertexInputBindingDescription bindings[3] = {};
  bindings[0] = {0, sizeof(vec3), VK_VERTEX_INPUT_RATE_VERTEX};
  bindings[1] = {1, sizeof(vec3), VK_VERTEX_INPUT_RATE_VERTEX};
  bindings[2] = {2, sizeof(vec2), VK_VERTEX_INPUT_RATE_VERTEX};

  VkVertexInputAttributeDescription attributes[3] = {};
  attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
  attributes[1] = {1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0};
  attributes[2] = {2, 2, VK_FORMAT_R32G32_SFLOAT, 0};

  VkPipelineVertexInputStateCreateInfo vertexInputState = {};
  vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputState.vertexBindingDescriptionCount = 3;
  vertexInputState.pVertexBindingDescriptions = bindings;
  vertexInputState.vertexAttributeDescriptionCount = 3;
  vertexInputState.pVertexAttributeDescriptions = attributes;

  VkPipelineShaderStageCreateInfo shaderStages[2] = {};
  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = _device->createShader(SHADER_DIR "/pbr_tex.vert.spv");
  shaderStages[0].pName = "main";
  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = _device->createShader(SHADER_DIR "/pbr_tex.frag.spv");
  shaderStages[1].pName = "main";

  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = shaderStages;
  pipelineCreateInfo.pVertexInputState = &vertexInputState;
  pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
  pipelineCreateInfo.pRasterizationState = &rasterizationState;
  pipelineCreateInfo.pColorBlendState = &colorBlendState;
  pipelineCreateInfo.pMultisampleState = &multisampleState;
  pipelineCreateInfo.pViewportState = &viewportState;
  pipelineCreateInfo.pDepthStencilState = &depthStencilState;
  pipelineCreateInfo.pDynamicState = &dynamicState;
  pipelineCreateInfo.subpass = subpass;

  VK_CHECK_RESULT(vkCreateGraphicsPipelines(*_device, _device->getOrCreatePipecache(), 1, &pipelineCreateInfo, nullptr, &_pipeline));
  vkDestroyShaderModule(*_device, shaderStages[0].module, nullptr);
  vkDestroyShaderModule(*_device, shaderStages[1].module, nullptr);
}

VkPipelineLayout TexPBRPipeline::createPipeLayout()
{
  VkDescriptorSetLayout layouts[] = {matrixLayout(), lightLayout(), pbrLayout(), textureLayout()};

  VkPushConstantRange transformConstants = {};
  transformConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  transformConstants.size = sizeof(Transform);

  VkPipelineLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 4;
  layoutInfo.pSetLayouts = layouts;
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &transformConstants;

  VkPipelineLayout layout = VK_NULL_HANDLE;
  VK_CHECK_RESULT(vkCreatePipelineLayout(*_device, &layoutInfo, nullptr, &layout));
  return layout;
}
