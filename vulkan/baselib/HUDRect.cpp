#include "HUDRect.h"
#include "HUDPipeline.h"
#include "tvec.h"
#include "VulkanTools.h"
#include "VulkanTexture.h"
#include "VulkanBuffer.h"

HUDRect::HUDRect(const std::shared_ptr<VulkanDevice>& dev) : _device(dev)
{
}

HUDRect::~HUDRect()
{
}

void HUDRect::setGeometry(float x, float y, float w, float h)
{
  std::vector<tg::vec2> vs; 
  vs.push_back({x, y});
  vs.push_back({x, y + h});
  vs.push_back({x + w, y + h});
  vs.push_back({x, y});
  vs.push_back({x + w, y + h});
  vs.push_back({x + w, y});

  vs.push_back({0, 0});
  vs.push_back({0, 1});
  vs.push_back({1, 1});
  vs.push_back({0, 0});
  vs.push_back({1, 1});
  vs.push_back({1, 0});

  int n = vs.size() * sizeof(tg::vec2);

  auto dst = _device->create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, n, 0);
  auto ori = _device->create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, n, vs.data());
  _device->copy_buffer(ori.get(), dst.get(), _device->transfer_queue());

  _buffer = dst;
}

void HUDRect::setTexture(HUDPipeline *pipeline, VulkanTexture *tex, VkDescriptorPool pool)
{
  auto des_layout = pipeline->texture_layout();
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &des_layout;

  VK_CHECK_RESULT(vkAllocateDescriptorSets(*_device, &allocInfo, &_set));

  auto descriptor = tex->descriptor();

  VkWriteDescriptorSet writeDescriptorSet = {};
  writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSet.dstSet = _set;
  writeDescriptorSet.descriptorCount = 1;
  writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writeDescriptorSet.pImageInfo = &descriptor;
  writeDescriptorSet.dstBinding = 0;
  vkUpdateDescriptorSets(*_device, 1, &writeDescriptorSet, 0, nullptr);
}

void HUDRect::fill_command(VkCommandBuffer cmdbuf, HUDPipeline *pipeline)
{
  vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipe_layout(), 0, 1, &_set, 0, 0);

  VkDeviceAddress offset[2] = {0, 6 * sizeof(tg::vec2)};
  VkBuffer bufs[2] = {};
  bufs[0] = *_buffer;
  bufs[1] = *_buffer;
  vkCmdBindVertexBuffers(cmdbuf, 0, 2, bufs, offset);
  vkCmdDraw(cmdbuf, 6, 1, 0, 0);
}
