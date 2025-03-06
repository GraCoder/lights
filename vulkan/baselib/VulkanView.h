#ifndef __VULKAN_VIEW_INC__
#define __VULKAN_VIEW_INC__

class VulkanDevice;

#include <memory>
#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanDef.h"
#include "Manipulator.h"

class VulkanSwapChain;
class VulkanImGUI;
class VulkanImage;
class VulkanPass;

class VulkanView {
public:
  VulkanView(const std::shared_ptr<VulkanDevice> &dev, bool overlay = true);

  virtual ~VulkanView();

  void set_surface(VkSurfaceKHR surface, int w, int h);

  void update_overlay();

  void frame(bool continus = true);

  VulkanDevice *device() { return _device.get(); }

  VulkanSwapChain *swapchain() { return _swapchain.get(); }

  VulkanPass* render_pass() { return _render_pass.get(); }

  void set_render_pass(VkRenderPass render_pass);

  const std::vector<VkFramebuffer> &frame_buffers() { return _frame_bufs; } 

  void set_frame_buffers(const std::vector<VkFramebuffer> &frame_bufs);

  uint32_t frame_count();

  Manipulator &manipulator() { return _manip; }

  int width() { return _w; }
  int height() { return _h; }

  virtual void update_scene(){};
  virtual void resize(int w, int h) = 0;
  virtual void build_command_buffer(VkCommandBuffer cmd_buf) = 0;

  virtual void left_dn(int x, int y){};
  virtual void left_up(int x, int y){};
  virtual void wheel(int delta){};
  virtual void left_drag(int x, int y, int xdel, int ydel){};
  virtual void right_drag(int x, int y, int xdel, int ydel){};
  virtual void key_up(int){};

  virtual void render();

protected:

  void update_frame();

  virtual void create_frame_buffers();

  virtual void create_command_buffers();

  virtual void build_command_buffers();

private:
  void initialize();

  void check_frame();

  void clear_frame();

  void create_sync_objs();

  void resize_impl(int w, int h);

protected:
  std::shared_ptr<VulkanDevice> _device;
  std::shared_ptr<VulkanSwapChain> _swapchain;
  std::shared_ptr<VulkanPass> _render_pass;

  std::shared_ptr<VulkanImGUI> _imgui = 0;

  std::vector<VkCommandBuffer> _cmd_bufs;

  int _w, _h;
  uint32_t _framenum = 0;

  VkFormat _depth_format = VK_FORMAT_D24_UNORM_S8_UINT;

  std::shared_ptr<VulkanImage> _depth = {VK_NULL_HANDLE};
  std::vector<std::shared_ptr<VulkanImage>> _images;

private:
  std::vector<VkFramebuffer> _frame_bufs;

  VkSemaphore _presentSemaphore = VK_NULL_HANDLE;
  VkSemaphore _renderSemaphore = VK_NULL_HANDLE;
  std::vector<VkFence> _fences;

  Manipulator _manip;
};

#endif