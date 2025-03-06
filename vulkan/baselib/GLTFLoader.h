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

  std::shared_ptr<MeshInstance> load_file(const std::string &file);

private:

  std::shared_ptr<MeshPrimitive> create_primitive(const tinygltf::Primitive *pri);

  VkFormat attr_format(const tinygltf::Accessor *acc);

private:
  std::shared_ptr<tinygltf::Model> _m;
};
