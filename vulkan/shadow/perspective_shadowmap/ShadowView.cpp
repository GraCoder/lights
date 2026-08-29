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
#include "Manipulator.h"
#include "DepthPass.h"
#include "VulkanInitializers.hpp"

#include "VulkanPipeline.h"
#include "TexturePipeline.h"
#include "DepthPipeline.h"
#include "DepthPersPipeline.h"

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

inline tg::mat4 lookatLh(const tg::vec3& eye, const tg::vec3& center, const tg::vec3& up)
{
  vec3 f = -normalize(eye - center);
  const vec3 s = normalize(cross(up, f));
  const vec3 u = normalize(cross(f, s));
  const tg::mat4 M =
      tg::mat4(
        tg::vec4(s[0], u[0], -f[0], 0),
        tg::vec4(s[1], u[1], -f[1], 0),
        tg::vec4(s[2], u[2], -f[2], 0),
        tg::vec4(0, 0, 0, 1)
      );

  return M * tg::translate<float>(-eye);
}

auto calPsmMatrix(const tg::vec3 &lightDir, const tg::vec3 &camEye, const tg::vec3 &camCenter, float zn, float zf, const tg::boundingbox &psc)
{
  struct Ret {
    tg::mat4 mm;
    tg::mat4 mp;
  };

  tg::vec3 lt = tg::normalize(lightDir);
  auto rt = tg::cross(lt, camEye - camCenter);
  rt = tg::normalize(rt);
  auto ft = tg::cross(lt, rt);

  tg::boundingbox bd = psc;
  float n = sqrt(zf * zn) - zn;
  float dis = 0, f = 0, fov = 0;

  Ret ret;
  {
    bd.expand(camEye);
    float rad = bd.radius();
    tg::vec3 center = bd.center();
    dis = n + rad;
    tg::vec3 eye = center - ft * dis;
    ret.mm = tg::lookat(eye, center, lt);
    f = n + bd.radius() * 2.0;
    fov = tg::degrees(asin(sin(bd.radius() / dis)) * 2);
  }

  ret.mp = tg::perspective<float>(fov, 1.0, n, f);

  return ret;
}

ShadowView::ShadowView(const std::shared_ptr<VulkanDevice> &dev) : VulkanView(dev, true)
{
  createSphere();

  GLTFLoader loader;
  _tree = loader.loadFile(ROOT_DIR "/data/oaktree.gltf");
  _tree->setTransform(tg::mat4(tg::translate(tg::vec3(0, 1, 0)) * tg::scale(4.0f)));

  _deer = loader.loadFile(ROOT_DIR "/data/deer.gltf");
  _deer->setTransform(tg::mat4(tg::translate(tg::vec3(3, 1, 0)) * tg::rotate(tg::radians(30.f), tg::vec3(0, 1, 0)) * tg::scale(1.f)));

  _shadowPipeline = std::make_shared<ShadowPipeline>(dev);

  _depthPipeline = std::make_shared<DepthPersPipeline>(dev, 2048, 2048);
  _depthImage = _device->createDepthImage(2048, 2048, VK_FORMAT_D32_SFLOAT);

  _depthPass = std::make_shared<DepthPass>(dev);

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

  //manipulator()->setHome({0, 0, 30}, {0, 0, 0}, {0, 1, 0});
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

  if (_descriptPool) {
    vkDestroyDescriptorPool(*device(), _descriptPool, nullptr);
    _descriptPool = VK_NULL_HANDLE;
  }

  for (int i = 0; i < _depthFrames.size(); i++) {
    vkDestroyFramebuffer(*_device, _depthFrames[i], 0);
  }
  _depthFrames.clear();

  for (int i = 0; i < _hudFrames.size(); i++)
  {
    vkDestroyFramebuffer(*_device, _hudFrames[i], 0);
  }
  _hudFrames.clear();
}

