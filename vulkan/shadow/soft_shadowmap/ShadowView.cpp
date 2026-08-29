#include "ShadowView.h"

#include "PCFShadow.h"
#include "ShadowTechnique.h"

#include "Manipulator.h"
#include "VulkanBuffer.h"
#include "VulkanDebug.h"
#include "VulkanDevice.h"
#include "VulkanInstance.h"
#include "VulkanPass.h"
#include "VulkanSwapChain.h"
#include "VulkanTools.h"
#include "VulkanView.h"

#include "GLTFLoader.h"
#include "MeshInstance.h"
#include "RenderData.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#include "config.h"
#include "imgui/imgui.h"

#define WM_PAINT 1

constexpr float fov = 60;

PBRBase pbr;
ParallelLight light;

VulkanInstance &inst = VulkanInstance::instance();

ShadowView::ShadowView(const std::shared_ptr<VulkanDevice> &dev)
  : VulkanView(dev, false)
{
  GLTFLoader loader;
  _model = loader.loadFile(ROOT_DIR "/data/plane_sphere.glb");
  //_tree->set_transform(tg::translate(tg::vec3(0, 1, 0)) * tg::scale(4.0f));

  setShadowType(ShadowType::PCF);

  {
    _basicTexture = std::make_shared<VulkanTexture>();
    _basicTexture->setImage(32, 32, tg::Tvec4<uint8_t>(128, 128, 128, 255));
    _basicTexture->realize(_device);
  }

  {
    _hudPass = std::make_shared<HUDPass>(dev);
    _hudPipeline = std::make_shared<HUDPipeline>(dev);
    _hudRect = std::make_shared<HUDRect>(dev);
    _hudRect->setGeometry(50, 50, 400, 400);
  }

  createPipeLayout();

  setUniforms();
}

ShadowView::~ShadowView()
{
  vkDeviceWaitIdle(*device());

  if (_vertBuf) {
    vkDestroyBuffer(*device(), _vertBuf, nullptr);
    _vertBuf = VK_NULL_HANDLE;
  }

  if (_vertMem) {
    vkFreeMemory(*device(), _vertMem, nullptr);
    _vertMem = VK_NULL_HANDLE;
  }

  if (_indexBuf) {
    vkDestroyBuffer(*device(), _indexBuf, nullptr);
    _indexBuf = VK_NULL_HANDLE;
  }

  if (_indexMem) {
    vkFreeMemory(*device(), _indexMem, nullptr);
    _indexMem = VK_NULL_HANDLE;
  }

  for (int i = 0; i < _hudFrames.size(); i++) {
    vkDestroyFramebuffer(*_device, _hudFrames[i], 0);
  }
  _hudFrames.clear();

  if (_shadow)
    _shadow->destroyFrameBuffers();
  _shadow.reset();
  _hudRect.reset();

  if (_descriptPool) {
    vkDestroyDescriptorPool(*device(), _descriptPool, nullptr);
    _descriptPool = VK_NULL_HANDLE;
  }
}

void ShadowView::setShadowType(ShadowType type)
{
  if (_shadow && _shadowType == type)
    return;

  bool rebuild = _shadowRealized;
  if (rebuild) {
    vkDeviceWaitIdle(*device());
    _shadow->destroyFrameBuffers();
  }

  _shadow.reset();
  _shadowType = type;
  switch (type) {
  case ShadowType::PCF:
    _shadow = std::make_unique<PCFShadow>(_device);
    break;
  }

  _shadow->initializeUniforms();
  if (rebuild) {
    _shadow->realize(renderPass(), _descriptPool);
    _shadow->createFrameBuffers(_swapchain->imageCount());
    _hudRect->setTexture(_hudPipeline.get(), _shadow->debugTexture(), _descriptPool);
  }
}

void ShadowView::setUniforms()
{
  light.lightDir = tg::normalize(tg::vec3(1, 1, 1));
  light.lightColor = tg::vec3(10);

  uint8_t *data = 0;
  VK_CHECK_RESULT(vkMapMemory(*device(), _light->memory(), 0, sizeof(light), 0, (void **)&data));
  memcpy(data, &light, sizeof(light));
}

void ShadowView::updateUbo()
{
  _matrix.eye = manipulator()->eye();
  _matrix.view = manipulator()->viewMatrix();
  _matrix.prj = tg::perspective<float>(fov, float(width()) / height(), 0.1, 1000);
  //_matrix.prj = tg::ortho(-100, 100, -100, 100, 1, 1000);
  // tg::near_clip(_matrix.prj, tg::vec4(0, 0, -1, 0.5));

  // auto xx = _matrix.view * tg::vec4(0, 0, 100, 1);
  // xx = _matrix.prj * xx;

  uint8_t *data = 0;
  VK_CHECK_RESULT(vkMapMemory(*device(), _uboBuf->memory(), 0, sizeof(_matrix), 0, (void **)&data));
  memcpy(data, &_matrix, sizeof(_matrix));
  vkUnmapMemory(*device(), _uboBuf->memory());
}

