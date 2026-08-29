#pragma once

#include "RenderData.h"
#include "ShadowTechnique.h"

#include <memory>
#include <vector>

class DepthPass;
class DepthPipeline;
class ShadowPipeline;
class VulkanBuffer;
class VulkanDevice;
class VulkanImage;
class VulkanTexture;

class PCFShadow final : public ShadowTechnique
{
public:
  static constexpr uint32_t MapSize = 2048;
  static constexpr float MapWorldSize = 10.0f;

  explicit PCFShadow(const std::shared_ptr<VulkanDevice> &device);
  ~PCFShadow() override;

  VkDescriptorSetLayout matrixLayout() const override;
  VkDescriptorSetLayout lightLayout() const override;
  VkPipelineLayout lightingPipelineLayout() const override;
  VulkanTexture *debugTexture() const override;

  void initializeUniforms() override;
  void realize(VulkanPass *renderPass, VkDescriptorPool descriptorPool) override;
  void createFrameBuffers(uint32_t count) override;
  void destroyFrameBuffers() override;
  void recordShadowPass(VkCommandBuffer cmdBuf, uint32_t frameIndex, MeshInstance &model) override;
  void bindLighting(VkCommandBuffer cmdBuf, VkDescriptorSet matrixSet, VkDescriptorSet lightSet) override;
  void toggleFilterMode() override;
  bool valid() const override;

private:
  template <typename T>
  void writeBuffer(VulkanBuffer &buffer, const T &value);

  VkDescriptorSet allocateSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout layout);
  void writeUniformDescriptor(VkDescriptorSet set, uint32_t binding, VulkanBuffer &buffer, VkDeviceSize size);
  void createCompareSampler();

  std::shared_ptr<VulkanDevice> _device;
  std::shared_ptr<ShadowPipeline> _lightingPipeline;
  std::shared_ptr<DepthPipeline> _casterPipeline;
  std::shared_ptr<DepthPass> _pass;
  std::shared_ptr<VulkanImage> _image;
  std::shared_ptr<VulkanTexture> _texture;
  std::shared_ptr<VulkanBuffer> _depthMatrixBuffer;
  std::shared_ptr<VulkanBuffer> _shadowBuffer;
  std::vector<VkFramebuffer> _frameBuffers;
  VkDescriptorSet _depthMatrixSet = VK_NULL_HANDLE;
  VkDescriptorSet _shadowSet = VK_NULL_HANDLE;
  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
  VkSampler _compareSampler = VK_NULL_HANDLE;
  MVP _depthMatrix;
  ShadowMatrix _shadowMatrix;
  float _filterMode = 1.0f;
};
