#include "VulkanTexture.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanInitializers.hpp"
#include "VulkanTools.h"

#include "stb_image.h"

VulkanTexture::VulkanTexture(std::shared_ptr<VulkanDevice>& dev) : _device(dev)
{
}

VulkanTexture::~VulkanTexture()
{
  if (_image)
    vkDestroyImage(*_device, _image, nullptr);
  if (_image_view)
    vkDestroyImageView(*_device, _image_view, nullptr);
  if (_image_mem)
    vkFreeMemory(*_device, _image_mem, nullptr);
  if (_sampler)
    vkDestroySampler(*_device, _sampler, nullptr);
}

void VulkanTexture::load_image(const std::string& file)
{
  int x = 0, y = 0, n = 0;
  uint8_t* data = stbi_load(file.c_str(), &x, &y, &n, 0);
}

void VulkanTexture::load_data(int w, int h, int comp, int compsize, uint8_t*data, int n)
{
  auto buf = _device->create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, n, (void *)data);
  auto [img, mem] = _device->create_image(w, h);
  _image = img; _image_mem = mem;

  VkBufferImageCopy buffer_region = {};
  buffer_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  buffer_region.imageSubresource.mipLevel = 0;
  buffer_region.imageSubresource.baseArrayLayer = 0;
  buffer_region.imageSubresource.layerCount = 1;
  buffer_region.imageExtent.width = w;
  buffer_region.imageExtent.height = h;
  buffer_region.imageExtent.depth = 1;
  buffer_region.bufferOffset = 0;
  buffer_region.bufferImageHeight = h;
  buffer_region.bufferRowLength = w ;

  VkImageSubresourceRange subrange = {};
  subrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subrange.baseMipLevel = 0;
  //layoutBinding[2].binding = 0;
  //layoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  //layoutBinding[2].descriptorCount = 1;
  //layoutBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  //layoutBinding[2].pImmutableSamplers = nullptr;

  subrange.levelCount = 1;
  subrange.layerCount = 1;

  auto cmdbuf = _device->create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

  VkImageMemoryBarrier imgbarrier = vks::initializers::imageMemoryBarrier();
  imgbarrier.image = img;
  imgbarrier.subresourceRange = subrange;
  imgbarrier.srcAccessMask = 0;
  imgbarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imgbarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imgbarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgbarrier);

  vkCmdCopyBufferToImage(cmdbuf, *buf, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_region);

  imgbarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imgbarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  imgbarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imgbarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgbarrier);

  _device->flush_command_buffer(cmdbuf, _device->transfer_queue());

  auto samplerinfo = vks::initializers::samplerCreateInfo();
  samplerinfo.maxLod = 1;
  VkSampler sampler;
  VK_CHECK_RESULT(vkCreateSampler(*_device, &samplerinfo, nullptr, &sampler));
  _sampler = sampler;

  auto view = _device->create_image_view(img);
  _image_view = view;
}