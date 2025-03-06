#include "VulkanImGUI.h"

#include "VulkanView.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanTools.h"
#include "VulkanInitializers.hpp"
#include "VulkanSwapChain.h"

#include "imgui/imgui.h"

#include "tvec.h"
#include "shaders/imgui.vert.spv"
#include "shaders/imgui.frag.spv"

namespace {
struct PushConstBlock {
  tg::vec2 scale;
  tg::vec2 translate;
} const_block;
}  // namespace

VulkanImGUI::VulkanImGUI(VulkanView* view) : _view(view)
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
  ImGui::StyleColorsLight();

  io.Fonts->AddFontFromFileTTF("c:/Windows/fonts/msyh.ttc", 16);
  unsigned char* data = 0;
  int texWidth, texHeight;
  io.Fonts->GetTexDataAsRGBA32(&data, &texWidth, &texHeight);

  auto device = view->device();
  std::tie(_font_img, _font_memory) = device->create_image(texWidth, texHeight);
  _font_view = device->create_image_view(_font_img);

  uint32_t sz = texWidth * texHeight * 4;
  auto buf = device->create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz, data);
  auto cmd = device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
  vks::tools::setImageLayout(cmd, _font_img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkBufferImageCopy copy_region = {};
  copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_region.imageSubresource.layerCount = 1;
  copy_region.imageExtent.width = texWidth;
  copy_region.imageExtent.height = texHeight;
  copy_region.imageExtent.depth = 1;

  vkCmdCopyBufferToImage(cmd, *buf, _font_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

  vks::tools::setImageLayout(cmd, _font_img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

  device->flush_command_buffer(cmd, device->transfer_queue(0));
  buf->destroy();
}

VulkanImGUI::~VulkanImGUI()
{
  auto device = _view->device();
  if (_pipeline)
    vkDestroyPipeline(*device, _pipeline, nullptr);
  if (_pipe_layout)
    vkDestroyPipelineLayout(*device, _pipe_layout, nullptr);
  if (_descriptor_pool)
    vkDestroyDescriptorPool(*device, _descriptor_pool, nullptr);
  if (_sampler)
    vkDestroySampler(*device, _sampler, nullptr);
  if(_descriptor_layout)
    vkDestroyDescriptorSetLayout(*device, _descriptor_layout, 0);

  for (auto& frame : _frame_bufs)
    vkDestroyFramebuffer(*device, frame, nullptr);

  if (_render_pass)
    vkDestroyRenderPass(*device, _render_pass, nullptr);

  if (_font_view)
    vkDestroyImageView(*device, _font_view, nullptr);
  if (_font_img)
    vkDestroyImage(*device, _font_img, nullptr);
  if (_font_memory)
    vkFreeMemory(*device, _font_memory, nullptr);
}

void VulkanImGUI::resize(int w, int h)
{
  auto& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(w, h);

  check_frame(_view->swapchain()->image_count(), _view->swapchain()->color_format());
}

void VulkanImGUI::create_pipeline(VkFormat clrformat)
{
  auto device = _view->device();

  if (!_render_pass)
    create_renderpass(clrformat);

  VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
  VK_CHECK_RESULT(vkCreateSampler(*device, &samplerInfo, nullptr, &_sampler));

  // Descriptor pool
  std::vector<VkDescriptorPoolSize> poolSizes = {vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)};
  VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
  VK_CHECK_RESULT(vkCreateDescriptorPool(*device, &descriptorPoolInfo, nullptr, &_descriptor_pool));

  // Descriptor set layout
  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
      vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
  };

  VkDescriptorSetLayout descriptor_layout;
  VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*device, &descriptorLayout, nullptr, &descriptor_layout));
  _descriptor_layout = descriptor_layout;

  VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(_descriptor_pool, &descriptor_layout, 1);
  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device, &allocInfo, &_descriptor_set));
  VkDescriptorImageInfo fontDescriptor = vks::initializers::descriptorImageInfo(_sampler, _font_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
      vks::initializers::writeDescriptorSet(_descriptor_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)};
  vkUpdateDescriptorSets(*device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

  VkPipelineLayout pipeLayout;
  VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptor_layout, 1);
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
  VK_CHECK_RESULT(vkCreatePipelineLayout(*device, &pipelineLayoutCreateInfo, nullptr, &pipeLayout));
  _pipe_layout = pipeLayout;

  //-----------------------------------------------------------------------------------------------------------

  // Push constants for UI rendering parameters
  VkPipelineCache pipe_cache = device->get_or_create_pipecache();

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
      vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

  VkPipelineRasterizationStateCreateInfo rasterizationState =
      vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

  VkPipelineColorBlendAttachmentState blendAttachmentState{};
  blendAttachmentState.blendEnable = VK_TRUE;
  blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
  blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

  VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

  VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

  VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

  std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

  VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(pipeLayout, _render_pass);

  VkPipelineShaderStageCreateInfo shaderStages[2] = {};
  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = device->create_shader((char *)imgui_vert, sizeof(imgui_vert));
  shaderStages[0].pName = "main";
  assert(shaderStages[0].module != VK_NULL_HANDLE);

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = device->create_shader((char *)imgui_frag, sizeof(imgui_frag));
  shaderStages[1].pName = "main";
  assert(shaderStages[1].module != VK_NULL_HANDLE);

  pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
  pipelineCreateInfo.pRasterizationState = &rasterizationState;
  pipelineCreateInfo.pColorBlendState = &colorBlendState;
  pipelineCreateInfo.pMultisampleState = &multisampleState;
  pipelineCreateInfo.pViewportState = &viewportState;
  pipelineCreateInfo.pDepthStencilState = &depthStencilState;
  pipelineCreateInfo.pDynamicState = &dynamicState;
  pipelineCreateInfo.stageCount = 2;
  pipelineCreateInfo.pStages = shaderStages;
  pipelineCreateInfo.subpass = 0;

