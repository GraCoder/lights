#version 450

layout(location = 0) in vec3 vp_pos;
layout(location = 1) in vec3 vp_norm;
layout(location = 2) in vec2 vp_uv;

layout(location = 0) out vec4 frag_color;

layout(set = 0, binding = 0) uniform MatrixObject
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

layout(set = 2, binding = 0) uniform Material
{
  float ao;
  float metallic;
  float roughness;
  vec4 albedo;
} material;

layout(set = 3, binding = 0) uniform sampler2D tex;

layout(set = 4, binding = 0) uniform ShadowMatrix{
  vec4 light;
  mat4 proj;
  mat4 view;
  mat4 mvp;
  mat4 pers;
} shadow_matrix;

layout(set = 5, binding = 0) uniform sampler2D shadow_tex;

const float pi = 3.14159265359;

vec3 fresnel_schlick(float cosTheta, vec3 f0)
{
  return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

float distribution_GGX(vec3 n, vec3 h, float roughness)
{
  float a = roughness * roughness;
  float a2 = a * a;
  float ndot_h = max(dot(n, h), 0.0);
  float ndot_h2 = ndot_h * ndot_h;

  float denom = ndot_h2 * (a2 - 1.0) + 1.0;
  denom = pi * denom * denom;

  return a2 / max(denom, 0.0000001);
}

float schlick_GGX(float ndotv, float roughness)
{
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float denom = ndotv * (1.0 - k) + k;

  return ndotv / denom;
}

float smith_Geometry(vec3 n, vec3 v, vec3 l, float roughness)
{
  float ndotv = max(dot(n, v), 0.0);
  float ndotl = max(dot(n, l), 0.0);
  float ggx2 = schlick_GGX(ndotv, roughness);
  float ggx1 = schlick_GGX(ndotl, roughness);

  return ggx1 * ggx2;
}

void main(void)
{
  frag_color = texture(tex, vp_uv);
  if (frag_color.a == 0.f)
    discard;

  vec3 mate_albedo = frag_color.rgb;
  float mate_roughness = material.roughness;
  float mate_metallic = material.metallic;
  float mate_ao = material.ao;

  vec3 eye = mvp.eye.xyz;
  vec3 n = normalize(vp_norm);
  vec3 v = normalize(eye - vp_pos);

  vec3 f0 = vec3(0.04);
  f0 = mix(f0, mate_albedo, mate_metallic);
  vec3 lo = vec3(0.0);

  vec3 l = normalize(light.light_dir.xyz);
  vec3 h = normalize(v + l);
  vec3 radiance = light.light_color.rgb;

  float nv = distribution_GGX(n, h, mate_roughness);
  float gv = smith_Geometry(n, v, l, mate_roughness);
  vec3 fv = fresnel_schlick(clamp(dot(h, v), 0.0, 1.0), f0);

  vec3 nominator = nv * gv * fv;
  float denominator = 4 * max(dot(n, v), 0) * max(dot(n, l), 0.0);
  vec3 specular = nominator / max(denominator, 0.000001);

  vec3 ks = fv;
  vec3 kd = vec3(1.0) - ks;
  kd *= (1.0 - mate_metallic);

  float ndotl = max(dot(n, l), 0);

  lo += (kd * mate_albedo / pi + specular) * radiance * ndotl;

  // 默认阴影投影范围外的片元完全可见。
  float visibility = 1.0;

  // 只保留固定 receiver bias。shadow_matrix.light.xyz 的语义是从场景指向
  // 光源，因此沿该方向稍微移动接收点，可以减轻表面自遮挡。
  const float shadow_bias = 0.001;
  vec3 receiver_pos = vp_pos + normalize(shadow_matrix.light.xyz) * shadow_bias;

  // 深度生成阶段采用：world -> PSM warp -> 除以 warp.w -> light VP。
  // 采样阶段必须使用完全相同的变换顺序。
  vec4 warped = shadow_matrix.pers * vec4(receiver_pos, 1.0);
  if (warped.w > 0.000001)
  {
    warped.xyz /= warped.w;
    warped.w = 1.0;

    vec4 light_clip = shadow_matrix.mvp * warped;
    if (light_clip.w > 0.000001)
    {
      vec3 shadow_ndc = light_clip.xyz / light_clip.w;

      // 只对实际落在阴影贴图覆盖范围内的接收点采样。这样即使 sampler
      // 使用 REPEAT，越界坐标也不会从阴影贴图另一侧产生虚假阴影。
      if (shadow_ndc.x > -1.0 && shadow_ndc.x < 1.0 &&
          shadow_ndc.y > -1.0 && shadow_ndc.y < 1.0 &&
          shadow_ndc.z >= 0.0 && shadow_ndc.z <= 1.0)
      {
        vec2 suv = (shadow_ndc.xy + vec2(1.0)) * 0.5;
        suv.y = 1.0 - suv.y;

        // 普通 sampler2D 使用 linear 深度采样，随后执行一次硬阴影比较。
        float closest_depth = texture(shadow_tex, suv).r;
        visibility = shadow_ndc.z <= closest_depth ? 1.0 : 0.0;
      }
    }
  }

  // 阴影只衰减方向光产生的直接光照，环境光始终保留；先完成 HDR 光照
  // 合成，再执行非线性的 tone mapping。
  vec3 ambient = vec3(0.03) * mate_albedo * mate_ao;
  vec3 color = ambient + visibility * lo;

  color = color / (color + vec3(1.0));
  //color = pow(color, vec3(1.0 / 2.2));

  frag_color = vec4(color, 1.0);
}
