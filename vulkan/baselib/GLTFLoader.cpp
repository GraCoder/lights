#include "GLTFLoader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
#include "MeshInstance.h"
#include "MeshPrimitive.h"
#include "VulkanTexture.h"
#include "tmath.h"
#include "RenderData.h"

#include <set>

GLTFLoader::GLTFLoader() 
{
}

GLTFLoader::~GLTFLoader()
{
}

std::shared_ptr<MeshInstance> GLTFLoader::load_file(const std::string& file)
{
  tinygltf::TinyGLTF gltf;
  std::string err, warn;
  _m = std::make_shared<tinygltf::Model>();
  if (!gltf.LoadASCIIFromFile(_m.get(), &err, &warn, file))
    return nullptr;
  //_m->scenes;

  std::vector<Material> materials;
  materials.resize(_m->materials.size());

  for (int i = 0; i < _m->materials.size(); i++) {
    Material &m = materials[i];
    m.pbrdata.ao = 1;
    auto &material = _m->materials[i];
    auto &color = material.pbrMetallicRoughness.baseColorFactor;
    m.pbrdata.albedo = tg::vec4(color[0], color[1], color[2], color[3]);
    m.pbrdata.metallic = material.pbrMetallicRoughness.metallicFactor;
    m.pbrdata.roughness = material.pbrMetallicRoughness.roughnessFactor;

    material.pbrMetallicRoughness.baseColorTexture.texCoord;
    int idx = material.pbrMetallicRoughness.baseColorTexture.index;

    if (idx == -1)
      continue;
    auto &tex = _m->textures[idx];
    auto &img = _m->images[tex.source];
    if (tinygltf::IsDataURI(img.uri) || img.image.size() > 0) {}

    auto texture =std::make_shared<VulkanTexture>();
    
    if(img.image.size() > 0) {
      texture->set_image(img.width, img.height, img.component, img.bits, img.image.data(), img.image.size());
    } else {
      auto &bufview = _m->bufferViews[img.bufferView];
      auto &buf = _m->buffers[bufview.buffer];
      texture->set_image(img.width, img.height, img.component, img.bits, buf.data.data() + bufview.byteOffset, bufview.byteLength);
    }

    m.albedo_tex = texture;
  }

  auto meshInst = std::make_shared<MeshInstance>();
  auto m = tg::rotate<double>(M_PI_2, tg::vec3(1, 0, 0));

  for (auto &node : _m->nodes) {
    tg::mat4d t, r, s; t.identity();
    if (!node.translation.empty()) t = tg::translate(tg::vec3d(node.translation.data())); 
    r.identity(); if(!node.rotation.empty()) r = tg::mat4d(tg::dmat3(tg::quatd(tg::vec4d(node.rotation.data()))));
    s.identity(); if(!node.scale.empty()) s = tg::scale(tg::vec3d(node.scale.data()));

    auto mesh = _m->meshes[node.mesh];
    for (auto &pri : mesh.primitives) {
      auto mesh_pri = create_primitive(&pri);
      auto dm = m * t *s * r;
      mesh_pri->set_transform(tg::mat4(dm));

      mesh_pri->set_material(materials[pri.material]);

      meshInst->add_primitive(mesh_pri);
    }
  }

  return meshInst;
}

std::shared_ptr<MeshPrimitive>
GLTFLoader::create_primitive(const tinygltf::Primitive *pri)
{
  auto mesh_pri = std::make_shared<MeshPrimitive>();

  for (auto &attr : pri->attributes) {
    VkVertexInputBindingDescription vkinput;
    VkVertexInputAttributeDescription vkattr;
    auto &acc = _m->accessors[attr.second];
    auto &bufview = _m->bufferViews[acc.bufferView];
    auto &buf = _m->buffers[bufview.buffer];
    if (attr.first.compare("POSITION") == 0) {
      vkattr.location = 0;
      vkattr.binding = 0;
      mesh_pri->set_vertex(buf.data.data() + bufview.byteOffset, bufview.byteLength);
    } else if (attr.first.compare("NORMAL") == 0) {
      vkattr.location = 1;
      vkattr.binding = 1;
      mesh_pri->set_normal(buf.data.data() + bufview.byteOffset, bufview.byteLength);
    } else if (attr.first.compare("TEXCOORD_0") == 0) {
      vkattr.location = 2;
      vkattr.binding = 2;
      mesh_pri->set_uvs(buf.data.data() + bufview.byteOffset, bufview.byteLength);
    } else if (attr.first.compare("TEXCOORD_1") == 1) {
      vkattr.location = 3;
      vkattr.binding = 3;
    } else
      continue;

    //vkattr.format = attr_format(&acc);
    //vkattr.offset = 0;
    //vkinput.binding = vkattr.binding;
    //vkinput.stride = acc.ByteStride(_m->bufferViews[acc.bufferView]);
    //vkinput.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  }
  //if (pri->mode == TINYGLTF_MODE_LINE_LOOP)
  //else if (pri->mode == TINYGLTF_MODE_TRIANGLES)
  //else return nullptr;

  if (pri->indices >= 0) {
    auto &idx_acc = _m->accessors[pri->indices];
    auto ty = idx_acc.componentType;
    auto &bufview = _m->bufferViews[idx_acc.bufferView];
    auto &buf = _m->buffers[bufview.buffer];

    if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
      mesh_pri->_index_type = VK_INDEX_TYPE_UINT16;
      mesh_pri->set_index(buf.data.data() + bufview.byteOffset, bufview.byteLength);
    }
    else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
      mesh_pri->_index_type = VK_INDEX_TYPE_UINT32;
    else
      return nullptr;

  }

  return mesh_pri; 
}

VkFormat GLTFLoader::attr_format(const tinygltf::Accessor *acc)
{
  switch (acc->componentType) {
    case TINYGLTF_COMPONENT_TYPE_FLOAT: {
      if (acc->type == TINYGLTF_TYPE_VEC2) {
        return VK_FORMAT_R32G32_SFLOAT;
      } else if (acc->type == TINYGLTF_TYPE_VEC3) {
        return VK_FORMAT_R32G32B32_SFLOAT;
      } else if (acc->type == TINYGLTF_TYPE_VEC4) {
        return VK_FORMAT_R32G32B32A32_SFLOAT;
      }
      break;
    }
    case TINYGLTF_COMPONENT_TYPE_INT: {
      if (acc->type == TINYGLTF_TYPE_VEC2) {
        return VK_FORMAT_R32G32_SINT;
      } else if (acc->type == TINYGLTF_TYPE_VEC3) {
        return VK_FORMAT_R32G32B32_SINT;
      } else if (acc->type == TINYGLTF_TYPE_VEC4) {
        return VK_FORMAT_R32G32B32A32_SINT;
      }
      break;
    }
    case TINYGLTF_COMPONENT_TYPE_SHORT:
      if (acc->type == TINYGLTF_TYPE_VEC2) {
        return VK_FORMAT_R16G16_SINT;
      } else if (acc->type == TINYGLTF_TYPE_VEC3) {
        return VK_FORMAT_R16G16B16_SINT;
      } else if (acc->type == TINYGLTF_TYPE_VEC4) {
        return VK_FORMAT_R16G16B16A16_SINT;
      }
      break;
  }
}
