#include "ShadowView.h"

#include "VulkanDebug.h"
#include "VulkanView.h"
#include "VulkanInstance.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanTools.h"
#include "VulkanPass.h"
#include "DepthPass.h"
#include "VulkanInitializers.hpp"

#include "VulkanPipeline.h"
#include "TexturePipeline.h"
#include "DepthPipeline.h"

#include "SimpleShape.h"
#include "RenderData.h"
#include "GLTFLoader.h"
#include "MeshInstance.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#include "config.h"
#include "imgui/imgui.h"

#define WM_PAINT 1

constexpr float fov = 60;

PBRBase pbr;
ParallelLight light;

VulkanInstance &inst = VulkanInstance::instance();

ShadowView::ShadowView(const std::shared_ptr<VulkanDevice> &dev) : VulkanView(dev, false)
{
  create_sphere();

  GLTFLoader loader;
  _tree = loader.load_file(ROOT_DIR "/data/oaktree.gltf");
  _tree->set_transform(tg::translate(tg::vec3(0, 0, 1)) * tg::scale(4.0f));

  _deer = loader.load_file(ROOT_DIR "/data/deer.gltf");
  _deer->set_transform(tg::translate(tg::vec3(3, 3, 1)) * tg::rotate(tg::radians(30.f), tg::vec3(0, 0, 1)) * tg::scale(1.0f));

  _shadow_pipeline = std::make_shared<ShadowPipeline>(dev);
  _depth_pipeline = std::make_shared<DepthPipeline>(dev, 2048, 2048);

  _depth_image = _device->create_depth_image(2048, 2048, VK_FORMAT_D32_SFLOAT);

  _depth_pass = std::make_shared<DepthPass>(dev);

  {
    _basic_texture = std::make_shared<VulkanTexture>();
    _basic_texture->set_image(32, 32, tg::Tvec4<uint8_t>(128, 128, 128, 255));
    _basic_texture->realize(_device);
  }

  create_pipe_layout();

  set_uniforms();
}

ShadowView::~ShadowView()
{
  vkDeviceWaitIdle(*device());

  if (_vert_buf) {
    vkDestroyBuffer(*device(), _vert_buf, nullptr);
    _vert_buf = VK_NULL_HANDLE;
  }

  if (_vert_mem) {
    vkFreeMemory(*device(), _vert_mem, nullptr);
    _vert_mem = VK_NULL_HANDLE;
  }

  if (_index_buf) {
    vkDestroyBuffer(*device(), _index_buf, nullptr);
    _index_buf = VK_NULL_HANDLE;
  }

  if (_index_mem) {
    vkFreeMemory(*device(), _index_mem, nullptr);
    _index_mem = VK_NULL_HANDLE;
  }

  if (_descript_pool) {
    vkDestroyDescriptorPool(*device(), _descript_pool, nullptr);
    _descript_pool = VK_NULL_HANDLE;
  }

  for (int i = 0; i < _depth_frames.size(); i++) {
    vkDestroyFramebuffer(*_device, _depth_frames[i], 0);
  }
  _depth_frames.clear();
}

