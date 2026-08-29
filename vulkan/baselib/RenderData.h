#pragma once

#include "tvec.h"
#include "VulkanTexture.h"

struct MVP {
  tg::vec4 eye;
  tg::mat4 prj;
  tg::mat4 view;
};

struct Transform{
  tg::mat4 m;
};

struct ParallelLight{
  tg::vec4 lightDir;
  tg::vec4 lightColor;
};

struct PointLight{
  tg::vec4 lightPos;
  tg::vec4 lightColor;
};

struct alignas(64) PBRBase {
  float   ao;
  float   metallic;
  float   roughness;
  int     place;
  tg::vec4 albedo;
};

struct Material{
  bool          cull;
  PBRBase       pbrdata;
  std::shared_ptr<VulkanTexture> albedoTex;
};

struct ShadowMatrix {
  tg::vec4 light;
  tg::mat4 prj;
  tg::mat4 view;
  tg::mat4 mvp;
  tg::mat4 pers;
  tg::vec4 options;
};
