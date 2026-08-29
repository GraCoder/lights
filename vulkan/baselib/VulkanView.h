#ifndef __VULKAN_VIEW_INC__
#define __VULKAN_VIEW_INC__

class VulkanDevice;

#include <memory>
#include <array>
#include <vector>

#include "vulkan/vulkan.h"
#include "VulkanDef.h"

class VulkanSwapChain;
class VulkanImGUI;
class VulkanImage;
class VulkanPass;
class Manipulator;

class VulkanView {
public:
  static constexpr uint32_t MaxConcurrentFrames = 3;

  VulkanView(const std::shared_ptr<VulkanDevice> &dev, bool overlay = true);

  virtual ~VulkanView();

  void setSurface(VkSurfaceKHR surface, int w, int h);

  void updateOverlay();

  void frame(bool continus = true);

  VulkanDevice *device() { return _device.get(); }

  VulkanSwapChain *swapchain() { return _swapchain.get(); }

  VulkanPass* renderPass() { return _renderPass.get(); }

  void setRenderPass(VkRenderPass renderPass);

  const std::vector<VkFramebuffer> &frameBuffers() { return _frameBufs; }

  void setFrameBuffers(const std::vector<VkFramebuffer> &frameBufs);

  uint32_t frameCount();

  std::shared_ptr<Manipulator> &manipulator() { return _manip; }
  const std::shared_ptr<Manipulator> &manipulator() const { return _manip; }

  int width() { return _w; }
  int height() { return _h; }

  virtual void updateScene(){};
  virtual void resize(int w, int h) = 0;
  virtual void buildCommandBuffer(VkCommandBuffer cmdBuf) = 0;

  virtual void leftDn(int x, int y){};
  virtual void leftUp(int x, int y){};
  virtual void wheel(int delta){};
  virtual void leftDrag(int x, int y, int xdel, int ydel){};
  virtual void rightDrag(int x, int y, int xdel, int ydel){};
  virtual void keyUp(int){};

  virtual void render();

protected:

  void updateFrame();

  virtual void createFrameBuffers();

  virtual void createCommandBuffers();

  virtual void recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t imageIndex);

private:
  void initialize();

  void checkFrame();

  void clearFrame();

  void createSyncObjs();

  void destroySyncObjs();

  void resizeImpl(int w, int h);

protected:
  std::shared_ptr<VulkanDevice> _device;
  std::shared_ptr<VulkanSwapChain> _swapchain;
  std::shared_ptr<VulkanPass> _renderPass;

  std::shared_ptr<VulkanImGUI> _imgui = 0;

  std::vector<VkCommandBuffer> _cmdBufs;

  int _w, _h;
  uint32_t _framenum = 0;
  uint32_t _currentFrame = 0;

  VkFormat _depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

  std::shared_ptr<VulkanImage> _depth = {VK_NULL_HANDLE};
  std::vector<std::shared_ptr<VulkanImage>> _images;

  std::shared_ptr<Manipulator> _manip;

private:
  std::vector<VkFramebuffer> _frameBufs;

  std::array<VkSemaphore, MaxConcurrentFrames> _imageAvailableSemaphores{};
  std::vector<VkSemaphore> _renderFinishedSemaphores;
  std::array<VkFence, MaxConcurrentFrames> _frameFences{};
};

#endif