void ShadowView::create_sphere()
{
  Box box(vec3(0), vec3(40, 40, 2));
  box.build();
  auto &verts = box.get_vertex();
  auto &norms = box.get_norms();
  auto &uv = box.get_uvs();
  auto &index = box.get_index();
  _vert_count = verts.size();
  _index_count = index.size();

  struct StageBuffer {
    VkBuffer buffer;
    VkDeviceMemory mem;
  };
  StageBuffer vertices, indices;

  uint64_t vert_size = verts.size() * (sizeof(vec3) * 2 + sizeof(vec2));
  VkBufferCreateInfo vertexBufferInfo = {};
  vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vertexBufferInfo.size = vert_size;
  vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &vertexBufferInfo, nullptr, &vertices.buffer));
  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(*device(), vertices.buffer, &memReqs);

  VkMemoryAllocateInfo memAlloc = {};
  memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memory_type_index(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(*device(), &memAlloc, nullptr, &vertices.mem));

  void *data = 0;
  VK_CHECK_RESULT(vkMapMemory(*device(), vertices.mem, 0, memAlloc.allocationSize, 0, &data));
  uint64_t offset = 0;
  memcpy(data, verts.data(), verts.size() * sizeof(vec3));
  offset += verts.size() * sizeof(vec3);
  memcpy((uint8_t *)data + offset, norms.data(), norms.size() * sizeof(vec3));
  offset += norms.size() * sizeof(vec3);
  memcpy((uint8_t *)data + offset, uv.data(), uv.size() * sizeof(vec2));
  vkUnmapMemory(*device(), vertices.mem);
  VK_CHECK_RESULT(vkBindBufferMemory(*device(), vertices.buffer, vertices.mem, 0));

  vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &vertexBufferInfo, nullptr, &_vert_buf));
  vkGetBufferMemoryRequirements(*device(), _vert_buf, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memory_type_index(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(*device(), &memAlloc, nullptr, &_vert_mem));
  VK_CHECK_RESULT(vkBindBufferMemory(*device(), _vert_buf, _vert_mem, 0));

  uint64_t index_size = index.size() * sizeof(uint16_t);
  VkBufferCreateInfo indexbufferInfo = {};
  indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  indexbufferInfo.size = index_size;
  indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  // Copy index data to a buffer visible to the host (staging buffer)
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &indexbufferInfo, nullptr, &indices.buffer));
  vkGetBufferMemoryRequirements(*device(), indices.buffer, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memory_type_index(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(*device(), &memAlloc, nullptr, &indices.mem));
  VK_CHECK_RESULT(vkMapMemory(*device(), indices.mem, 0, index_size, 0, &data));
  memcpy(data, index.data(), index_size);
  vkUnmapMemory(*device(), indices.mem);
  VK_CHECK_RESULT(vkBindBufferMemory(*device(), indices.buffer, indices.mem, 0));

  // Create destination buffer with device only visibility
  indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &indexbufferInfo, nullptr, &_index_buf));
  vkGetBufferMemoryRequirements(*device(), _index_buf, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memory_type_index(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(*device(), &memAlloc, nullptr, &_index_mem));
  VK_CHECK_RESULT(vkBindBufferMemory(*device(), _index_buf, _index_mem, 0));

  {
    VkCommandBuffer cmdBuffer;

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = device()->command_pool();
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = 1;

    VK_CHECK_RESULT(vkAllocateCommandBuffers(*device(), &cmdBufAllocateInfo, &cmdBuffer));

    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

    VkBufferCopy copyRegion = {};
    copyRegion.size = vert_size;
    vkCmdCopyBuffer(cmdBuffer, vertices.buffer, _vert_buf, 1, &copyRegion);

    copyRegion.size = index_size;
    vkCmdCopyBuffer(cmdBuffer, indices.buffer, _index_buf, 1, &copyRegion);

    VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;
    VkFence fence;
    VK_CHECK_RESULT(vkCreateFence(*device(), &fenceCreateInfo, nullptr, &fence));

    VK_CHECK_RESULT(vkQueueSubmit(device()->transfer_queue(), 1, &submitInfo, fence));
    VK_CHECK_RESULT(vkWaitForFences(*device(), 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
    vkDestroyFence(*device(), fence, nullptr);
    vkFreeCommandBuffers(*device(), device()->command_pool(), 1, &cmdBuffer);
  }

  vkDestroyBuffer(*device(), vertices.buffer, nullptr);
  vkDestroyBuffer(*device(), indices.buffer, nullptr);
  vkFreeMemory(*device(), vertices.mem, nullptr);
  vkFreeMemory(*device(), indices.mem, nullptr);
}

void ShadowView::set_uniforms()
{
  light.light_dir = tg::normalize(vec3(1, 1, 1));
  light.light_color = vec3(10);

  uint8_t *data = 0;
  VK_CHECK_RESULT(vkMapMemory(*device(), _light->memory(), 0, sizeof(light), 0, (void **)&data));
  memcpy(data, &light, sizeof(light));

  pbr.albedo = vec3(0.8);
  pbr.ao = 1;
  pbr.metallic = 0.2;
  pbr.roughness = 0.7;
  VK_CHECK_RESULT(vkMapMemory(*device(), _material->memory(), 0, sizeof(pbr), 0, (void **)&data));
  memcpy(data, &pbr, sizeof(pbr));

  auto vp = tg::vec3(100);
  _depth_matrix_buf = device()->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(MVP));
  _depth_matrix.view = tg::lookat(vp);
  _depth_matrix.prj = tg::ortho<float>(-25, 25, -25, 25, 10, 400);

  VK_CHECK_RESULT(vkMapMemory(*device(), _depth_matrix_buf->memory(), 0, sizeof(MVP), 0, (void **)&data));
  memcpy(data, &_depth_matrix, sizeof(MVP));

  {
    _shadow_buf = device()->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                          sizeof(ShadowMatrix));
    ShadowMatrix sm;
    sm.light = tg::normalize(vp); 
    sm.view = _depth_matrix.view;
    sm.prj = _depth_matrix.prj;
    sm.mvp = _depth_matrix.prj * _depth_matrix.view;
    
    VK_CHECK_RESULT(vkMapMemory(*device(), _shadow_buf->memory(), 0, sizeof(ShadowMatrix), 0, (void **)&data));
    memcpy(data, &sm, sizeof(ShadowMatrix));
  }
}