void ShadowView::createSphere()
{
  Box box(vec3(0), vec3(20, 2, 20));
  box.build();
  auto &verts = box.getVertex();
  auto &norms = box.getNorms();
  auto &uv = box.getUvs();
  auto &index = box.getIndex();
  _vertCount = verts.size();
  _indexCount = index.size();

  struct StageBuffer {
    VkBuffer buffer;
    VkDeviceMemory mem;
  };
  StageBuffer vertices, indices;

  uint64_t vertSize = verts.size() * (sizeof(vec3) * 2 + sizeof(vec2));
  VkBufferCreateInfo vertexBufferInfo = {};
  vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vertexBufferInfo.size = vertSize;
  vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &vertexBufferInfo, nullptr, &vertices.buffer));
  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(*device(), vertices.buffer, &memReqs);

  VkMemoryAllocateInfo memAlloc = {};
  memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &vertexBufferInfo, nullptr, &_vertBuf));
  vkGetBufferMemoryRequirements(*device(), _vertBuf, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(*device(), &memAlloc, nullptr, &_vertMem));
  VK_CHECK_RESULT(vkBindBufferMemory(*device(), _vertBuf, _vertMem, 0));

  uint64_t indexSize = index.size() * sizeof(uint16_t);
  VkBufferCreateInfo indexbufferInfo = {};
  indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  indexbufferInfo.size = indexSize;
  indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  // Copy index data to a buffer visible to the host (staging buffer)
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &indexbufferInfo, nullptr, &indices.buffer));
  vkGetBufferMemoryRequirements(*device(), indices.buffer, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(*device(), &memAlloc, nullptr, &indices.mem));
  VK_CHECK_RESULT(vkMapMemory(*device(), indices.mem, 0, indexSize, 0, &data));
  memcpy(data, index.data(), indexSize);
  vkUnmapMemory(*device(), indices.mem);
  VK_CHECK_RESULT(vkBindBufferMemory(*device(), indices.buffer, indices.mem, 0));

  // Create destination buffer with device only visibility
  indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &indexbufferInfo, nullptr, &_indexBuf));
  vkGetBufferMemoryRequirements(*device(), _indexBuf, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(*device(), &memAlloc, nullptr, &_indexMem));
  VK_CHECK_RESULT(vkBindBufferMemory(*device(), _indexBuf, _indexMem, 0));

  {
    VkCommandBuffer cmdBuffer;

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = device()->commandPool();
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = 1;

    VK_CHECK_RESULT(vkAllocateCommandBuffers(*device(), &cmdBufAllocateInfo, &cmdBuffer));

    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

    VkBufferCopy copyRegion = {};
    copyRegion.size = vertSize;
    vkCmdCopyBuffer(cmdBuffer, vertices.buffer, _vertBuf, 1, &copyRegion);

    copyRegion.size = indexSize;
    vkCmdCopyBuffer(cmdBuffer, indices.buffer, _indexBuf, 1, &copyRegion);

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

    VK_CHECK_RESULT(vkQueueSubmit(device()->transferQueue(), 1, &submitInfo, fence));
    VK_CHECK_RESULT(vkWaitForFences(*device(), 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
    vkDestroyFence(*device(), fence, nullptr);
    vkFreeCommandBuffers(*device(), device()->commandPool(), 1, &cmdBuffer);
  }

  vkDestroyBuffer(*device(), vertices.buffer, nullptr);
  vkDestroyBuffer(*device(), indices.buffer, nullptr);
  vkFreeMemory(*device(), vertices.mem, nullptr);
  vkFreeMemory(*device(), indices.mem, nullptr);
}

void ShadowView::setUniforms()
{
  light.lightDir = tg::normalize(vec3(0, 1, 2));
  light.lightColor = vec3(10);

  pbr.albedo = vec3(0.8);
  pbr.ao = 1;
  pbr.metallic = 0.2;
  pbr.roughness = 0.7;
  uint8_t *data = 0;

  VK_CHECK_RESULT(vkMapMemory(*device(), _material->memory(), 0, sizeof(pbr), 0, (void **)&data));
  memcpy(data, &pbr, sizeof(pbr));
  vkUnmapMemory(*device(), _material->memory());

  _shadowBuf = device()->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(ShadowMatrix));

  updateLight();
}