void ShadowView::resize(int w, int h)
{
  updateUbo();
}

void ShadowView::updateScene()
{
  if (_imgui) {
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Once);

    bool overlay = ImGui::Begin("test");
    ImGui::Text("wtf");
    ImGui::End();
    ImGui::EndFrame();
    ImGui::Render();
  }
}

void ShadowView::keyUp(int key)
{
  if (key == SDL_SCANCODE_SPACE)
    updateUbo();
  else if (key == SDL_SCANCODE_P) {
    _shadow->toggleFilterMode();
    updateUbo();
  }
}

void ShadowView::createCommandBuffers()
{
  int count = MaxConcurrentFrames;
  if (count != _cmdBufs.size()) {
    _device->destroyCommandBuffers(_cmdBufs);
    _cmdBufs = _device->createCommandBuffers(count);
  }
}

void ShadowView::recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t i)
{
  if (!_shadow || !_shadow->valid())
    return;

  auto &framebuffers = frameBuffers();
  auto &activeRenderPass = *renderPass();
  assert(i < framebuffers.size());
  assert(i < _hudFrames.size());

  VkCommandBufferBeginInfo bufInfo = {};
  bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bufInfo.pNext = nullptr;

  VkClearValue clearValues[2];

  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.pNext = nullptr;
  renderPassBeginInfo.renderArea.offset.x = 0;
  renderPassBeginInfo.renderArea.offset.y = 0;
  renderPassBeginInfo.renderArea.extent.width = _w;
  renderPassBeginInfo.renderArea.extent.height = _h;
  renderPassBeginInfo.clearValueCount = 1;
  renderPassBeginInfo.pClearValues = clearValues;

  VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuf, &bufInfo));

  _shadow->recordShadowPass(cmdBuf, i, *_model);

  clearValues[0].color = {{0.0, 0.0, 0.2, 1.0}};
  clearValues[1].depthStencil = {1.f, 0};
  renderPassBeginInfo.clearValueCount = 2;

  renderPassBeginInfo.renderPass = activeRenderPass;
  renderPassBeginInfo.framebuffer = framebuffers[i];
  renderPassBeginInfo.renderArea.extent.width = width();
  renderPassBeginInfo.renderArea.extent.height = height();

  vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  buildCommandBuffer(cmdBuf);

  vkCmdEndRenderPass(cmdBuf);

  {
    renderPassBeginInfo.renderPass = *_hudPass;
    renderPassBeginInfo.framebuffer = _hudFrames[i];
    renderPassBeginInfo.clearValueCount = 0;
    vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    {
      VkViewport viewport = {};
      viewport.width = _w;
      viewport.height = _h;
      viewport.minDepth = 0;
      viewport.maxDepth = 1;
      vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    }

    {
      VkRect2D scissor = {};
      scissor.extent.width = _w;
      scissor.extent.height = _h;
      scissor.offset.x = 0;
      scissor.offset.y = 0;
      vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
    }

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_hudPipeline);
    tg::mat4 mat;
    mat.identity();
    mat[0][0] = 2.0 / width();
    mat[1][1] = 2.0 / height();
    mat = tg::translate(-1.f, -1.f, 0.f) * mat;
    vkCmdPushConstants(cmdBuf, _hudPipeline->pipeLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat), &mat);
    _hudRect->fillCommand(cmdBuf, _hudPipeline.get());
    vkCmdEndRenderPass(cmdBuf);
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuf));
}

void ShadowView::buildCommandBuffer(VkCommandBuffer cmdBuf)
{
  tg::mat4 mt;
  mt.identity();
  {
    VkViewport viewport = {};
    viewport.y = _h;
    viewport.width = _w;
    viewport.height = -_h;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
  }

  {
    VkRect2D scissor = {};
    scissor.extent.width = _w;
    scissor.extent.height = _h;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
  }

  if (_shadow && _shadow->valid()) {
    _shadow->bindLighting(cmdBuf, _matrixSet, _lightSet);
    vkCmdPushConstants(cmdBuf, _shadow->lightingPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Transform), &mt);
  }

  _model->buildTextureCommandBuffer(cmdBuf, _shadow->lightingPipelineLayout(), 2);
}

