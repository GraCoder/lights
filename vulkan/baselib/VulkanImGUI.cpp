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
} constBlock;
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
  std::tie(_fontImg, _fontMemory) = device->createImage(texWidth, texHeight);
  _fontView = device->createImageView(_fontImg);

  uint32_t sz = texWidth * texHeight * 4;
  auto buf = device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz, data);
  auto cmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
  vks::tools::setImageLayout(cmd, _fontImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkBufferImageCopy copyRegion = {};
  copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copyRegion.imageSubresource.layerCount = 1;
  copyRegion.imageExtent.width = texWidth;
  copyRegion.imageExtent.height = texHeight;
  copyRegion.imageExtent.depth = 1;

  vkCmdCopyBufferToImage(cmd, *buf, _fontImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

  vks::tools::setImageLayout(cmd, _fontImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

  device->flushCommandBuffer(cmd, device->transferQueue(0));
  buf->destroy();
}

VulkanImGUI::~VulkanImGUI()
{
  auto device = _view->device();
  if (_pipeline)
    vkDestroyPipeline(*device, _pipeline, nullptr);
  if (_pipeLayout)
    vkDestroyPipelineLayout(*device, _pipeLayout, nullptr);
  if (_descriptorPool)
    vkDestroyDescriptorPool(*device, _descriptorPool, nullptr);
  if (_sampler)
    vkDestroySampler(*device, _sampler, nullptr);
  if(_descriptorLayout)
    vkDestroyDescriptorSetLayout(*device, _descriptorLayout, 0);

  for (auto& frame : _frameBufs)
    vkDestroyFramebuffer(*device, frame, nullptr);

  if (_renderPass)
    vkDestroyRenderPass(*device, _renderPass, nullptr);

  if (_fontView)
    vkDestroyImageView(*device, _fontView, nullptr);
  if (_fontImg)
    vkDestroyImage(*device, _fontImg, nullptr);
  if (_fontMemory)
    vkFreeMemory(*device, _fontMemory, nullptr);
}

void VulkanImGUI::resize(int w, int h)
{
  auto& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(w, h);

  checkFrame(_view->swapchain()->imageCount(), _view->swapchain()->colorFormat());
}

void VulkanImGUI::createPipeline(VkFormat clrformat)
{
  auto device = _view->device();

  if (!_renderPass)
    createRenderpass(clrformat);

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
  VK_CHECK_RESULT(vkCreateDescriptorPool(*device, &descriptorPoolInfo, nullptr, &_descriptorPool));

  // Descriptor set layout
  std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
      vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
  };

  VkDescriptorSetLayout descriptorLayout;
  VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*device, &descriptorLayoutInfo, nullptr, &descriptorLayout));
  _descriptorLayout = descriptorLayout;

  VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(_descriptorPool, &descriptorLayout, 1);
  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device, &allocInfo, &_descriptorSet));
  VkDescriptorImageInfo fontDescriptor = vks::initializers::descriptorImageInfo(_sampler, _fontView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
      vks::initializers::writeDescriptorSet(_descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)};
  vkUpdateDescriptorSets(*device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

  VkPipelineLayout pipeLayout;
  VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorLayout, 1);
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
  VK_CHECK_RESULT(vkCreatePipelineLayout(*device, &pipelineLayoutCreateInfo, nullptr, &pipeLayout));
  _pipeLayout = pipeLayout;

  //-----------------------------------------------------------------------------------------------------------

  // Push constants for UI rendering parameters
  VkPipelineCache pipeCache = device->getOrCreatePipecache();

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

  VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(pipeLayout, _renderPass);

  VkPipelineShaderStageCreateInfo shaderStages[2] = {};
  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = device->createShader((char *)imgui_vert, sizeof(imgui_vert));
  shaderStages[0].pName = "main";
  assert(shaderStages[0].module != VK_NULL_HANDLE);

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = device->createShader((char *)imgui_frag, sizeof(imgui_frag));
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
  if (_renderPass == VK_NULL_HANDLE) {
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

  VK_CHECK_RESULT(vkCreateGraphicsPipelines(*device, pipeCache, 1, &pipelineCreateInfo, nullptr, &_pipeline));

  vkDestroyShaderModule(*device, shaderStages[0].module, nullptr);
  vkDestroyShaderModule(*device, shaderStages[1].module, nullptr);
}

void VulkanImGUI::checkFrame(int count, VkFormat clrformat)
{
  auto device = _view->device();

  for (auto& framebuf : _frameBufs)
    vkDestroyFramebuffer(*device, framebuf, nullptr);

  _frameBufs = _view->swapchain()->createFrameBuffer(_renderPass, VK_NULL_HANDLE);
  if (VulkanView::MaxConcurrentFrames != _cmdBufs.size()) {
    _view->device()->destroyCommandBuffers(_cmdBufs);
    _cmdBufs = _view->device()->createCommandBuffers(VulkanView::MaxConcurrentFrames);
  }
}

