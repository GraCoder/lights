#include "VulkanImage.h"
#include "VulkanDevice.h"

VulkanImage::VulkanImage(std::shared_ptr<VulkanDevice> &dev) : _device(dev)
{
}

VulkanImage::~VulkanImage()
{
  if (_imageView) {
    vkDestroyImageView(*_device, _imageView, nullptr);
    _imageView = VK_NULL_HANDLE;
  }
  if (_image) {
    vkDestroyImage(*_device, _image, nullptr);
    _image = VK_NULL_HANDLE;
  }
  if (_imageMem) {
    vkFreeMemory(*_device, _imageMem, nullptr);
    _imageMem = VK_NULL_HANDLE;
  }

}

void VulkanImage::setImage(int w, int h, VkFormat format, VkDeviceMemory imgmem, VkImage img, VkImageView imgview)
{
  _w = w;
  _h = h;
  _format = format;
  _image = img;
  _imageView = imgview;
  _imageMem = imgmem;
}