void ShadowView::createPipeLayout()
{
  VkDescriptorPoolSize typeCounts[2] = {};
  typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  typeCounts[0].descriptorCount = 10;
  typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  typeCounts[1].descriptorCount = 2;

  VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.pNext = nullptr;
  descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptorPoolInfo.poolSizeCount = 2;
  descriptorPoolInfo.pPoolSizes = typeCounts;
  descriptorPoolInfo.maxSets = 10;

  VkDescriptorPool desPool;
  VK_CHECK_RESULT(vkCreateDescriptorPool(*device(), &descriptorPoolInfo, nullptr, &desPool));
  _descriptPool = desPool;

  //----------------------------------------------------------------------------------------------------
  auto mlayout = _shadow->matrixLayout();
  auto llayout = _shadow->lightLayout();

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = desPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &mlayout;

  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_matrixSet));

  allocInfo.pSetLayouts = &llayout;
  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_lightSet));

  VkDescriptorBufferInfo descriptor = {};
  int sz = sizeof(_matrix);
  _uboBuf = device()->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz);
  descriptor.buffer = *_uboBuf;
  descriptor.offset = 0;
  descriptor.range = sizeof(_matrix);

  sz = sizeof(light);
  VkDescriptorBufferInfo ldescriptor = {};
  _light = device()->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz);
  ldescriptor.buffer = *_light;
  ldescriptor.offset = 0;
  ldescriptor.range = sz;

  VkWriteDescriptorSet writeDescriptorSet = {};
  writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSet.dstSet = _matrixSet;
  writeDescriptorSet.descriptorCount = 1;
  writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writeDescriptorSet.pBufferInfo = &descriptor;
  writeDescriptorSet.dstBinding = 0;
  vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);

  writeDescriptorSet.dstSet = _lightSet;
  writeDescriptorSet.pBufferInfo = &ldescriptor;
  writeDescriptorSet.dstBinding = 0;
  vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);

  //----------------------------------------------------------------------------------------------------
  //{
  //  auto clayout = _shadow_pipeline->texture_layout();
  //  VkDescriptorSetAllocateInfo allocInfo = {};
  //  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  //  allocInfo.descriptorPool = desPool;
  //  allocInfo.descriptorSetCount = 1;
  //  allocInfo.pSetLayouts = &clayout;

  //  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_basic_tex_set));

  //  VkDescriptorImageInfo texDescriptor = {};
  //  texDescriptor.imageView = _basic_texture->image_view();
  //  texDescriptor.sampler = _basic_texture->sampler();
  //  texDescriptor.imageLayout = _basic_texture->image_layout();

  //  VkWriteDescriptorSet wd_set = {};
  //  wd_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  //  wd_set.dstBinding = 0;
  //  wd_set.descriptorCount = 1;
  //  wd_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  //  wd_set.pImageInfo = &_basic_texture->descriptor();
  //  wd_set.dstSet = _basic_tex_set;

  //  vkUpdateDescriptorSets(*device(), 1, &wd_set, 0, nullptr);
  //}
}

void ShadowView::createFrameBuffers()
{
  _shadow->createFrameBuffers(_swapchain->imageCount());

  setFrameBuffers(_swapchain->createFrameBuffer(*renderPass()));

  {
    for (int i = 0; i < _hudFrames.size(); i++)
      vkDestroyFramebuffer(*device(), _hudFrames[i], 0);

    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.pNext = NULL;
    frameBufferCreateInfo.renderPass = *_hudPass;
    frameBufferCreateInfo.attachmentCount = 1;
    frameBufferCreateInfo.width = _w;
    frameBufferCreateInfo.height = _h;
    frameBufferCreateInfo.layers = 1;

    std::vector<VkFramebuffer> frameBuffers;
    frameBuffers.resize(_swapchain->imageCount());
    for (uint32_t i = 0; i < frameBuffers.size(); i++) {
      VkImageView img = _swapchain->imageView(i);
      frameBufferCreateInfo.pAttachments = &img;
      VK_CHECK_RESULT(vkCreateFramebuffer(*_device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
    }
    _hudFrames = std::move(frameBuffers);
  }
}

void ShadowView::destroyFrameBuffers()
{
  if (_shadow)
    _shadow->destroyFrameBuffers();

  for (auto frame : _hudFrames)
    vkDestroyFramebuffer(*device(), frame, nullptr);
  _hudFrames.clear();

  VulkanView::destroyFrameBuffers();
}

void ShadowView::createPipeline()
{
  _shadow->realize(renderPass(), _descriptPool);
  _shadowRealized = true;
  _model->realize(_device);

  {
    _hudPipeline->realize(_hudPass.get());
    _hudRect->setTexture(_hudPipeline.get(), _shadow->debugTexture(), _descriptPool);
  }
}
