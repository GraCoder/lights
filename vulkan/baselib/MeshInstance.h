#pragma once

#include <vulkan/vulkan_core.h>
#include <vector>
#include <memory>

#include "tvec.h"
#include "RenderData.h"

class VulkanDevice;
class MeshPrimitive;
class VulkanBuffer;
class VulkanPipeline;
class TexturePipeline;
class DepthPersPipeline;

class MeshInstance{
public:
  MeshInstance();
  ~MeshInstance();

  void set_transform(const tg::mat4 &);

  void add_primitive(std::shared_ptr<MeshPrimitive> &pri);

  void realize(const std::shared_ptr<VulkanDevice> &dev);

  void realize(const std::shared_ptr<VulkanDevice> &dev, const std::shared_ptr<TexturePipeline> &pipeline);

  void build_command_buffer(VkCommandBuffer cmd_buf, const std::shared_ptr<VulkanPipeline> &pipeline);

  void build_command_buffer(VkCommandBuffer cmd_buf, const std::shared_ptr<TexturePipeline> &pipeline);

  void build_command_buffer(VkCommandBuffer cmd_buf, const std::shared_ptr<DepthPersPipeline> &pipeline);

private:

private:
  std::shared_ptr<VulkanDevice> _device;

  PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;

  tg::mat4 _transform;

  std::vector<std::shared_ptr<MeshPrimitive>> _pris;

  std::shared_ptr<VulkanBuffer> _pbr_buf;

  VkDescriptorSet _pbr_set = VK_NULL_HANDLE;
};