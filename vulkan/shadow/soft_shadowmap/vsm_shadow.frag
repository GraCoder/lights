#version 450

layout(location = 0) in vec3 vp_pos;
layout(location = 1) in vec3 vp_norm;
layout(location = 2) in vec2 vp_uv;
layout(location = 3) in vec3 vp_suv;

layout(location = 0) out vec4 frag_color;

layout(binding = 0) uniform MatrixObject
{
  vec4 eye;
  mat4 proj;
  mat4 view;
} mvp;

layout(set = 1, binding = 0) uniform ParallelLight
{
  vec4 light_dir;
  vec4 light_color;
} light;

layout(set = 2, binding = 0) uniform sampler2D tex;

layout(set = 3, binding = 0) uniform ShadowMatrix
{
  vec4 light;
  mat4 proj;
  mat4 view;
  mat4 mvp;
  mat4 pers;
  vec4 options;
} shadow_matrix;

layout(set = 3, binding = 1) uniform sampler2D shadow_tex;

// =============================================================================
// VSM 阴影采样
// 阴影贴图的 R/G 通道分别保存深度的一阶矩 E[x] 和二阶矩 E[x^2]。
// 返回值 visibility 位于 [0, 1]：0 表示完全处于阴影中，1 表示完全可见。
// =============================================================================

// 将低于阈值的可见度重新映射到 0，用于减轻 VSM 常见的漏光现象。
float reduce_light_bleeding(float p, float amount)
{
  return clamp((p - amount) / (1.0 - amount), 0.0, 1.0);
}

// 读取深度矩。light.w 为过滤开关：关闭时直接采样，开启时执行 5x5 高斯模糊。
vec2 sample_moments(vec2 uv)
{
  if (shadow_matrix.light.w < 0.5)
    return texture(shadow_tex, uv).rg;

  // 可分离二项式权重 [1, 4, 6, 4, 1] 的二维组合，总权重为 256。
  vec2 texel = 1.0 / vec2(textureSize(shadow_tex, 0));
  float weights[5] = float[](1.0, 4.0, 6.0, 4.0, 1.0);
  vec2 moments = vec2(0.0);
  for (int y = -2; y <= 2; ++y) {
    for (int x = -2; x <= 2; ++x) {
      float weight = weights[x + 2] * weights[y + 2];
      moments += texture(shadow_tex, uv + vec2(x, y) * texel).rg * weight;
    }
  }
  return moments * (1.0 / 256.0);
}

// 使用 Chebyshev 上界，根据深度矩估算接收点被光源看见的概率。
float vsm_visibility(vec2 uv, float receiver_depth)
{
  vec2 moments = sample_moments(uv);

  // 接收点位于平均遮挡深度之前时，可直接判定为完全可见。
  if (receiver_depth <= moments.x)
    return 1.0;

  // Var(x) = E[x^2] - E[x]^2；最小方差可避免数值不稳定和过硬边缘。
  float variance = max(moments.y - moments.x * moments.x, shadow_matrix.options.x);
  float distance_to_mean = receiver_depth - moments.x;
  float p_max = variance / (variance + distance_to_mean * distance_to_mean);

  // options.y 控制漏光抑制强度。
  return reduce_light_bleeding(p_max, shadow_matrix.options.y);
}

void main(void)
{
  // =============================================================================
  // 材质采样与普通光照部分
  // =============================================================================

  // 读取基础颜色；完全透明的纹理片元不参与后续光照与阴影计算。
  vec4 base_color = texture(tex, vp_uv);
  if (base_color.a == 0.0)
    discard;

  // 构造 Blinn/Phong 风格光照所需的法线、视线方向和光线方向。
  vec3 n = normalize(vp_norm);
  vec3 v = normalize(mvp.eye.xyz - vp_pos);
  vec3 l = normalize(light.light_dir.xyz);
  vec3 radiance = light.light_color.rgb;

  // 漫反射强度由 N dot L 决定；镜面反射使用反射向量和 32 次高光指数。
  float ndotl = max(dot(n, l), 0.0);
  vec3 reflected = reflect(-l, n);
  float specular_factor = 0.0;
  if (ndotl > 0.0)
    specular_factor = pow(max(dot(v, reflected), 0.0), 32.0);

  // =============================================================================
  // 阴影部分：光空间坐标转换与 VSM 可见度计算
  // =============================================================================

  // 默认位于阴影贴图覆盖范围之外的片元完全受光照。
  float visibility = 1.0;

  // vp_suv 已是透视除法后的光源 NDC 坐标，只处理有效裁剪范围内的片元。
  if (vp_suv.x > -1.0 && vp_suv.x < 1.0 &&
      vp_suv.y > -1.0 && vp_suv.y < 1.0 &&
      vp_suv.z >= 0.0 && vp_suv.z <= 1.0) {
    // 将 NDC 的 xy 从 [-1, 1] 转换到纹理坐标 [0, 1]，并翻转 Y 轴。
    vec2 suv = (vp_suv.xy + vec2(1.0)) * 0.5;
    suv.y = 1.0 - suv.y;
    visibility = vsm_visibility(suv, vp_suv.z);
  }

  // =============================================================================
  // 普通光照与阴影合成
  // 环境光始终保留，阴影可见度只衰减直接光的漫反射和镜面反射。
  // =============================================================================

  vec3 ambient = base_color.rgb * 0.05;
  vec3 diffuse = base_color.rgb * radiance * ndotl;
  vec3 specular = radiance * 0.2 * specular_factor;
  vec3 color = ambient + visibility * (diffuse + specular);

  // Reinhard 色调映射，将 HDR 光照结果压缩到可显示范围。
  color = color / (color + vec3(1.0));
  frag_color = vec4(color, 1.0);
}
