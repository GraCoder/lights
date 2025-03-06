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


using tg::vec2;
using tg::vec3;
using tg::mat4;

VulkanView::VulkanView(const std::shared_ptr<VulkanDevice>& dev, bool overlay) : _device(dev)
{
  initialize();

  if(overlay)
    _imgui = std::make_shared<VulkanImGUI>(this);
}

VulkanView::~VulkanView()
{
  vkDeviceWaitIdle(*_device);

  _imgui.reset();
  for (auto fence : _fences)
    vkDestroyFence(*_device, fence, nullptr);

  vkDestroySemaphore(*_device, _presentSemaphore, nullptr);
  vkDestroySemaphore(*_device, _renderSemaphore, nullptr);

  _swapchain.reset();

  _device->destroy_command_buffers(_cmd_bufs);

  clear_frame();

  //auto surface = _swapchain->surface();
  //if (surface) {
  //  vkDestroySurfaceKHR(VulkanInstance::instance(), surface, nullptr);
  //}
}

void VulkanView::set_surface(VkSurfaceKHR surface, int w, int h)
{
  _swapchain->set_surface(surface);
  _swapchain->realize(w, h, true);

  if (_imgui) _imgui->create_pipeline(_swapchain->color_format());

  resize_impl(w, h);
}

void VulkanView::update_overlay()
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
            resize_impl(w, h);
            update_frame();
          }
          break;
        case SDL_USEREVENT:
          if (event.user.code == WM_PAINT)
            render();
          break;
        case SDL_MOUSEBUTTONDOWN:
          if (_imgui && _imgui->mouse_down(event.button)) {
          } else {
            if (event.button.button == 1)
              left_dn(event.button.x, event.button.y);
          }
          update_frame();
          break;
        case SDL_MOUSEBUTTONUP:
          if (_imgui && _imgui->mouse_up(event.button)) {
          } else {
            if (event.button.button == 1)
              left_up(event.button.x, event.button.y);
          }
          update_frame();
          break;
        case SDL_MOUSEMOTION:
          if (_imgui && _imgui->mouse_move(event.motion)) {
          } else {
            if (event.motion.state & SDL_BUTTON_LMASK) {
              _manip.rotate(event.motion.xrel, event.motion.yrel);
              left_drag(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
            } else if (event.motion.state & SDL_BUTTON_MMASK) {
            } else if (event.motion.state & SDL_BUTTON_RMASK) {
              _manip.translate(event.motion.xrel, -event.motion.yrel);
              right_drag(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
            }
          }
          update_frame();
          break;
        case SDL_MOUSEWHEEL: {
          _manip.zoom(event.wheel.y);
          wheel(event.wheel.y);
          update_frame();
          break;
        }
        case SDL_KEYUP: {
          if (event.key.keysym.scancode == SDL_SCANCODE_SPACE)
            _manip.home();
          key_up(event.key.keysym.scancode);
          update_frame();
        } break;
        default:
          break;
      }
    }
  }
}

void VulkanView::set_render_pass(VkRenderPass render_pass)
{
}

void VulkanView::set_frame_buffers(const std::vector<VkFramebuffer>& frame_bufs)
{
  for (auto& frame : _frame_bufs) {
    if (frame) {
      vkDestroyFramebuffer(*_device, frame, nullptr);
      frame = VK_NULL_HANDLE;
    }
  }
  _frame_bufs = frame_bufs;
}

uint32_t VulkanView::frame_count()
{
  return _swapchain->image_count();
}

void VulkanView::update_frame()
{
  update_scene();

  SDL_Event ev;
  ev.type = SDL_USEREVENT;
  ev.user.code = WM_PAINT;
  SDL_PushEvent(&ev);

  if (_imgui && _imgui->update_frame()) {
    VK_CHECK_RESULT(vkWaitForFences(*_device, 1, &_fences[_framenum], VK_TRUE, UINT64_MAX));
    _imgui->build_command_buffers();
  }
}

void VulkanView::create_frame_buffers()
{
  #if 0
  std::vector<VkImageView> imgs;
  for(int i = 0; i < count; i++) {
    auto img = _device->create_color_image(_w, _h);
    _images.push_back(img);
    imgs.push_back(img->image_view());
  }
  _frame_bufs = _swapchain->create_frame_buffer(_render_pass, imgs, _depth->image_view());
  #else
  _frame_bufs = _swapchain->create_frame_buffer(*render_pass(), _depth->image_view());
  #endif

}

void VulkanView::create_command_buffers()
{
  int count = _swapchain->image_count();
  if (count != _cmd_bufs.size()) {
    _cmd_bufs = _device->create_command_buffers(count);
  }
}

