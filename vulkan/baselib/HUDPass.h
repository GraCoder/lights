#pragma once

#include "VulkanPass.h"

class HUDPass : public VulkanPass {
public:
  HUDPass(const std::shared_ptr<VulkanDevice> &dev);
  ~HUDPass();

protected:
  void initialize() override;
};