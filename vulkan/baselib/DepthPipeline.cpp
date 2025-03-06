#include "DepthPipeline.h"
#include "VulkanPass.h"
#include "VulkanTools.h"

#include "config.h"
#include "tvec.h"
#include "RenderData.h"

using tg::vec3;

#define SHADER_DIR ROOT_DIR##"/vulkan/baselib/shaders"


DepthPipeline::DepthPipeline(const std::shared_ptr<VulkanDevice>& dev, int w, int h) : Base(dev), _w(w), _h(h)
{
}

DepthPipeline::~DepthPipeline()
{
}

void DepthPipeline::realize(VulkanPass *render_pass, int subpass)
{
  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.layout = pipe_layout();
  pipelineCreateInfo.renderPass = *render_pass;

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

  VkViewport vp; 
  vp.x = 0; vp.y = _h;
  vp.width = _w; vp.height = -_h;
  vp.minDepth = 0; vp.maxDepth = 1;
  VkRect2D rt = {0}; rt.extent.width = _w; rt.extent.height = _h;
  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;
  viewportState.pViewports = &vp;
  viewportState.pScissors = &rt;

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

  VkVertexInputBindingDescription vertexInputBinding = {};
  vertexInputBinding.binding = 0;  
  vertexInputBinding.stride = sizeof(vec3);
  vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vertexInputAttribut = {};
  vertexInputAttribut.binding = 0;
  vertexInputAttribut.location = 0;
  vertexInputAttribut.format = VK_FORMAT_R32G32B32_SFLOAT;
  vertexInputAttribut.offset = 0;

  VkPipelineVertexInputStateCreateInfo vertexInputState = {};
  vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputState.vertexBindingDescriptionCount = 1;
  vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
  vertexInputState.vertexAttributeDescriptionCount = 1;
  vertexInputState.pVertexAttributeDescriptions = &vertexInputAttribut;

  VkPipelineShaderStageCreateInfo shaderStages[2] = {};
  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = _device->create_shader(SHADER_DIR "/depth.vert.spv");
  shaderStages[0].pName = "main";
  assert(shaderStages[0].module != VK_NULL_HANDLE);

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = _device->create_shader(SHADER_DIR "/depth.frag.spv");
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
  pipelineCreateInfo.pDynamicState = 0;
  pipelineCreateInfo.subpass = subpass;

  VK_CHECK_RESULT(vkCreateGraphicsPipelines(*_device, _device->get_or_create_pipecache(), 1, &pipelineCreateInfo, nullptr, &_pipeline));

  vkDestroyShaderModule(*_device, shaderStages[0].module, nullptr);
  vkDestroyShaderModule(*_device, shaderStages[1].module, nullptr);
}

VkPipelineLayout DepthPipeline::pipe_layout()
{
  if (!_pipe_layout) {
    auto lay = matrix_layout();

    VkPushConstantRange transformConstants;
    transformConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    transformConstants.offset = 0;
    transformConstants.size = sizeof(Transform);

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = nullptr;
    pPipelineLayoutCreateInfo.setLayoutCount = 1;
    pPipelineLayoutCreateInfo.pSetLayouts = &lay;
    pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pPipelineLayoutCreateInfo.pPushConstantRanges = &transformConstants;

    VkPipelineLayout pipe_layout = VK_NULL_HANDLE;
    VK_CHECK_RESULT(vkCreatePipelineLayout(*_device, &pPipelineLayoutCreateInfo, nullptr, &pipe_layout));
    _pipe_layout = pipe_layout;
  }
  return _pipe_layout;
}

VkPipelineLayout DepthPipeline::create_pipe_layout()
{
  VkDescriptorSetLayout layout = matrix_layout();

  VkPushConstantRange transformConstants;
  transformConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  transformConstants.offset = 0;
  transformConstants.size = sizeof(Transform);

  VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
  pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pPipelineLayoutCreateInfo.pNext = nullptr;
  pPipelineLayoutCreateInfo.setLayoutCount = 1;
  pPipelineLayoutCreateInfo.pSetLayouts = &layout;
  pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pPipelineLayoutCreateInfo.pPushConstantRanges = &transformConstants;

  VkPipelineLayout pipe_layout = VK_NULL_HANDLE;
  VK_CHECK_RESULT(vkCreatePipelineLayout(*_device, &pPipelineLayoutCreateInfo, nullptr, &pipe_layout));

  return pipe_layout;
}