void ShadowView::updateUbo()
{
  _matrix.eye = manipulator()->eye();
  _matrix.view = manipulator()->viewMatrix();
  _matrix.prj = tg::perspective<float>(fov, float(width()) / height(), 0.1, 1000);

  void *data = 0;
  {
    VK_CHECK_RESULT(vkMapMemory(*device(), _uboBuf->memory(), 0, sizeof(_matrix), 0, (void **)&data));
    memcpy(data, &_matrix, sizeof(_matrix));
    vkUnmapMemory(*device(), _uboBuf->memory());
  }

  tg::boundingbox psc(tg::vec3(-10, 0, -10), tg::vec3(10, 4, 10));
  auto vp = manipulator()->eye();
  auto ct = tg::vec3(0, 0, 0);
  auto [mm, mp] = calPsmMatrix(light.lightDir, vp, ct, 0.1, 20, psc);

  tg::mat4 mat = mp * mm;

  {
    tg::boundingbox perbox, viewbox;
    for (int i = 0; i < 8; i++)
    {
      auto v = mat * psc.corner(i);
      perbox.expand(v);
    }
    auto nup = tg::vec3(0, 0, 1);
    auto neye = perbox.center();
    auto npos = neye;
    neye.y() = perbox.max().y() + 0.1;

    auto viewMatrix = lookatLh(neye, npos, nup);
    _shadowMatrix.view = viewMatrix;

    for (int i = 0; i < 8; i++)
    {
      auto v = viewMatrix * perbox.corner(i);
      viewbox.expand(v);
    }

    _shadowMatrix.prj = tg::ortho(viewbox.min().x(), viewbox.max().x(), viewbox.min().y(), viewbox.max().y(), -viewbox.max().z(), - viewbox.min().z());
    _shadowMatrix.mvp = _shadowMatrix.prj * _shadowMatrix.view;

    auto pp1 = mat * vec3(-10, 0, -10);
    auto pp2 = _shadowMatrix.view * pp1;
    auto pp3 = _shadowMatrix.prj * pp2;

    auto pp4 = mat * vec3(10, 0, -10);
    auto pp5 = _shadowMatrix.mvp * pp4;
    printf("");
  }

  _shadowMatrix.pers = mat;

  {
    VK_CHECK_RESULT(vkMapMemory(*device(), _shadowBuf->memory(), 0, sizeof(ShadowMatrix), 0, (void **)&data));
    memcpy(data, &_shadowMatrix, sizeof(ShadowMatrix));
    vkUnmapMemory(*device(), _shadowBuf->memory());
  }
}

void ShadowView::updateLight()
{
  _shadowMatrix.light = light.lightDir;

  uint8_t *data = 0;
  {
    VK_CHECK_RESULT(vkMapMemory(*device(), _light->memory(), 0, sizeof(light), 0, (void **)&data));
    memcpy(data, &light, sizeof(light));
  vkUnmapMemory(*device(), _light->memory());
  }
}

void ShadowView::resize(int w, int h)
{
  updateUbo();
}

void ShadowView::updateScene()
{
  auto fun = [this]() {
    tg::vec3 dir;
    auto x = tg::radians(_lightDir.x());
    auto y = tg::radians(_lightDir.y());
    dir.x() = cos(x) * cos(y);
    dir.y() = sin(y);
    dir.z() = sin(x) * cos(y);

    tg::normalize(dir);
    light.lightDir = dir;

    updateLight();
    updateUbo();
  };

  if (_imgui) {
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Once);

    ImGui::Begin("test");

    if (ImGui::SliderFloat("x", &_lightDir.x(), -180, 180)) {
      fun();
    }

    if (ImGui::SliderFloat("y", &_lightDir.y(), 0, 90)) {
      fun();
    }

    ImGui::End();
    ImGui::EndFrame();
    ImGui::Render();
  }
}

void ShadowView::keyUp(int key)
{
  if (key == SDL_SCANCODE_UP)
    manipulator()->rotate(0, 1);
  else if (key == SDL_SCANCODE_DOWN)
    manipulator()->rotate(0, -1);
  else if (key == SDL_SCANCODE_LEFT)
    manipulator()->rotate(-1, 0);
  else if (key == SDL_SCANCODE_RIGHT)
    manipulator()->rotate(1, 0);

  updateUbo();
}

