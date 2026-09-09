#version 450

layout(location = 0) in vec3 vp_pos;
layout(location = 1) in vec3 vp_norm;
layout(location = 2) in vec2 vp_uv;
layout(location = 3) in vec3 vp_suv;

layout(location = 0) out vec4 frag_color;

// Basic shadow map 只显示纹理本色和硬阴影，不包含 PBR 或 Phong 光照。
layout(set = 1, binding = 0) uniform sampler2D tex;

layout(set = 2, binding = 0) uniform ShadowMatrix
{
  vec4 light;
  mat4 proj;
  mat4 view;
  mat4 mvp;
} shadow_matrix;

// 使用普通 sampler2D 读取深度。VulkanTexture 的采样器使用 linear filter，
// 因此这里会先对相邻 texel 的深度值进行线性插值，再做一次硬阈值比较。
layout(set = 2, binding = 1) uniform sampler2D shadow_tex;

const float shadow_bias = 0.001;
const float shadow_darkness = 0.2;

void main(void)
{
  vec4 base_color = texture(tex, vp_uv);
  if (base_color.a == 0.0)
    discard;

  // 默认认为阴影贴图覆盖范围外的片元可见。
  float visibility = 1.0;

  // 只保留固定 receiver bias：将世界空间接收点沿指向光源的方向移动
  // 0.001 个世界单位。这里不再使用基于法线角度的 slope/normal bias。
  vec3 shadow_light = normalize(shadow_matrix.light.xyz);
  vec3 receiver_pos = vp_pos + shadow_light * shadow_bias;

  // 将偏移后的世界空间位置重新投影到光源 NDC 空间。
  vec4 biased_suv = shadow_matrix.mvp * vec4(receiver_pos, 1.0);
  biased_suv /= biased_suv.w;

  // Soft shadow map 示例使用 Vulkan 深度范围 [0, 1]，因此 XYZ 都必须位于
  // 有效光源裁剪范围内；范围外保持 visibility = 1.0。
  if (biased_suv.x > -1.0 && biased_suv.x < 1.0 &&
      biased_suv.y > -1.0 && biased_suv.y < 1.0 &&
      biased_suv.z >= 0.0 && biased_suv.z <= 1.0)
  {
    vec2 suv = (biased_suv.xy + vec2(1.0)) * 0.5;
    suv.y = 1.0 - suv.y;

    float closest_depth = texture(shadow_tex, suv).r;
    visibility = biased_suv.z <= closest_depth ? 1.0 : 0.0;
  }

  // 不计算任何光照项。可见区域显示纹理本色，阴影区域保留固定的 20% 亮度，
  // 便于观察模型轮廓；该常量只是阴影显示强度，不是 PBR/Phong 环境光。
  float shadow_factor = mix(shadow_darkness, 1.0, visibility);
  frag_color = vec4(base_color.rgb * shadow_factor, base_color.a);
}