void ShadowView::update_ubo()
{
  _matrix.eye = manipulator().eye();
  _matrix.view = manipulator().view_matrix();
  _matrix.prj = tg::perspective<float>(fov, float(width()) / height(), 0.1, 1000);
  //_matrix.prj = tg::ortho(-100, 100, -100, 100, 1, 1000);
  // tg::near_clip(_matrix.prj, tg::vec4(0, 0, -1, 0.5));

  // auto xx = _matrix.view * tg::vec4(0, 0, 100, 1);
  // xx = _matrix.prj * xx;

  uint8_t *data = 0;
  VK_CHECK_RESULT(vkMapMemory(*device(), _ubo_buf->memory(), 0, sizeof(_matrix), 0, (void **)&data));
  memcpy(data, &_matrix, sizeof(_matrix));
  vkUnmapMemory(*device(), _ubo_buf->memory());
}

void ShadowView::resize(int w, int h)
{
  update_ubo();
}

void ShadowView::update_scene()
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

  // if (overlay)
  //   update_overlay();
}

void ShadowView::key_up(int key)
{
  if (key == SDL_SCANCODE_SPACE)
    update_ubo();
}

void ShadowView::create_command_buffers()
{
  int count = _swapchain->image_count();
  if (count != _cmd_bufs.size()) {
    _cmd_bufs = _device->create_command_buffers(count);
  }
}

void ShadowView::build_depth_command_buffer(VkCommandBuffer cmd_buf)
{
  tg::mat4 mt;
  mt.identity();
  if (_depth_pipeline && _depth_pipeline->valid()) {
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_depth_pipeline);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _depth_pipeline->pipe_layout(), 0, 1, &_depth_matrix_set, 0, nullptr);

    vkCmdPushConstants(cmd_buf, _depth_pipeline->pipe_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Transform), &mt);

    VkDeviceSize oft = 0;
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &_vert_buf, &oft);
    vkCmdBindIndexBuffer(cmd_buf, _index_buf, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd_buf, _index_count, 1, 0, 0, 0);

    _tree->build_command_buffer(cmd_buf, _depth_pipeline);

    _deer->build_command_buffer(cmd_buf, _depth_pipeline);
  }
}

void ShadowView::build_command_buffers()
{
  if (!_depth_pipeline->valid() || !_shadow_pipeline->valid())
    return;

  vkDeviceWaitIdle(*_device);

  auto &framebuffers = frame_buffers();
  auto &renderPass = *render_pass();
  assert(framebuffers.size() == _cmd_bufs.size());

  VkCommandBufferBeginInfo buf_info = {};
  buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  buf_info.pNext = nullptr;

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

  for (int i = 0; i < _cmd_bufs.size(); i++) {
    renderPassBeginInfo.renderPass = *_depth_pass;
    renderPassBeginInfo.framebuffer = _depth_frames[i];
    renderPassBeginInfo.renderArea.extent.width = _depth_image->width();
    renderPassBeginInfo.renderArea.extent.height = _depth_image->height();
    auto &cmd_buf = _cmd_bufs[i];
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmd_buf, &buf_info));

    clearValues[0].depthStencil = {1.f, 0};
    renderPassBeginInfo.clearValueCount = 1;
    vkCmdBeginRenderPass(cmd_buf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    build_depth_command_buffer(cmd_buf);

    vkCmdEndRenderPass(cmd_buf);

    clearValues[0].color = {{0.0, 0.0, 0.2, 1.0}};
    clearValues[1].depthStencil = {1.f, 0};
    renderPassBeginInfo.clearValueCount = 2;

    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.framebuffer = framebuffers[i];
    renderPassBeginInfo.renderArea.extent.width = width();
    renderPassBeginInfo.renderArea.extent.height = height();

    vkCmdBeginRenderPass(cmd_buf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    build_command_buffer(cmd_buf);

    vkCmdEndRenderPass(cmd_buf);

    VK_CHECK_RESULT(vkEndCommandBuffer(cmd_buf));
  }
}

