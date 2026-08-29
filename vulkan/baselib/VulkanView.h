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
class VulkanPass;
class Manipulator;

class VulkanView {
public:
  static constexpr uint32_t MaxConcurrentFrames = 2;

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

  virtual void destroyFrameBuffers();

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

protected:
  int _w, _h;
  uint32_t _frameNum = 0;
  std::shared_ptr<Manipulator> _manip;

  VkFormat _depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

  uint32_t _currentFrame = 0;

private:
  std::vector<VkFramebuffer> _frameBufs;

  std::array<VkSemaphore, MaxConcurrentFrames> _imageSemaphores{};
  std::array<VkFence, MaxConcurrentFrames> _frameFences{};
  std::vector<VkSemaphore> _renderSemaphores;
};

#endif
