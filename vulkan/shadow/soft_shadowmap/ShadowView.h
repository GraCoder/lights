#pragma once

#include "HUDPass.h"
#include "HUDPipeline.h"
#include "HUDRect.h"
#include "MeshInstance.h"
#include "RenderData.h"
#include "VulkanView.h"

class ShadowTechnique;

class ShadowView : public VulkanView
{
public:
  enum class ShadowType {
    PCF,
    VSM,
  };

  ShadowView(const std::shared_ptr<VulkanDevice> &dev);
  ~ShadowView();

  void setShadowType(ShadowType type);
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

  void recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t imageIndex) override;
  void buildCommandBuffer(VkCommandBuffer cmdBuf) override;
  void createPipeLayout();
  void createFrameBuffers();
  void destroyFrameBuffers() override;
  void createPipeline();

private:
  VkBuffer _vertBuf = VK_NULL_HANDLE;
  VkDeviceMemory _vertMem = VK_NULL_HANDLE;
  VkBuffer _indexBuf = VK_NULL_HANDLE;
  VkDeviceMemory _indexMem = VK_NULL_HANDLE;

  VkDescriptorPool _descriptPool = VK_NULL_HANDLE;

  std::unique_ptr<ShadowTechnique> _shadow;
  ShadowType _shadowType = ShadowType::PCF;
  bool _shadowRealized = false;

  VkDescriptorSet _matrixSet = VK_NULL_HANDLE;
  VkDescriptorSet _lightSet = VK_NULL_HANDLE;
  VkDescriptorSet _basicTexSet = VK_NULL_HANDLE;

  std::shared_ptr<VulkanBuffer> _uboBuf, _light;

  MVP _matrix;

  std::shared_ptr<MeshInstance> _model;

  std::shared_ptr<VulkanTexture> _basicTexture;

  std::vector<VkFramebuffer> _hudFrames;
  std::shared_ptr<HUDPass> _hudPass;
  std::shared_ptr<HUDPipeline> _hudPipeline;
  std::shared_ptr<HUDRect> _hudRect;

  tg::vec2 _lightDir = tg::vec2(90, 45);
};