void VulkanView::build_command_buffers()
{
  auto& framebuffers = _frame_bufs;
  auto& renderPass = _render_pass;
  vkDeviceWaitIdle(*_device);

  assert(framebuffers.size() == _cmd_bufs.size());

  VkCommandBufferBeginInfo buf_info = {};
  buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  buf_info.pNext = nullptr;

  VkClearValue clearValues[2];
  clearValues[0].color = {{0.0, 0.0, 0.0, 1.0}};
  clearValues[1].depthStencil = {1.f, 0};

  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.pNext = nullptr;
  renderPassBeginInfo.renderPass = *render_pass();
  renderPassBeginInfo.renderArea.offset.x = 0;
  renderPassBeginInfo.renderArea.offset.y = 0;
  renderPassBeginInfo.renderArea.extent.width = _w;
  renderPassBeginInfo.renderArea.extent.height = _h;
  renderPassBeginInfo.clearValueCount = 2;
  renderPassBeginInfo.pClearValues = clearValues;

  for (int i = 0; i < _cmd_bufs.size(); i++) {
    renderPassBeginInfo.framebuffer = framebuffers[i];
    auto& cmd_buf = _cmd_bufs[i];
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmd_buf, &buf_info));

    vkCmdBeginRenderPass(cmd_buf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

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

    build_command_buffer(cmd_buf);

    vkCmdEndRenderPass(cmd_buf);

    VK_CHECK_RESULT(vkEndCommandBuffer(cmd_buf));
  }
}

void VulkanView::render()
{
  // vkWaitForFences(*_device, 1, _fences[]);
  auto [result, index] = _swapchain->acquire_image(_presentSemaphore);
  if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
    VK_CHECK_RESULT(result);
  }

  VK_CHECK_RESULT(vkWaitForFences(*_device, 1, &_fences[index], VK_TRUE, UINT64_MAX));
  VK_CHECK_RESULT(vkResetFences(*_device, 1, &_fences[index]));

  VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.pWaitDstStageMask = &waitStageMask;   // Pointer to the list of pipeline stages that the semaphore waits will occur at
  submitInfo.waitSemaphoreCount = 1;               // One wait semaphore
  submitInfo.signalSemaphoreCount = 1;             // One signal semaphore

  int cmdcount = 1;
  VkCommandBuffer cmdbufs[2] = {_cmd_bufs[index], 0};
  if (_imgui) {
    cmdcount = 2;
    cmdbufs[1] = _imgui->_cmd_bufs[index];
  }
  submitInfo.pCommandBuffers = cmdbufs;             // Command buffers(s) to execute in this batch (submission)
  submitInfo.commandBufferCount = cmdcount;         // One command buffer

  submitInfo.pWaitSemaphores = &_presentSemaphore;   // Semaphore(s) to wait upon before the submitted command buffer starts executing
  submitInfo.pSignalSemaphores = &_renderSemaphore;  // Semaphore(s) to be signaled when command buffers have completed

  auto queue =_device->graphic_queue(0);
  VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, _fences[index]));

  {
    _framenum = index;
    auto present = _swapchain->queue_present(queue, _framenum, _renderSemaphore);
    if (!((present == VK_SUCCESS) || (present == VK_SUBOPTIMAL_KHR))) {
      VK_CHECK_RESULT(present);
    }
  }
}

void VulkanView::initialize()
{
    _manip.set_home(vec3(0, -30, 0), vec3(0), vec3(0, 0, 1));

    _swapchain = std::make_shared<VulkanSwapChain>(_device);

    create_sync_objs();

    _render_pass = std::make_unique<VulkanPass>(_device);
}

void VulkanView::check_frame()
{
  _depth = _device->create_depth_image(_w, _h, _depth_format);

  create_frame_buffers();

  create_command_buffers();

  build_command_buffers();

  if (_fences.size() != _cmd_bufs.size()) {
    for (auto fence : _fences)
      vkDestroyFence(*_device, fence, nullptr);
    _fences = _device->create_fences(_cmd_bufs.size());
  }
}

void VulkanView::clear_frame()
{
  for (auto& framebuf : _frame_bufs)
    vkDestroyFramebuffer(*_device, framebuf, nullptr);
  _frame_bufs.clear();

  _depth.reset();
}

void VulkanView::create_sync_objs()
{
  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreCreateInfo.pNext = nullptr;

  // Semaphore used to ensure that image presentation is complete before starting to submit again
  VK_CHECK_RESULT(vkCreateSemaphore(*_device, &semaphoreCreateInfo, nullptr, &_presentSemaphore));

  // Semaphore used to ensure that all commands submitted have been finished before submitting the image to the queue
  VK_CHECK_RESULT(vkCreateSemaphore(*_device, &semaphoreCreateInfo, nullptr, &_renderSemaphore));
}

void VulkanView::resize_impl(int w, int h)
{
  if (w != _w && h != _h) {
    _w = w; _h = h;

    _swapchain->realize(w, h, true);

    clear_frame();
    check_frame();

    resize(w, h);
  }

  if (_imgui)
    _imgui->resize(w, h);
}
