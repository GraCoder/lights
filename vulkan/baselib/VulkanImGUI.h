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

  void createPipeline(VkFormat clrformat);

  void checkFrame(int n, VkFormat clrformat);

  bool updateFrame();

  void draw(const VkCommandBuffer cmdbuf);

  bool mouseDown(SDL_MouseButtonEvent &ev);

  bool mouseUp(SDL_MouseButtonEvent &ev);

  bool mouseMove(SDL_MouseMotionEvent &ev);

private:
  void dirty();

  void destroyFrameBuffers();

  void createCanvas();

  void createRenderpass(VkFormat color);

  void recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex);

private:
  bool _initialized = false;
  bool _forceUpdate = false;
  VulkanView *_view = 0;

  VkRenderPass _renderPass = VK_NULL_HANDLE;

  std::vector<VkFramebuffer> _frameBufs;
  std::vector<VkCommandBuffer> _cmdBufs;

  VkSampler _sampler = VK_NULL_HANDLE;
  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSet _descriptorSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout _descriptorLayout = VK_NULL_HANDLE;

  VkPipelineLayout _pipeLayout = VK_NULL_HANDLE;
  VkPipeline _pipeline = VK_NULL_HANDLE;

  std::shared_ptr<VulkanBuffer> _vertBuf, _vertBufBak;
  std::shared_ptr<VulkanBuffer> _indexBuf, _indexBufBak;

  VkImage _fontImg = VK_NULL_HANDLE;
  VkDeviceMemory _fontMemory = VK_NULL_HANDLE;
  VkImageView _fontView = VK_NULL_HANDLE;
};
