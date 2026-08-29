#include "ShadowPipeline.h"
#include "VulkanPass.h"
#include "VulkanTools.h"
#include "RenderData.h"


#include "config.h"
#include "tvec.h"

using tg::vec3;
using tg::vec2;

#define SHADER_DIR ROOT_DIR##"/vulkan/shadow/soft_shadowmap"

ShadowPipeline::ShadowPipeline(const std::shared_ptr<VulkanDevice> &dev, std::string fragmentShader)
  : TexturePipeline(dev), _fragmentShader(std::move(fragmentShader))
{
}

ShadowPipeline::~ShadowPipeline()
{
  if (_lightLayout)
    vkDestroyDescriptorSetLayout(*_device, _lightLayout, 0);
  if (_shadowLayout)
    vkDestroyDescriptorSetLayout(*_device, _shadowLayout, 0);
}

void ShadowPipeline::realize(VulkanPass *renderPass, int subpass)
{
  auto pipeLay = pipeLayout();

  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.layout = pipeLay;
  pipelineCreateInfo.renderPass = *renderPass;

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
  inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineRasterizationStateCreateInfo rasterizationState = {};
  rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizationState.depthClampEnable = VK_FALSE;
  rasterizationState.rasterizerDiscardEnable = VK_FALSE;
  rasterizationState.depthBiasEnable = VK_FALSE;
  rasterizationState.lineWidth = 1.0f;

  VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
  blendAttachmentState[0].colorWriteMask = 0xf;
  blendAttachmentState[0].blendEnable = VK_FALSE;
  VkPipelineColorBlendStateCreateInfo colorBlendState = {};
  colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlendState.attachmentCount = 1;
  colorBlendState.pAttachments = blendAttachmentState;

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  std::vector<VkDynamicState> dynamicStateEnables;
  dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.pDynamicStates = dynamicStateEnables.data();
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

  VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
  depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilState.depthTestEnable = VK_TRUE;
  depthStencilState.depthWriteEnable = VK_TRUE;
  depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
  depthStencilState.depthBoundsTestEnable = VK_FALSE;
  depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
  depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
  depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
  depthStencilState.stencilTestEnable = VK_FALSE;
  depthStencilState.front = depthStencilState.back;

  VkPipelineMultisampleStateCreateInfo multisampleState = {};
  multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampleState.pSampleMask = nullptr;

  VkVertexInputBindingDescription vertexInputBindings[3] = {};
  vertexInputBindings[0].binding = 0;  // vkCmdBindVertexBuffers
  vertexInputBindings[0].stride = sizeof(vec3);
  vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  vertexInputBindings[1].binding = 1;  // vkCmdBindVertexBuffers
  vertexInputBindings[1].stride = sizeof(vec3);
  vertexInputBindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  vertexInputBindings[2].binding = 2;
  vertexInputBindings[2].stride = sizeof(vec2);
  vertexInputBindings[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vertexInputAttributs[3] = {};
  vertexInputAttributs[0].binding = 0;
  vertexInputAttributs[0].location = 0;
  vertexInputAttributs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertexInputAttributs[0].offset = 0;
  vertexInputAttributs[1].binding = 1;
  vertexInputAttributs[1].location = 1;
  vertexInputAttributs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  vertexInputAttributs[1].offset = 0;
  vertexInputAttributs[2].binding = 2;
  vertexInputAttributs[2].location = 2;
  vertexInputAttributs[2].format = VK_FORMAT_R32G32_SFLOAT;
  vertexInputAttributs[2].offset = 0;

  VkPipelineVertexInputStateCreateInfo vertexInputState = {};
  vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputState.vertexBindingDescriptionCount = 3;
  vertexInputState.pVertexBindingDescriptions = vertexInputBindings;
  vertexInputState.vertexAttributeDescriptionCount = 3;
  vertexInputState.pVertexAttributeDescriptions = vertexInputAttributs;

  VkPipelineShaderStageCreateInfo shaderStages[2] = {};
  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = _device->createShader(SHADER_DIR "/shadow.vert.spv");
  shaderStages[0].pName = "main";
  assert(shaderStages[0].module != VK_NULL_HANDLE);

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = _device->createShader(std::string(SHADER_DIR) + "/" + _fragmentShader);
  shaderStages[1].pName = "main";
  assert(shaderStages[1].module != VK_NULL_HANDLE);

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

VkPipelineLayout ShadowPipeline::pipeLayout()
{
  if (!_pipeLayout) {
    VkDescriptorSetLayout layouts[] = {matrixLayout(), lightLayout(), textureLayout(), shadowLayout()};

    VkPushConstantRange transformConstants;
    transformConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    transformConstants.offset = 0;
    transformConstants.size = sizeof(Transform);

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = nullptr;
    pPipelineLayoutCreateInfo.setLayoutCount = 4;
    pPipelineLayoutCreateInfo.pSetLayouts = layouts;
    pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pPipelineLayoutCreateInfo.pPushConstantRanges = &transformConstants;

    VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
    VK_CHECK_RESULT(vkCreatePipelineLayout(*_device, &pPipelineLayoutCreateInfo, nullptr, &pipeLayout));
    _pipeLayout = pipeLayout;
  }

  return _pipeLayout;
}

VkDescriptorSetLayout ShadowPipeline::lightLayout()
{
  if (!_lightLayout) {
    VkDescriptorSetLayoutBinding binding = {};
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.binding = 0;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*_device, &layoutInfo, nullptr, &_lightLayout));
  }
  return _lightLayout;
}

VkDescriptorSetLayout ShadowPipeline::shadowLayout()
{
  if (!_shadowLayout) {
    VkDescriptorSetLayoutBinding layoutBinding[2] = {};
    layoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding[0].descriptorCount = 1;
    layoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    layoutBinding[0].pImmutableSamplers = nullptr;
    layoutBinding[0].binding = 0;

    layoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBinding[1].descriptorCount = 1;
    layoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBinding[1].pImmutableSamplers = nullptr;
    layoutBinding[1].binding = 1;

    VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.pNext = nullptr;
    descriptorLayout.bindingCount = 2;
    descriptorLayout.pBindings = layoutBinding;

    VkDescriptorSetLayout deslay = VK_NULL_HANDLE;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*_device, &descriptorLayout, nullptr, &deslay));
    _shadowLayout = deslay;
  }
  return _shadowLayout;
}
