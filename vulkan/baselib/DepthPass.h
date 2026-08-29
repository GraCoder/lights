#pragma once

#include "VulkanPass.h"

class DepthPass : public VulkanPass {
public:
  DepthPass(const std::shared_ptr<VulkanDevice> &dev);
  ~DepthPass();

protected:
  void initialize() override;
};