void ShadowView::createCommandBuffers()
{
  int count = MaxConcurrentFrames;
  if (count != _cmdBufs.size()) {
    _device->destroyCommandBuffers(_cmdBufs);
    _cmdBufs = _device->createCommandBuffers(count);
  }
}

void ShadowView::buildDepthCommandBuffer(VkCommandBuffer cmdBuf)
{
  tg::mat4 mt;
  mt.identity();
  if (_depthPipeline && _depthPipeline->valid()) {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_depthPipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _depthPipeline->pipeLayout(), 0, 1, &_shadowMatrixSet, 0, nullptr);

    vkCmdPushConstants(cmdBuf, _depthPipeline->pipeLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Transform), &mt);

    VkWriteDescriptorSet textureSet = {};
    textureSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    textureSet.dstSet = 0;
    textureSet.dstBinding = 0;
    textureSet.descriptorCount = 1;
    textureSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    auto descriptor = _basicTexture->descriptor();
    textureSet.pImageInfo = &descriptor;
    _device->vkCmdPushDescriptorSetKHR(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _depthPipeline->pipeLayout(), 1, 1, &textureSet);

    {
      VkDeviceSize offset[3] = {0, _vertCount * sizeof(vec3), _vertCount * (sizeof(vec3) + sizeof(vec2))};
      VkBuffer bufs[3] = {};
      bufs[0] = _vertBuf;
      bufs[1] = _vertBuf;
      bufs[2] = _vertBuf;

      vkCmdBindVertexBuffers(cmdBuf, 0, 3, bufs, offset);
      vkCmdBindIndexBuffer(cmdBuf, _indexBuf, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmdBuf, _indexCount, 1, 0, 0, 0);
    }

    _tree->buildDepthTextureCommandBuffer(cmdBuf, _depthPipeline->pipeLayout());

    _deer->buildDepthTextureCommandBuffer(cmdBuf, _depthPipeline->pipeLayout());
  }
}

void ShadowView::recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t i)
{
  if (!_depthPipeline->valid() || !_shadowPipeline->valid())
    return;

  auto &framebuffers = frameBuffers();
  auto &activeRenderPass = *renderPass();
  assert(i < framebuffers.size());
  assert(i < _depthFrames.size());
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

    renderPassBeginInfo.renderPass = *_depthPass;
    renderPassBeginInfo.framebuffer = _depthFrames[i];
    renderPassBeginInfo.renderArea.extent.width = _depthImage->width();
    renderPassBeginInfo.renderArea.extent.height = _depthImage->height();
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuf, &bufInfo));

    clearValues[0].depthStencil = {1.f, 0};
    renderPassBeginInfo.clearValueCount = 1;
    vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    buildDepthCommandBuffer(cmdBuf);

    vkCmdEndRenderPass(cmdBuf);

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

  if (_shadowPipeline && _shadowPipeline->valid()) {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_shadowPipeline);

    uint32_t offset[1] = {};
    VkDescriptorSet dessets[3] = {_matrixSet, _lightSet, _pbrSet};
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline->pipeLayout(), 0, 3, dessets, 1, offset);

    VkWriteDescriptorSet textureSet = {};
    textureSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    textureSet.dstSet = 0;
    textureSet.dstBinding = 0;
    textureSet.descriptorCount = 1;
    textureSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    auto descriptor = _basicTexture->descriptor();
    textureSet.pImageInfo = &descriptor;
    _device->vkCmdPushDescriptorSetKHR(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline->pipeLayout(), 3, 1, &textureSet);

    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline->pipeLayout(), 4, 1, &_shadowMatrixSet, 0, 0);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline->pipeLayout(), 5, 1, &_shadowTextureSet, 0, 0);

    vkCmdPushConstants(cmdBuf, _shadowPipeline->pipeLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Transform), &mt);

    {
      VkDeviceSize offset[3] = {0, _vertCount * sizeof(vec3), _vertCount * (sizeof(vec3) + sizeof(vec2))};
      VkBuffer bufs[3] = {};
      bufs[0] = _vertBuf;
      bufs[1] = _vertBuf;
      bufs[2] = _vertBuf;

      vkCmdBindVertexBuffers(cmdBuf, 0, 3, bufs, offset);
      vkCmdBindIndexBuffer(cmdBuf, _indexBuf, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmdBuf, _indexCount, 1, 0, 0, 0);
    }
  }

  _tree->buildTextureCommandBuffer(cmdBuf, _shadowPipeline->pipeLayout());

  _deer->buildTextureCommandBuffer(cmdBuf, _shadowPipeline->pipeLayout());
}

