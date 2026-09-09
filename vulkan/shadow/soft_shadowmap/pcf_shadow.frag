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

layout(set = 3, binding = 0) uniform ShadowMatrix{
  vec4 light;
  mat4 proj;
  mat4 view;
  mat4 mvp;
  mat4 pers;
  vec4 options;
} shadow_matrix;

layout(set = 3, binding = 1) uniform sampler2DShadow shadow_tex;

// =============================================================================
// PCF 阴影采样
// 返回值 visibility 位于 [0, 1]：0 表示完全处于阴影中，1 表示完全可见。
// sampler2DShadow 会将传入的 depth 与阴影贴图深度进行硬件深度比较。
// =============================================================================

#define SHADOW_SAMPLING_PCF_HARD 0
#define SHADOW_SAMPLING_PCF_LOW 1

// 在阴影贴图的 uv 处执行一次深度比较。
float shadow_visible(vec2 uv, float depth)
{
  return texture(shadow_tex, vec3(uv, depth));
}

// 单次深度比较，不额外过滤，因此阴影边缘较硬。
float ShadowSample_PCF_Hard(vec2 suv, float depth)
{
  return shadow_visible(suv, depth);
}

// 4 次采样的高斯近似，移植自 Filament 的 ShadowSample_PCF_Low。
// 利用双线性过滤将周围多个深度比较结果合并为较平滑的阴影可见度。
float ShadowSample_PCF_Low(vec2 suv, float depth)
{
  // 将归一化纹理坐标转换到 texel 空间，计算当前点在 texel 内的位置。
  vec2 size = vec2(textureSize(shadow_tex, 0));
  vec2 texelSize = vec2(1.0) / size;

  vec2 offset = vec2(0.5);
  vec2 uv = (suv * size) + offset;
  vec2 base = (floor(uv) - offset) * texelSize;
  vec2 st = fract(uv);

  // 根据小数位置计算 4 个双线性采样点及对应的高斯近似权重。
  vec2 uw = vec2(3.0 - 2.0 * st.x, 1.0 + 2.0 * st.x);
  vec2 vw = vec2(3.0 - 2.0 * st.y, 1.0 + 2.0 * st.y);

  vec2 u = vec2((2.0 - st.x) / uw.x - 1.0, st.x / uw.y + 1.0);
  vec2 v = vec2((2.0 - st.y) / vw.x - 1.0, st.y / vw.y + 1.0);

  u *= texelSize.x;
  v *= texelSize.y;

  float w0 = uw.x * vw.x;
  float w1 = uw.y * vw.x;
  float w2 = uw.x * vw.y;
  float w3 = uw.y * vw.y;

  vec2 uv0 = base + vec2(u.x, v.x);
  vec2 uv1 = base + vec2(u.y, v.x);
  vec2 uv2 = base + vec2(u.x, v.y);
  vec2 uv3 = base + vec2(u.y, v.y);

  // 防止过滤采样越过阴影贴图边界。
  uv0 = clamp(uv0, vec2(0.0), vec2(1.0));
  uv1 = clamp(uv1, vec2(0.0), vec2(1.0));
  uv2 = clamp(uv2, vec2(0.0), vec2(1.0));
  uv3 = clamp(uv3, vec2(0.0), vec2(1.0));

  // 对 4 次深度比较结果加权求和，并将权重归一化。
  float sum = 0.0;
  sum += w0 * shadow_visible(uv0, depth);
  sum += w1 * shadow_visible(uv1, depth);
  sum += w2 * shadow_visible(uv2, depth);
  sum += w3 * shadow_visible(uv3, depth);

  return sum * 0.0625;
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
  // 阴影部分：接收点偏移、光空间投影与 PCF 可见度计算
  // =============================================================================

  // 默认位于阴影贴图覆盖范围之外的片元完全受光照。
  float visibility = 1.0;

  // 沿光线方向和表面法线偏移接收点，减轻 shadow acne。
  // 掠射角越大，法线方向偏移越大。
  vec3 shadow_light = normalize(shadow_matrix.light.xyz);
  float shadow_nol = clamp(dot(n, shadow_light), 0.0, 1.0);
  float normal_offset = shadow_matrix.options.y * shadow_matrix.options.z *
      sqrt(max(1.0 - shadow_nol * shadow_nol, 0.0));
  vec3 receiver_pos = vp_pos + shadow_light * shadow_matrix.options.x + n * normal_offset;
  vec4 biased_suv = shadow_matrix.mvp * vec4(receiver_pos, 1.0);
  biased_suv /= biased_suv.w;

  // 仅对位于光源裁剪空间/阴影贴图覆盖范围内的片元采样阴影。
  if (biased_suv.x > -1.0 && biased_suv.x < 1.0 &&
      biased_suv.y > -1.0 && biased_suv.y < 1.0 &&
      biased_suv.z >= 0.0 && biased_suv.z <= 1.0)
  {
    // 将 NDC 的 xy 从 [-1, 1] 转换到纹理坐标 [0, 1]，并翻转 Y 轴。
    vec2 suv = biased_suv.xy;
    suv = (suv + vec2(1.0)) * 0.5;
    suv.y = 1.0 - suv.y;

    // light.w 用作过滤模式开关：启用时使用软 PCF，否则执行硬阴影比较。
    if (shadow_matrix.light.w >= 0.5)
      visibility = ShadowSample_PCF_Low(suv, biased_suv.z);
    else
      visibility = ShadowSample_PCF_Hard(suv, biased_suv.z);
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
