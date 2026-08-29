#include "VulkanView.h"

#include <stdint.h>
#include <vulkan/vulkan.h>

#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL.h>

#include "tvec.h"
#include "tmath.h"

#include "VulkanInstance.h"
#include "VulkanTools.h"
#include "VulkanDevice.h"
#include "VulkanImage.h"
#include "VulkanSwapChain.h"
#include "VulkanPass.h"
#include "VulkanImGUI.h"
#include "Manipulator.h"


using tg::vec2;
using tg::vec3;
using tg::mat4;

VulkanView::VulkanView(const std::shared_ptr<VulkanDevice>& dev, bool overlay)
  : _device(dev), _manip(std::make_shared<Manipulator>())
{
  initialize();

  if(overlay)
    _imgui = std::make_shared<VulkanImGUI>(this);
}

VulkanView::~VulkanView()
{
  vkDeviceWaitIdle(*_device);

  _imgui.reset();
  destroySyncObjs();

  _device->destroyCommandBuffers(_cmdBufs);

  destroyFrameBuffers();
  _swapchain.reset();

  //auto surface = _swapchain->surface();
  //if (surface) {
  //  vkDestroySurfaceKHR(VulkanInstance::instance(), surface, nullptr);
  //}
}

void VulkanView::setSurface(VkSurfaceKHR surface, int w, int h)
{
  _swapchain->setSurface(surface);
  _swapchain->realize(w, h, true);

  if (_imgui) _imgui->createPipeline(_swapchain->colorFormat());

  resizeImpl(w, h);
}

void VulkanView::updateOverlay()
{
  if (_imgui)
    _imgui->dirty();
}

void VulkanView::frame(bool continus)
{
  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          running = false;
          break;
        case SDL_WINDOWEVENT:
          if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
            vkDeviceWaitIdle(*_device);
            int w = event.window.data1;
            int h = event.window.data2;
            resizeImpl(w, h);
            updateFrame();
          }
          break;
        case SDL_USEREVENT:
          if (event.user.code == WM_PAINT)
            render();
          break;
        case SDL_MOUSEBUTTONDOWN:
          if (_imgui && _imgui->mouseDown(event.button)) {
          } else {
            if (event.button.button == 1)
              leftDn(event.button.x, event.button.y);
          }
          updateFrame();
          break;
        case SDL_MOUSEBUTTONUP:
          if (_imgui && _imgui->mouseUp(event.button)) {
          } else {
            if (event.button.button == 1)
              leftUp(event.button.x, event.button.y);
          }
          updateFrame();
          break;
        case SDL_MOUSEMOTION:
          if (_imgui && _imgui->mouseMove(event.motion)) {
          } else {
            if (event.motion.state & SDL_BUTTON_LMASK) {
              _manip->rotate(event.motion.xrel, event.motion.yrel);
              leftDrag(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
            } else if (event.motion.state & SDL_BUTTON_MMASK) {
            } else if (event.motion.state & SDL_BUTTON_RMASK) {
              _manip->translate(event.motion.xrel, -event.motion.yrel);
              rightDrag(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
            }
          }
          updateFrame();
          break;
        case SDL_MOUSEWHEEL: {
          _manip->zoom(event.wheel.y);
          wheel(event.wheel.y);
          updateFrame();
          break;
        }
        case SDL_KEYUP: {
          if (event.key.keysym.scancode == SDL_SCANCODE_SPACE)
            _manip->home();
          keyUp(event.key.keysym.scancode);
          updateFrame();
        } break;
        default:
          break;
      }
    }
  }
}

void VulkanView::setRenderPass(VkRenderPass renderPass)
{
}

void VulkanView::setFrameBuffers(const std::vector<VkFramebuffer>& frameBufs)
{
  for (auto& frame : _frameBufs) {
    if (frame) {
      vkDestroyFramebuffer(*_device, frame, nullptr);
      frame = VK_NULL_HANDLE;
    }
  }
  _frameBufs = frameBufs;
}

uint32_t VulkanView::frameCount()
{
  return _swapchain->imageCount();
}

void VulkanView::updateFrame()
{
  updateScene();

  SDL_Event ev;
  ev.type = SDL_USEREVENT;
  ev.user.code = WM_PAINT;
  SDL_PushEvent(&ev);

  if (_imgui && _imgui->updateFrame()) {
    if (_frameFences[0] != VK_NULL_HANDLE)
      VK_CHECK_RESULT(vkWaitForFences(*_device, MaxConcurrentFrames, _frameFences.data(), VK_TRUE, UINT64_MAX));
  }
}

void VulkanView::createFrameBuffers()
{
  _frameBufs = _swapchain->createFrameBuffer(*renderPass());
}

void VulkanView::destroyFrameBuffers()
{
  if (_imgui)
    _imgui->destroyFrameBuffers();

  for (auto &framebuf : _frameBufs)
    vkDestroyFramebuffer(*_device, framebuf, nullptr);
  _frameBufs.clear();
}

void VulkanView::createCommandBuffers()
{
  int count = MaxConcurrentFrames;
  if (count != _cmdBufs.size()) {
    _device->destroyCommandBuffers(_cmdBufs);
    _cmdBufs = _device->createCommandBuffers(count);
  }
}