void ShadowView::createPipeLayout()
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
  _descriptPool = desPool;

  //----------------------------------------------------------------------------------------------------
  auto mlayout = _shadowPipeline->matrixLayout();
  auto llayout = _shadowPipeline->lightLayout();
  auto playout = _shadowPipeline->pbrLayout();

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = desPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &mlayout;

  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_matrixSet));

  allocInfo.pSetLayouts = &llayout;
  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_lightSet));

  allocInfo.pSetLayouts = &playout;
  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_pbrSet));

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

  sz = sizeof(pbr);
  VkDescriptorBufferInfo mdescriptor = {};
  _material = device()->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz);
  mdescriptor.buffer = *_material;
  mdescriptor.offset = 0;
  mdescriptor.range = sz;

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

  writeDescriptorSet.dstSet = _pbrSet;
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

void ShadowView::createFrameBuffers()
{
  {
    auto view = _depthImage->imageView();
    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.pNext = NULL;
    frameBufferCreateInfo.renderPass = *_depthPass;
    frameBufferCreateInfo.attachmentCount = 1;
    frameBufferCreateInfo.pAttachments = &view;
    frameBufferCreateInfo.width = _depthImage->width();
    frameBufferCreateInfo.height = _depthImage->height();
    frameBufferCreateInfo.layers = 1;

    for (int i = 0; i < _depthFrames.size(); i++)
      vkDestroyFramebuffer(*device(), _depthFrames[i], 0);

    _depthFrames.resize(_swapchain->imageCount());
    for (int i = 0; i < _depthFrames.size(); i++) {
      VK_CHECK_RESULT(vkCreateFramebuffer(*_device, &frameBufferCreateInfo, nullptr, &_depthFrames[i]));
    }
  }

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
  for (auto frame : _depthFrames)
    vkDestroyFramebuffer(*device(), frame, nullptr);
  _depthFrames.clear();

  for (auto frame : _hudFrames)
    vkDestroyFramebuffer(*device(), frame, nullptr);
  _hudFrames.clear();

  VulkanView::destroyFrameBuffers();
}

void ShadowView::createPipeline()
{
  if (_depthPipeline) {
    _depthPipeline->realize(_depthPass.get());

    auto desLayout = _depthPipeline->matrixLayout();
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descriptPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &desLayout;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_shadowMatrixSet));

    int sz = sizeof(_shadowMatrix);
    VkDescriptorBufferInfo descriptor = {};
    descriptor.buffer = *_shadowBuf;
    descriptor.offset = 0;
    descriptor.range = sz;

    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = _shadowMatrixSet;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &descriptor;
    writeDescriptorSet.dstBinding = 0;
    vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);
  }

  _shadowPipeline->realize(renderPass());

  _tree->realize(_device, _shadowPipeline);

  _deer->realize(_device, _shadowPipeline);

  {
    auto slayout = _shadowPipeline->shadowTextureLayout();
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descriptPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &slayout;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_shadowTextureSet));

    _shadowTexture = std::make_shared<VulkanTexture>();
    _shadowTexture->realize(_depthImage);
    VkDescriptorImageInfo depthDescriptor = _shadowTexture->descriptor();

    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = _shadowTextureSet;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pBufferInfo = 0;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.pImageInfo = &depthDescriptor;
    vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);
  }

  {
    _hudPipeline->realize(_hudPass.get());
    _hudRect->setTexture(_hudPipeline.get(), _shadowTexture.get(), _descriptPool);
  }

}
