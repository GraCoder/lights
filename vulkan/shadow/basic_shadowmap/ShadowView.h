#pragma once

#include "VulkanView.h"
#include "ShadowPipeline.h"
#include "RenderData.h"
#include "MeshInstance.h"
#include "DepthPipeline.h"
#include "DepthPass.h"

class ShadowView : public VulkanView {
public:
  ShadowView(const std::shared_ptr<VulkanDevice> &dev);
  ~ShadowView();

  void create_sphere();

  void set_uniforms();
  void update_ubo();

  void resize(int w, int h);
  void update_scene();

  void wheel(int delta) { update_ubo(); }
  void left_drag(int x, int y, int, int) { update_ubo(); }
  void right_drag(int x, int y, int, int) { update_ubo(); }
  void key_up(int key);

  void create_command_buffers();
  void build_depth_command_buffer(VkCommandBuffer cmd_buf);

  void build_command_buffers();
  void build_command_buffer(VkCommandBuffer cmd_buf) override;
  void create_pipe_layout();
  void create_frame_buffers();
  void create_pipeline();

private:
  VkBuffer _vert_buf;
  VkDeviceMemory _vert_mem;
  VkBuffer _index_buf;
  VkDeviceMemory _index_mem;

  VkDescriptorPool _descript_pool = VK_NULL_HANDLE;

  std::vector<VkFramebuffer> _depth_frames;

  std::shared_ptr<DepthPass> _depth_pass;

  std::shared_ptr<ShadowPipeline> _shadow_pipeline;
  std::shared_ptr<DepthPipeline> _depth_pipeline;

  std::shared_ptr<VulkanImage> _depth_image;

  VkDescriptorSet _matrix_set = VK_NULL_HANDLE;
  VkDescriptorSet _light_set = VK_NULL_HANDLE;
  VkDescriptorSet _pbr_set = VK_NULL_HANDLE;
  VkDescriptorSet _basic_tex_set = VK_NULL_HANDLE;

  VkDescriptorSet _depth_matrix_set = VK_NULL_HANDLE;
  std::shared_ptr<VulkanBuffer> _depth_matrix_buf;

  VkDescriptorSet _shadow_set = VK_NULL_HANDLE;
  std::shared_ptr<VulkanBuffer> _shadow_buf;
  std::shared_ptr<VulkanTexture> _shadow_texture;

  std::shared_ptr<VulkanBuffer> _ubo_buf, _light, _material;

  MVP _matrix, _depth_matrix;

  uint32_t _vert_count = 0;
  uint32_t _index_count = 0;

  std::shared_ptr<MeshInstance> _tree, _deer;

  std::shared_ptr<VulkanTexture> _basic_texture;
};