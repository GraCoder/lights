#include "VulkanImage.h"
#include "VulkanDevice.h"

VulkanImage::VulkanImage(std::shared_ptr<VulkanDevice> &dev) : _device(dev)
{
}

VulkanImage::~VulkanImage()
{
  if (_image_view) {
    vkDestroyImageView(*_device, _image_view, nullptr);
    _image_view = VK_NULL_HANDLE;
  }
  if (_image) {
    vkDestroyImage(*_device, _image, nullptr);
    _image = VK_NULL_HANDLE;
  }
  if (_image_mem) {
    vkFreeMemory(*_device, _image_mem, nullptr);
    _image_mem = VK_NULL_HANDLE;
  }

}

void VulkanImage::setImage(int w, int h, VkFormat format, VkDeviceMemory imgmem, VkImage img, VkImageView imgview)
{
  _w = w;
  _h = h;
  _format = format;
  _image = img;
  _image_view = imgview;
  _image_mem = imgmem;
}
