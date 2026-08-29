#pragma once

#include <string>
#include <memory>
#include <vulkan/vulkan_core.h>

#include "tvec.h"

namespace tinygltf{
  class Model;
  class Accessor;
  class BufferView;
  class Primitive;
}

class VulkanDevice;

class MeshPrimitive;
class MeshInstance;

class GLTFLoader{
public:
  GLTFLoader();
  ~GLTFLoader();

  std::shared_ptr<MeshInstance> loadFile(const std::string &file);

private:

  std::shared_ptr<MeshPrimitive> createPrimitive(const tinygltf::Primitive *pri);

  VkFormat attrFormat(const tinygltf::Accessor *acc);

private:
  std::shared_ptr<tinygltf::Model> _m;
};