#if defined(VK_KHR_dynamic_rendering)
  // SRS - if we are using dynamic rendering (i.e. renderPass null), must define color, depth and stencil attachments at pipeline create time
  VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
  if (_render_pass == VK_NULL_HANDLE) {
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &clrformat;
    pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
  }
#endif

  // Vertex bindings an attributes based on ImGui vertex definition
  std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
      vks::initializers::vertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
  };
  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
      vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),
      vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),
      vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),
  };
  VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
  vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
  vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
  vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
  vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

  pipelineCreateInfo.pVertexInputState = &vertexInputState;

  VK_CHECK_RESULT(vkCreateGraphicsPipelines(*device, pipe_cache, 1, &pipelineCreateInfo, nullptr, &_pipeline));

  vkDestroyShaderModule(*device, shaderStages[0].module, nullptr);
  vkDestroyShaderModule(*device, shaderStages[1].module, nullptr);
}

void VulkanImGUI::check_frame(int count, VkFormat clrformat)
{
  auto device = _view->device();

  for (auto& framebuf : _frame_bufs)
    vkDestroyFramebuffer(*device, framebuf, nullptr);

  _frame_bufs = _view->swapchain()->create_frame_buffer(_render_pass, VK_NULL_HANDLE);
  if (count != _cmd_bufs.size())
    _cmd_bufs = _view->device()->create_command_buffers(count);

  build_command_buffers();
}

bool VulkanImGUI::update_frame()
{
  ImDrawData* imDrawData = ImGui::GetDrawData();
  bool updateCmdBuffers = false;

  if (!imDrawData) {
    return false;
  };

  VkDeviceSize vertex_buf_size = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
  VkDeviceSize index_buf_size = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

  if (vertex_buf_size == 0 || index_buf_size == 0) {
    return false;
  }
  vertex_buf_size = (vertex_buf_size / 0x40 + 1) * 0x40;
  index_buf_size = (index_buf_size / 0x40 + 1) * 0x40;

  auto device = _view->device();

  auto vbuf = _vert_buf, ibuf = _index_buf;
  if (_vert_buf == 0 || _vert_buf->size() < vertex_buf_size) {
    _vert_buf_bak = device->create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, vertex_buf_size, 0);
    vbuf = _vert_buf_bak;
    updateCmdBuffers = true;
  }

  if (_index_buf == 0 || _index_buf->size() < index_buf_size) {
    _index_buf_bak = device->create_buffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, index_buf_size, 0);
    ibuf = _index_buf_bak;
    updateCmdBuffers = true;
  }

  uint8_t* vtxDst = vbuf->map();
  uint8_t* idxDst = ibuf->map();
  for (int n = 0; n < imDrawData->CmdListsCount; n++) {
    const ImDrawList* cmd_list = imDrawData->CmdLists[n];
    memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
    memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
    vtxDst += cmd_list->VtxBuffer.Size;
    idxDst += cmd_list->IdxBuffer.Size;
  }
  vbuf->flush();
  ibuf->flush();
  vbuf->unmap();
  ibuf->unmap();

  return updateCmdBuffers || _force_update;
}

