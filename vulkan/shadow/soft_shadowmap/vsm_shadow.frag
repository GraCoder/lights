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

float reduce_light_bleeding(float p, float amount)
{
  return clamp((p - amount) / (1.0 - amount), 0.0, 1.0);
}

vec2 sample_moments(vec2 uv)
{
  if (shadow_matrix.light.w < 0.5)
    return texture(shadow_tex, uv).rg;

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

float vsm_visibility(vec2 uv, float receiver_depth)
{
  vec2 moments = sample_moments(uv);
  if (receiver_depth <= moments.x)
    return 1.0;

  float variance = max(moments.y - moments.x * moments.x, shadow_matrix.options.x);
  float distance_to_mean = receiver_depth - moments.x;
  float p_max = variance / (variance + distance_to_mean * distance_to_mean);
  return reduce_light_bleeding(p_max, shadow_matrix.options.y);
}

void main(void)
{
  vec4 base_color = texture(tex, vp_uv);
  if (base_color.a == 0.0)
    discard;

  vec3 n = normalize(vp_norm);
  vec3 v = normalize(mvp.eye.xyz - vp_pos);
  vec3 l = normalize(light.light_dir.xyz);
  vec3 radiance = light.light_color.rgb;

  float ndotl = max(dot(n, l), 0.0);
  vec3 reflected = reflect(-l, n);
  float specular_factor = 0.0;
  if (ndotl > 0.0)
    specular_factor = pow(max(dot(v, reflected), 0.0), 32.0);

  float visibility = 1.0;
  if (vp_suv.x > -1.0 && vp_suv.x < 1.0 &&
      vp_suv.y > -1.0 && vp_suv.y < 1.0 &&
      vp_suv.z >= 0.0 && vp_suv.z <= 1.0) {
    vec2 suv = (vp_suv.xy + vec2(1.0)) * 0.5;
    suv.y = 1.0 - suv.y;
    visibility = vsm_visibility(suv, vp_suv.z);
  }

  vec3 ambient = base_color.rgb * 0.05;
  vec3 diffuse = base_color.rgb * radiance * ndotl;
  vec3 specular = radiance * 0.2 * specular_factor;
  vec3 color = ambient + visibility * (diffuse + specular);
  color = color / (color + vec3(1.0));
  frag_color = vec4(color, 1.0);
}
