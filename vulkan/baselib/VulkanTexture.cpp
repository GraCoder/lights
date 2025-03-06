#include "VulkanTexture.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanInitializers.hpp"
#include "VulkanTools.h"
#include "VulkanImage.h"

#include "stb_image.h"

VulkanTexture::VulkanTexture()
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

VkImageView VulkanTexture::image_view()
{
  if (_vimage)
    return _vimage->image_view();

  return _image_view;
}

VkImageLayout VulkanTexture::image_layout()
{
  return _image_layout;
}

VkDescriptorImageInfo VulkanTexture::descriptor()
{
  VkDescriptorImageInfo descriptor;
  descriptor.imageLayout = image_layout();
  descriptor.imageView = image_view();
  descriptor.sampler = sampler();
  return descriptor;
}

void VulkanTexture::load_image(const std::string& file)
{
  int x = 0, y = 0, n = 0;
  uint8_t* data = stbi_load(file.c_str(), &x, &y, &n, 0);
}

void VulkanTexture::set_image(int w, int h, int channel, int channel_depth, uint8_t*data, int n)
{
  _w = w;
  _h = h;
  _channel = channel;
  _channel_depth = channel_depth;
  _data.resize(n);
  memcpy(_data.data(), data, n);
  return;
}

void VulkanTexture::set_image(int w, int h, const tg::Tvec4<uint8_t> &clr)
{
  _w = w;
  _h = h;
  _data.resize(w * h * 4);
  for (int i = 0; i < w * h; i++) 
    memcpy(&_data[i << 2], &clr, sizeof(tg::Tvec4<uint8_t>));
}

void VulkanTexture::realize(const std::shared_ptr<VulkanDevice>& dev)
{
  if (_sampler)
    return;

  _device = dev;
  auto buf = _device->create_buffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, _data.size(), (void*)_data.data());
  auto [img, mem] = _device->create_image(_w, _h);
  _image = img;
  _image_mem = mem;

  VkBufferImageCopy buffer_region = {};
  buffer_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  buffer_region.imageSubresource.mipLevel = 0;
  buffer_region.imageSubresource.baseArrayLayer = 0;
  buffer_region.imageSubresource.layerCount = 1;
  buffer_region.imageExtent.width = _w;
  buffer_region.imageExtent.height = _h;
  buffer_region.imageExtent.depth = 1;
  buffer_region.bufferOffset = 0;
  buffer_region.bufferImageHeight = _h;
  buffer_region.bufferRowLength = _w;

  VkImageSubresourceRange subrange = {};
  subrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subrange.baseMipLevel = 0;
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

void VulkanTexture::realize(const std::shared_ptr<VulkanImage> &img)
{
  _vimage = img;
  _device = _vimage->device();

  auto samplerinfo = vks::initializers::samplerCreateInfo();
  samplerinfo.maxLod = 1;
  //samplerinfo.magFilter = VK_FILTER_NEAREST;
  //samplerinfo.minFilter = VK_FILTER_NEAREST;
  VkSampler sampler;
  VK_CHECK_RESULT(vkCreateSampler(*_device, &samplerinfo, nullptr, &sampler));
  _sampler = sampler;
}