void VulkanImGUI::draw(const VkCommandBuffer cmdbuf)
{
  ImDrawData* imdata = ImGui::GetDrawData();
  if (!imdata || imdata->CmdListsCount == 0)
    return;

  _force_update = false;
  auto& io = ImGui::GetIO();

  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);
  vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipe_layout, 0, 1, &_descriptor_set, 0, nullptr);

  const_block.scale = tg::vec2(2.0 / io.DisplaySize.x, -2.0 / io.DisplaySize.y);
  const_block.translate = tg::vec2(-1.f, 1.f);

  vkCmdPushConstants(cmdbuf, _pipe_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &const_block);

  if (_vert_buf_bak)
    _vert_buf = std::move(_vert_buf_bak);
  if (_index_buf_bak)
    _index_buf = std::move(_index_buf_bak);

  VkDeviceSize offsets[1] = {};
  vkCmdBindVertexBuffers(cmdbuf, 0, 1, *_vert_buf, offsets);
  vkCmdBindIndexBuffer(cmdbuf, *_index_buf, 0, VK_INDEX_TYPE_UINT16);

  uint32_t indexOffset = 0, vertexOffset = 0;
  for (int32_t i = 0; i < imdata->CmdListsCount; i++) {
    const ImDrawList* cmd_list = imdata->CmdLists[i];
    for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
      const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
      VkRect2D scissorRect;
      scissorRect.offset.x = std::max<uint32_t>((int32_t)(pcmd->ClipRect.x), 0);
      scissorRect.offset.y = std::max<uint32_t>((int32_t)(pcmd->ClipRect.y), 0);
      scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
      scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
      vkCmdSetScissor(cmdbuf, 0, 1, &scissorRect);
      vkCmdDrawIndexed(cmdbuf, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
      indexOffset += pcmd->ElemCount;
    }
    vertexOffset += cmd_list->VtxBuffer.Size;
  }
}

constexpr uint8_t button_map[] = {0, 2, 1};

bool VulkanImGUI::mouse_down(SDL_MouseButtonEvent& ev)
{
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(ev.x, ev.y);
  io.AddMouseButtonEvent(button_map[ev.button - 1], true);
  return io.WantCaptureMouse;
}

bool VulkanImGUI::mouse_up(SDL_MouseButtonEvent& ev)
{
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(ev.x, ev.y);
  io.AddMouseButtonEvent(button_map[ev.button - 1], false);
  return io.WantCaptureMouse;
}

bool VulkanImGUI::mouse_move(SDL_MouseMotionEvent& ev)
{
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(ev.x, ev.y);

  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    dirty();

  return io.WantCaptureMouse;
}

void VulkanImGUI::dirty()
{
  _force_update = true;
}

void VulkanImGUI::create_canvas()
{
}

void VulkanImGUI::create_renderpass(VkFormat color)
{
  VkAttachmentDescription attachment = {};
  attachment.format = color;
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorReference = {};
  colorReference.attachment = 0;
  colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpassDescription = {};
  subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpassDescription.colorAttachmentCount = 1;
  subpassDescription.pColorAttachments = &colorReference;
  subpassDescription.inputAttachmentCount = 0;
  subpassDescription.pInputAttachments = nullptr;
  subpassDescription.preserveAttachmentCount = 0;
  subpassDescription.pPreserveAttachments = nullptr;
  subpassDescription.pResolveAttachments = nullptr;

  VkSubpassDependency dependency;

  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  dependency.dependencyFlags = 0;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &attachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpassDescription;
  renderPassInfo.dependencyCount = 1; 
  renderPassInfo.pDependencies = &dependency;

  VkRenderPass renderPass = VK_NULL_HANDLE;
  VK_CHECK_RESULT(vkCreateRenderPass(*_view->device(), &renderPassInfo, nullptr, &renderPass));

  _render_pass = renderPass;
}

void VulkanImGUI::build_command_buffers()
{
  auto& framebuffers = _frame_bufs;
  auto& renderPass = _render_pass;
  auto device = _view->device();

  assert(framebuffers.size() == _cmd_bufs.size());

  VkCommandBufferBeginInfo buf_info = {};
  buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  buf_info.pNext = nullptr;

  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.pNext = nullptr;
  renderPassBeginInfo.renderPass = renderPass;
  renderPassBeginInfo.renderArea.offset.x = 0;
  renderPassBeginInfo.renderArea.offset.y = 0;
  renderPassBeginInfo.renderArea.extent.width = _view->width();
  renderPassBeginInfo.renderArea.extent.height = _view->height();
  renderPassBeginInfo.clearValueCount = 0;
  renderPassBeginInfo.pClearValues = nullptr;

  for (int i = 0; i < _cmd_bufs.size(); i++) {
    renderPassBeginInfo.framebuffer = framebuffers[i];
    auto& cmd_buf = _cmd_bufs[i];
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmd_buf, &buf_info));

    vkCmdBeginRenderPass(cmd_buf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    {
      VkViewport viewport = {};
      viewport.y = _view->height();
      viewport.width = _view->width();
      viewport.height = -_view->height();
      viewport.minDepth = 0;
      viewport.maxDepth = 1;
      vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    }

    {
      VkRect2D scissor = {};
      scissor.extent.width = _view->width();
      scissor.extent.height = _view->height();
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      vkCmdSetScissor(cmd_buf, 0, 1, &scissor);
    }

    draw(cmd_buf);

    vkCmdEndRenderPass(cmd_buf);

    VK_CHECK_RESULT(vkEndCommandBuffer(cmd_buf));
  }
}
