#pragma once

#include <vulkan/vulkan.h>
#include <SDL2/SDL_events.h>

#include <memory>
#include <vector>

class VulkanView;
class VulkanBuffer;

class VulkanImGUI{
  friend class VulkanView;
public:
  VulkanImGUI(VulkanView* view);
  ~VulkanImGUI();

  void resize(int w, int h);

  void create_pipeline(VkFormat clrformat);

  void check_frame(int n, VkFormat clrformat);

  bool update_frame();

  void draw(const VkCommandBuffer cmdbuf);

  bool mouse_down(SDL_MouseButtonEvent &ev);

  bool mouse_up(SDL_MouseButtonEvent &ev);

  bool mouse_move(SDL_MouseMotionEvent &ev);

private:
  void dirty();

  void create_canvas();

  void create_renderpass(VkFormat color);

  void build_command_buffers();

private:
  bool _initialized = false;
  bool _force_update = false;
  VulkanView *_view = 0;

  VkRenderPass _render_pass = VK_NULL_HANDLE;

  std::vector<VkFramebuffer> _frame_bufs;
  std::vector<VkCommandBuffer> _cmd_bufs;

  VkSampler _sampler = VK_NULL_HANDLE;
  VkDescriptorPool _descriptor_pool = VK_NULL_HANDLE;
  VkDescriptorSet _descriptor_set = VK_NULL_HANDLE;
  VkDescriptorSetLayout _descriptor_layout = VK_NULL_HANDLE;

  VkPipelineLayout _pipe_layout = VK_NULL_HANDLE;
  VkPipeline _pipeline = VK_NULL_HANDLE;

  std::shared_ptr<VulkanBuffer> _vert_buf, _vert_buf_bak;
  std::shared_ptr<VulkanBuffer> _index_buf, _index_buf_bak;

  VkImage _font_img;
  VkDeviceMemory _font_memory;
  VkImageView _font_view;
};