bool VulkanImGUI::updateFrame()
{
  ImDrawData* imDrawData = ImGui::GetDrawData();
  bool updateCmdBuffers = false;

  if (!imDrawData) {
    return false;
  };

  VkDeviceSize vertexBufSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
  VkDeviceSize indexBufSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

  if (vertexBufSize == 0 || indexBufSize == 0) {
    return false;
  }
  vertexBufSize = (vertexBufSize / 0x40 + 1) * 0x40;
  indexBufSize = (indexBufSize / 0x40 + 1) * 0x40;

  auto device = _view->device();

  auto vbuf = _vertBuf, ibuf = _indexBuf;
  if (_vertBuf == 0 || _vertBuf->size() < vertexBufSize) {
    _vertBufBak = device->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, vertexBufSize, 0);
    vbuf = _vertBufBak;
    updateCmdBuffers = true;
  }

  if (_indexBuf == 0 || _indexBuf->size() < indexBufSize) {
    _indexBufBak = device->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, indexBufSize, 0);
    ibuf = _indexBufBak;
    updateCmdBuffers = true;
  }

  uint8_t* vtxDst = vbuf->map();
  uint8_t* idxDst = ibuf->map();
  for (int n = 0; n < imDrawData->CmdListsCount; n++) {
    const ImDrawList* cmdList = imDrawData->CmdLists[n];
    memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
    memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
    vtxDst += cmdList->VtxBuffer.Size;
    idxDst += cmdList->IdxBuffer.Size;
  }
  vbuf->flush();
  ibuf->flush();
  vbuf->unmap();
  ibuf->unmap();

  return updateCmdBuffers || _forceUpdate;
}

void VulkanImGUI::draw(const VkCommandBuffer cmdbuf)
{
  ImDrawData* imdata = ImGui::GetDrawData();
  if (!imdata || imdata->CmdListsCount == 0)
    return;

  _forceUpdate = false;
  auto& io = ImGui::GetIO();

  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);
  vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeLayout, 0, 1, &_descriptorSet, 0, nullptr);

  constBlock.scale = tg::vec2(2.0 / io.DisplaySize.x, -2.0 / io.DisplaySize.y);
  constBlock.translate = tg::vec2(-1.f, 1.f);

  vkCmdPushConstants(cmdbuf, _pipeLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &constBlock);

  if (_vertBufBak)
    _vertBuf = std::move(_vertBufBak);
  if (_indexBufBak)
    _indexBuf = std::move(_indexBufBak);

  VkDeviceSize offsets[1] = {};
  vkCmdBindVertexBuffers(cmdbuf, 0, 1, *_vertBuf, offsets);
  vkCmdBindIndexBuffer(cmdbuf, *_indexBuf, 0, VK_INDEX_TYPE_UINT16);

  uint32_t indexOffset = 0, vertexOffset = 0;
  for (int32_t i = 0; i < imdata->CmdListsCount; i++) {
    const ImDrawList* cmdList = imdata->CmdLists[i];
    for (int32_t j = 0; j < cmdList->CmdBuffer.Size; j++) {
      const ImDrawCmd* pcmd = &cmdList->CmdBuffer[j];
      VkRect2D scissorRect;
      scissorRect.offset.x = std::max<uint32_t>((int32_t)(pcmd->ClipRect.x), 0);
      scissorRect.offset.y = std::max<uint32_t>((int32_t)(pcmd->ClipRect.y), 0);
      scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
      scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
      vkCmdSetScissor(cmdbuf, 0, 1, &scissorRect);
      vkCmdDrawIndexed(cmdbuf, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
      indexOffset += pcmd->ElemCount;
    }
    vertexOffset += cmdList->VtxBuffer.Size;
  }
}

constexpr uint8_t buttonMap[] = {0, 2, 1};

bool VulkanImGUI::mouseDown(SDL_MouseButtonEvent& ev)
{
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(ev.x, ev.y);
  io.AddMouseButtonEvent(buttonMap[ev.button - 1], true);
  return io.WantCaptureMouse;
}

bool VulkanImGUI::mouseUp(SDL_MouseButtonEvent& ev)
{
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(ev.x, ev.y);
  io.AddMouseButtonEvent(buttonMap[ev.button - 1], false);
  return io.WantCaptureMouse;
}

bool VulkanImGUI::mouseMove(SDL_MouseMotionEvent& ev)
{
  auto& io = ImGui::GetIO();
  io.AddMousePosEvent(ev.x, ev.y);

  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    dirty();

  return io.WantCaptureMouse;
}

void VulkanImGUI::dirty()
{
  _forceUpdate = true;
}

void VulkanImGUI::createCanvas()
{
}

void VulkanImGUI::createRenderpass(VkFormat color)
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

  _renderPass = renderPass;
}

void VulkanImGUI::recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex)
{
  assert(frameIndex < _cmdBufs.size());
  assert(imageIndex < _frameBufs.size());

  auto cmdBuf = _cmdBufs[frameIndex];
  VK_CHECK_RESULT(vkResetCommandBuffer(cmdBuf, 0));

  VkCommandBufferBeginInfo bufInfo = {};
  bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bufInfo.pNext = nullptr;

  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.pNext = nullptr;
  renderPassBeginInfo.renderPass = _renderPass;
  renderPassBeginInfo.renderArea.offset.x = 0;
  renderPassBeginInfo.renderArea.offset.y = 0;
  renderPassBeginInfo.renderArea.extent.width = _view->width();
  renderPassBeginInfo.renderArea.extent.height = _view->height();
  renderPassBeginInfo.clearValueCount = 0;
  renderPassBeginInfo.pClearValues = nullptr;

  renderPassBeginInfo.framebuffer = _frameBufs[imageIndex];
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuf, &bufInfo));

    vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    {
      VkViewport viewport = {};
      viewport.y = _view->height();
      viewport.width = _view->width();
      viewport.height = -_view->height();
      viewport.minDepth = 0;
      viewport.maxDepth = 1;
      vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    }

    {
      VkRect2D scissor = {};
      scissor.extent.width = _view->width();
      scissor.extent.height = _view->height();
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
    }

    draw(cmdBuf);

    vkCmdEndRenderPass(cmdBuf);

  VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuf));
}