void ShadowView::build_command_buffer(VkCommandBuffer cmd_buf) 
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
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
  }

  {
    VkRect2D scissor = {};
    scissor.extent.width = _w;
    scissor.extent.height = _h;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);
  }

  if (_shadow_pipeline && _shadow_pipeline->valid()) {
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_shadow_pipeline);

    uint32_t offset[1] = {};
    VkDescriptorSet dessets[3] = {_matrix_set, _light_set, _pbr_set};
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadow_pipeline->pipe_layout(), 0, 3, dessets, 1, offset);

    VkWriteDescriptorSet texture_set = {};
    texture_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    texture_set.dstSet = 0;
    texture_set.dstBinding = 0;
    texture_set.descriptorCount = 1;
    texture_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    auto descriptor = _basic_texture->descriptor();
    texture_set.pImageInfo = &descriptor;
    _device->vkCmdPushDescriptorSetKHR(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadow_pipeline->pipe_layout(), 3, 1, &texture_set);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadow_pipeline->pipe_layout(), 4, 1, &_shadow_set, 0, 0);

    vkCmdPushConstants(cmd_buf, _shadow_pipeline->pipe_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Transform), &mt);

    {
      VkDeviceSize offset[3] = {0, _vert_count * sizeof(vec3), _vert_count * (sizeof(vec3) + sizeof(vec2))};
      VkBuffer bufs[3] = {};
      bufs[0] = _vert_buf;
      bufs[1] = _vert_buf;
      bufs[2] = _vert_buf;

      vkCmdBindVertexBuffers(cmd_buf, 0, 3, bufs, offset);
      vkCmdBindIndexBuffer(cmd_buf, _index_buf, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmd_buf, _index_count, 1, 0, 0, 0);
    }
  }

  _tree->build_command_buffer(cmd_buf, std::static_pointer_cast<TexturePipeline>(_shadow_pipeline));

  _deer->build_command_buffer(cmd_buf, std::static_pointer_cast<TexturePipeline>(_shadow_pipeline));
}

void ShadowView::create_pipe_layout()
{
  VkDescriptorPoolSize typeCounts[1];
  typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  typeCounts[0].descriptorCount = 10;

  VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.pNext = nullptr;
  descriptorPoolInfo.poolSizeCount = 1;
  descriptorPoolInfo.pPoolSizes = typeCounts;
  descriptorPoolInfo.maxSets = 10;

  VkDescriptorPool desPool;
  VK_CHECK_RESULT(vkCreateDescriptorPool(*device(), &descriptorPoolInfo, nullptr, &desPool));
  _descript_pool = desPool;

  //----------------------------------------------------------------------------------------------------
  auto mlayout = _shadow_pipeline->matrix_layout();
  auto llayout = _shadow_pipeline->light_layout();
  auto playout = _shadow_pipeline->pbr_layout();

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = desPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &mlayout;

  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_matrix_set));

  allocInfo.pSetLayouts = &llayout;
  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_light_set));

  allocInfo.pSetLayouts = &playout;
  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_pbr_set));

  VkDescriptorBufferInfo descriptor = {};
  int sz = sizeof(_matrix);
  _ubo_buf = device()->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz);
  descriptor.buffer = *_ubo_buf;
  descriptor.offset = 0;
  descriptor.range = sizeof(_matrix);

  sz = sizeof(light);
  VkDescriptorBufferInfo ldescriptor = {};
  _light = device()->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz);
  ldescriptor.buffer = *_light;
  ldescriptor.offset = 0;
  ldescriptor.range = sz;

  sz = sizeof(pbr);
  VkDescriptorBufferInfo mdescriptor = {};
  _material = device()->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz);
  mdescriptor.buffer = *_material;
  mdescriptor.offset = 0;
  mdescriptor.range = sz;

  VkWriteDescriptorSet writeDescriptorSet = {};
  writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSet.dstSet = _matrix_set;
  writeDescriptorSet.descriptorCount = 1;
  writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writeDescriptorSet.pBufferInfo = &descriptor;
  writeDescriptorSet.dstBinding = 0;
  vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);

  writeDescriptorSet.dstSet = _light_set;
  writeDescriptorSet.pBufferInfo = &ldescriptor;
  writeDescriptorSet.dstBinding = 0;
  vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);

  writeDescriptorSet.dstSet = _pbr_set;
  writeDescriptorSet.pBufferInfo = &mdescriptor;
  writeDescriptorSet.dstBinding = 0;
  //writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
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

