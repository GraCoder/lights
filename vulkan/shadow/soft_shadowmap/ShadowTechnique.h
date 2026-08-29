#pragma once

#include <vulkan/vulkan_core.h>

class MeshInstance;
class VulkanPass;
class VulkanTexture;

class ShadowTechnique
{
public:
  virtual ~ShadowTechnique() = default;

  virtual VkDescriptorSetLayout matrixLayout() const = 0;
  virtual VkDescriptorSetLayout lightLayout() const = 0;
  virtual VkPipelineLayout lightingPipelineLayout() const = 0;
  virtual VulkanTexture *debugTexture() const = 0;

  virtual void initializeUniforms() = 0;
  virtual void realize(VulkanPass *renderPass, VkDescriptorPool descriptorPool) = 0;
  virtual void createFrameBuffers(uint32_t count) = 0;
  virtual void destroyFrameBuffers() = 0;
  virtual void recordShadowPass(VkCommandBuffer cmdBuf, uint32_t frameIndex, MeshInstance &model) = 0;
  virtual void bindLighting(VkCommandBuffer cmdBuf, VkDescriptorSet matrixSet, VkDescriptorSet lightSet) = 0;
  virtual void toggleFilterMode() = 0;
  virtual bool valid() const = 0;
};
