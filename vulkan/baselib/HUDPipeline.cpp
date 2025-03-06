#include "HUDPipeline.h"
#include "VulkanPass.h"
#include "VulkanTools.h"

#include "config.h"
#include "tvec.h"
#include "RenderData.h"

using tg::vec3;
using tg::vec2;

#define SHADER_DIR ROOT_DIR##"/vulkan/baselib/shaders"


HUDPipeline::HUDPipeline(const std::shared_ptr<VulkanDevice>& dev) : Base(dev){
}

HUDPipeline::~HUDPipeline()
{
  if (_hud_tex_layout)
  {
    vkDestroyDescriptorSetLayout(*_device, _hud_tex_layout, 0);
    _hud_tex_layout = VK_NULL_HANDLE;
  }
}

void HUDPipeline::realize(VulkanPass *render_pass, int subpass)
{
  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.layout = pipe_layout();
  pipelineCreateInfo.renderPass = *render_pass;

  std::vector<VkDynamicState> dynamicStateEnables;
  dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
  dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.pDynamicStates = dynamicStateEnables.data();
  dynamicState.dynamicStateCount = dynamicStateEnables.size();

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

  VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
  depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilState.depthTestEnable = VK_FALSE;
  depthStencilState.depthWriteEnable = VK_FALSE;
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

  VkVertexInputBindingDescription vertexInputBindings[2] = {};
  vertexInputBindings[0].binding = 0;  
  vertexInputBindings[0].stride = sizeof(vec2);
  vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  vertexInputBindings[1].binding = 1;  
  vertexInputBindings[1].stride = sizeof(vec2);
  vertexInputBindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vertexInputAttributs[2] = {};
  vertexInputAttributs[0].binding = 0;
  vertexInputAttributs[0].location = 0;
  vertexInputAttributs[0].format = VK_FORMAT_R32G32_SFLOAT;
  vertexInputAttributs[0].offset = 0;
  vertexInputAttributs[1].binding = 1;
  vertexInputAttributs[1].location = 1;
  vertexInputAttributs[1].format = VK_FORMAT_R32G32_SFLOAT;
  vertexInputAttributs[1].offset = 0;

  VkPipelineVertexInputStateCreateInfo vertexInputState = {};
  vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputState.vertexBindingDescriptionCount = 2;
  vertexInputState.pVertexBindingDescriptions = vertexInputBindings;
  vertexInputState.vertexAttributeDescriptionCount = 2;
  vertexInputState.pVertexAttributeDescriptions = vertexInputAttributs;

  VkPipelineShaderStageCreateInfo shaderStages[2] = {};
  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = _device->create_shader(SHADER_DIR "/hud.vert.spv");
  shaderStages[0].pName = "main";
  assert(shaderStages[0].module != VK_NULL_HANDLE);

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = _device->create_shader(SHADER_DIR "/hud.frag.spv");
  shaderStages[1].pName = "main";
  assert(shaderStages[1].module != VK_NULL_HANDLE);

  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = shaderStages;

  pipelineCreateInfo.pVertexInputState = &vertexInputState;
  pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
  pipelineCreateInfo.pRasterizationState = &rasterizationState;
  pipelineCreateInfo.pColorBlendState = &colorBlendState;
  pipelineCreateInfo.pMultisampleState = &multisampleState;
  pipelineCreateInfo.pDepthStencilState = &depthStencilState;
  pipelineCreateInfo.pDynamicState = &dynamicState;
  pipelineCreateInfo.pViewportState = &viewportState;
  pipelineCreateInfo.subpass = 0;

  VK_CHECK_RESULT(vkCreateGraphicsPipelines(*_device, _device->get_or_create_pipecache(), 1, &pipelineCreateInfo, nullptr, &_pipeline));

  vkDestroyShaderModule(*_device, shaderStages[0].module, nullptr);
  vkDestroyShaderModule(*_device, shaderStages[1].module, nullptr);
}

VkDescriptorSetLayout HUDPipeline::texture_layout()
{
  if (!_hud_tex_layout) {
    VkDescriptorSetLayoutBinding layoutBinding;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBinding.pImmutableSamplers = nullptr;
    layoutBinding.binding = 0;

    VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.pNext = nullptr;
    descriptorLayout.bindingCount = 1;
    descriptorLayout.pBindings = &layoutBinding;

    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*_device, &descriptorLayout, 0, &_hud_tex_layout));
  }
  return _hud_tex_layout;
}

VkPipelineLayout HUDPipeline::create_pipe_layout()
{ 
  VkDescriptorSetLayout set_layout = texture_layout();

  VkPushConstantRange transformConstants;
  transformConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  transformConstants.offset = 0;
  transformConstants.size = sizeof(Transform);

  VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
  pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pPipelineLayoutCreateInfo.pNext = nullptr;
  pPipelineLayoutCreateInfo.setLayoutCount = 1;
  pPipelineLayoutCreateInfo.pSetLayouts = &set_layout;
  pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pPipelineLayoutCreateInfo.pPushConstantRanges = &transformConstants;

  VkPipelineLayout pipe_layout = VK_NULL_HANDLE;
  VK_CHECK_RESULT(vkCreatePipelineLayout(*_device, &pPipelineLayoutCreateInfo, nullptr, &pipe_layout));
  return pipe_layout;
}