void ShadowView::create_frame_buffers()
{
  {
    auto view = _depth_image->image_view();
    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.pNext = NULL;
    frameBufferCreateInfo.renderPass = *_depth_pass;
    frameBufferCreateInfo.attachmentCount = 1;
    frameBufferCreateInfo.pAttachments = &view;
    frameBufferCreateInfo.width = _depth_image->width();
    frameBufferCreateInfo.height = _depth_image->height();
    frameBufferCreateInfo.layers = 1;

    _depth_frames.resize(_swapchain->image_count());
    for (int i = 0; i < _depth_frames.size(); i++) {
      VK_CHECK_RESULT(vkCreateFramebuffer(*_device, &frameBufferCreateInfo, nullptr, &_depth_frames[i]));
    }
  }

  VkImageView attachments[2];
  attachments[1] = _depth->image_view();

  VkFramebufferCreateInfo frameBufferCreateInfo = {};
  frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  frameBufferCreateInfo.pNext = NULL;
  frameBufferCreateInfo.renderPass = *render_pass();
  frameBufferCreateInfo.attachmentCount = 2;
  frameBufferCreateInfo.pAttachments = attachments;
  frameBufferCreateInfo.width = _w;
  frameBufferCreateInfo.height = _h;
  frameBufferCreateInfo.layers = 1;

  std::vector<VkFramebuffer> frameBuffers;
  frameBuffers.resize(_swapchain->image_count());
  for (uint32_t i = 0; i < frameBuffers.size(); i++) {
    attachments[0] = _swapchain->image_view(i);
    VK_CHECK_RESULT(vkCreateFramebuffer(*_device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
  }

  set_frame_buffers(frameBuffers);
}

void ShadowView::create_pipeline()
{
  if (_depth_pipeline) {
    _depth_pipeline->realize(_depth_pass.get());

    auto des_layout = _depth_pipeline->matrix_layout();
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descript_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &des_layout;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_depth_matrix_set));

    int sz = sizeof(_depth_matrix);
    VkDescriptorBufferInfo descriptor = {};
    descriptor.buffer = *_depth_matrix_buf;
    descriptor.offset = 0;
    descriptor.range = sz;

    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = _depth_matrix_set;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &descriptor;
    writeDescriptorSet.dstBinding = 0;
    vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);
  }

  _shadow_pipeline->realize(render_pass());

  _tree->realize(_device, _shadow_pipeline);

  _deer->realize(_device, _shadow_pipeline);

  {
    auto slayout = _shadow_pipeline->shadow_layout();
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descript_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &slayout;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_shadow_set));

    int sz = sizeof(ShadowMatrix);
    VkDescriptorBufferInfo descriptor = {};
    descriptor.buffer = *_shadow_buf;
    descriptor.offset = 0;
    descriptor.range = sz;

    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = _shadow_set;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &descriptor;
    writeDescriptorSet.dstBinding = 0;
    vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);

    _shadow_texture = std::make_shared<VulkanTexture>();
    _shadow_texture->realize(_depth_image);

    VkDescriptorImageInfo depthDescriptor = _shadow_texture->descriptor();

    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.dstBinding = 1;
    writeDescriptorSet.pBufferInfo = 0;
    writeDescriptorSet.pImageInfo = &depthDescriptor;

    vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);
  }

  build_command_buffers();
}
