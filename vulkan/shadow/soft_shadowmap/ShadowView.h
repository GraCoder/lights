#pragma once

#include "VulkanView.h"
#include "ShadowPipeline.h"
#include "RenderData.h"
#include "MeshInstance.h"
#include "DepthPipeline.h"
#include "DepthPass.h"
#include "HUDPass.h"
#include "HUDPipeline.h"
#include "HUDRect.h"

class ShadowView : public VulkanView {
public:
  ShadowView(const std::shared_ptr<VulkanDevice> &dev);
  ~ShadowView();

  void setUniforms();
  void updateUbo();
  void updateLight();

  void resize(int w, int h);
  void updateScene();

  void wheel(int delta) { updateUbo(); }
  void leftDrag(int x, int y, int, int) { updateUbo(); }
  void rightDrag(int x, int y, int, int) { updateUbo(); }
  void keyUp(int key);

  void createCommandBuffers();
  void buildDepthCommandBuffer(VkCommandBuffer cmdBuf);

  void recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t imageIndex) override;
  void buildCommandBuffer(VkCommandBuffer cmdBuf) override;
  void createPipeLayout();
  void createFrameBuffers();
  void createPipeline();

private:
  VkBuffer _vertBuf = VK_NULL_HANDLE;
  VkDeviceMemory _vertMem = VK_NULL_HANDLE;
  VkBuffer _indexBuf = VK_NULL_HANDLE;
  VkDeviceMemory _indexMem = VK_NULL_HANDLE;

  VkDescriptorPool _descriptPool = VK_NULL_HANDLE;

  std::vector<VkFramebuffer> _depthFrames;

  std::shared_ptr<DepthPass> _depthPass;

  std::shared_ptr<ShadowPipeline> _shadowPipeline;
  std::shared_ptr<DepthPipeline> _depthPipeline;

  std::shared_ptr<VulkanImage> _depthImage;

  VkDescriptorSet _matrixSet = VK_NULL_HANDLE;
  VkDescriptorSet _lightSet = VK_NULL_HANDLE;
  VkDescriptorSet _basicTexSet = VK_NULL_HANDLE;

  VkDescriptorSet _depthMatrixSet = VK_NULL_HANDLE;
  std::shared_ptr<VulkanBuffer> _depthMatrixBuf;

  VkDescriptorSet _shadowSet = VK_NULL_HANDLE;
  std::shared_ptr<VulkanBuffer> _shadowBuf;
  std::shared_ptr<VulkanTexture> _shadowTexture;

  std::shared_ptr<VulkanBuffer> _uboBuf, _light;

  MVP _matrix, _depthMatrix;
  ShadowMatrix _shadowMatrix;
  float _pcfMode = 1;

  std::shared_ptr<MeshInstance> _model;

  std::shared_ptr<VulkanTexture> _basicTexture;

  std::vector<VkFramebuffer> _hudFrames;
  std::shared_ptr<HUDPass> _hudPass;
  std::shared_ptr<HUDPipeline> _hudPipeline;
  std::shared_ptr<HUDRect> _hudRect;

  tg::vec2 _lightDir = tg::vec2(90, 45);
};
