#pragma once

#include "RenderData.h"
#include "ShadowTechnique.h"

#include <memory>
#include <vector>

class ShadowPipeline;
class VulkanBuffer;
class VulkanDevice;
class VulkanImage;
class VulkanPass;
class VulkanPipeline;
class VulkanTexture;

class VSMShadow final : public ShadowTechnique
{
public:
  static constexpr uint32_t MapSize = 2048;

  explicit VSMShadow(const std::shared_ptr<VulkanDevice> &device);
  ~VSMShadow() override;

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
  void createSampler();

  std::shared_ptr<VulkanDevice> _device;
  std::shared_ptr<ShadowPipeline> _lightingPipeline;
  std::shared_ptr<VulkanPipeline> _casterPipeline;
  std::shared_ptr<VulkanPass> _pass;
  std::shared_ptr<VulkanImage> _momentsImage;
  std::shared_ptr<VulkanImage> _depthImage;
  std::shared_ptr<VulkanTexture> _texture;
  std::shared_ptr<VulkanBuffer> _depthMatrixBuffer;
  std::shared_ptr<VulkanBuffer> _shadowBuffer;
  std::vector<VkFramebuffer> _frameBuffers;
  VkDescriptorSet _depthMatrixSet = VK_NULL_HANDLE;
  VkDescriptorSet _shadowSet = VK_NULL_HANDLE;
  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
  VkSampler _sampler = VK_NULL_HANDLE;
  VkFormat _momentsFormat = VK_FORMAT_UNDEFINED;
  MVP _depthMatrix;
  ShadowMatrix _shadowMatrix;
  float _filterMode = 1.0f;
};