void VulkanView::recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t imageIndex)
{
  assert(imageIndex < _frameBufs.size());

  VkCommandBufferBeginInfo bufInfo = {};
  bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bufInfo.pNext = nullptr;

  VkClearValue clearValues[2];
  clearValues[0].color = {{0.0, 0.0, 0.0, 1.0}};
  clearValues[1].depthStencil = {1.f, 0};

  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.pNext = nullptr;
  renderPassBeginInfo.renderPass = *renderPass();
  renderPassBeginInfo.renderArea.offset.x = 0;
  renderPassBeginInfo.renderArea.offset.y = 0;
  renderPassBeginInfo.renderArea.extent.width = _w;
  renderPassBeginInfo.renderArea.extent.height = _h;
  renderPassBeginInfo.clearValueCount = 2;
  renderPassBeginInfo.pClearValues = clearValues;

  renderPassBeginInfo.framebuffer = _frameBufs[imageIndex];
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuf, &bufInfo));

    vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

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

    buildCommandBuffer(cmdBuf);

    vkCmdEndRenderPass(cmdBuf);

  VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuf));
}

void VulkanView::render()
{
  if (_frameFences[0] == VK_NULL_HANDLE)
    return;

  VkFence frameFence = _frameFences[_currentFrame];
  VK_CHECK_RESULT(vkWaitForFences(*_device, 1, &frameFence, VK_TRUE, UINT64_MAX));

  VkSemaphore imageAvailable = _imageSemaphores[_currentFrame];
  auto [result, index] = _swapchain->acquireImage(imageAvailable);
  if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
    VK_CHECK_RESULT(result);
  }

  VkCommandBuffer cmd = _cmdBufs[_currentFrame];
  VK_CHECK_RESULT(vkResetCommandBuffer(cmd, 0));
  recordCommandBuffer(cmd, index);

  if (_imgui) {
    _imgui->recordCommandBuffer(_currentFrame, index);
  }

  VK_CHECK_RESULT(vkResetFences(*_device, 1, &frameFence));

  VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.pWaitDstStageMask = &waitStageMask;   // Pointer to the list of pipeline stages that the semaphore waits will occur at
  submitInfo.waitSemaphoreCount = 1;               // One wait semaphore
  submitInfo.signalSemaphoreCount = 1;             // One signal semaphore

  int cmdcount = 1;
  VkCommandBuffer cmdbufs[2] = {cmd, 0};
  if (_imgui) {
    cmdcount = 2;
    cmdbufs[1] = _imgui->_cmdBufs[_currentFrame];
  }
  submitInfo.pCommandBuffers = cmdbufs;             // Command buffers(s) to execute in this batch (submission)
  submitInfo.commandBufferCount = cmdcount;         // One command buffer

  VkSemaphore renderFinished = _renderSemaphores[index];
  submitInfo.pWaitSemaphores = &imageAvailable;
  submitInfo.pSignalSemaphores = &renderFinished;

  auto queue =_device->graphicQueue(0);
  VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, frameFence));

  {
    _frameNum = index;
    auto present = _swapchain->queuePresent(queue, _frameNum, renderFinished);
    if (!((present == VK_SUCCESS) || (present == VK_SUBOPTIMAL_KHR))) {
      VK_CHECK_RESULT(present);
    }
  }

  _currentFrame = (_currentFrame + 1) % MaxConcurrentFrames;
}

void VulkanView::initialize()
{
    _swapchain = std::make_shared<VulkanSwapChain>(_device);

    _renderPass = std::make_unique<VulkanPass>(_device);
}

void VulkanView::checkFrame()
{
  createFrameBuffers();

  createCommandBuffers();

  createSyncObjs();
}

void VulkanView::clearFrame()
{
  destroyFrameBuffers();
}

void VulkanView::createSyncObjs()
{
  destroySyncObjs();

  const uint32_t count = _swapchain->imageCount();
  if (count == 0)
    return;

  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreCreateInfo.pNext = nullptr;

  _renderSemaphores.resize(count);
  for (auto &semaphore : _imageSemaphores)
    VK_CHECK_RESULT(vkCreateSemaphore(*_device, &semaphoreCreateInfo, nullptr, &semaphore));

  for (auto &semaphore : _renderSemaphores)
    VK_CHECK_RESULT(vkCreateSemaphore(*_device, &semaphoreCreateInfo, nullptr, &semaphore));

  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (auto &fence : _frameFences)
    VK_CHECK_RESULT(vkCreateFence(*_device, &fenceCreateInfo, nullptr, &fence));

  _currentFrame = 0;
}

void VulkanView::destroySyncObjs()
{
  for (auto &fence : _frameFences) {
    if (fence != VK_NULL_HANDLE) {
      vkDestroyFence(*_device, fence, nullptr);
      fence = VK_NULL_HANDLE;
    }
  }
  for (auto &semaphore : _imageSemaphores) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(*_device, semaphore, nullptr);
      semaphore = VK_NULL_HANDLE;
    }
  }

  for (auto semaphore : _renderSemaphores)
    vkDestroySemaphore(*_device, semaphore, nullptr);
  _renderSemaphores.clear();
}

void VulkanView::resizeImpl(int w, int h)
{
  if (w != _w && h != _h) {
    _w = w; _h = h;

    destroyFrameBuffers();
    _swapchain->realize(w, h, true);
    checkFrame();

    resize(w, h);
  }

  if (_imgui)
    _imgui->resize(w, h);
}
